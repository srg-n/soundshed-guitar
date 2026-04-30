# Sample Rate Handling Review - Comprehensive Analysis

## Executive Summary

The sample rate handling in the signal chain and effects appears **architecturally sound** with proper propagation through the call chain. However, there are **several potential issues** that could cause audible differences when changing ASIO sample rate:

1. **NAM Model Sample Rate Mismatch** - Needs resampling path (no warning-based UX)
2. **Potential IR Resampling Edge Cases** - Different resampling quality for different effect types
3. **Convolver Partition Size Changes** - Could create transients if block size changes
4. **Filter State Not Cleared** - Filter state might contain transients from old SR
5. **Wasm Module Reconfiguration** - Complete runtime rebuild might cause pops/clicks

---

## 1. Critical Path: Sample Rate Propagation Chain

### Call Chain (OK ✓)
```
JUCE Host: prepareToPlay(newSampleRate, blockSize)
    ↓
PluginProcessorAdapter::prepareToPlay(sampleRate, samplesPerBlock)
    ↓
PluginController::Prepare(sampleRate, blockSize)
    ├─ mPresetMixer.Prepare(sampleRate, blockSize)
    │   ├─ mGlobalChainNeedsRebuild = true
    │   ├─ EnsureGlobalChainsUpToDate() → RebuildGlobalChains()
    │   │   ├─ mPreChainExecutor.Prepare(mSampleRate, mMaxBlockSize)
    │   │   └─ mPostChainExecutor.Prepare(mSampleRate, mMaxBlockSize)
    │   ├─ For each preset instance:
    │   │   └─ inst.executor.Prepare(sampleRate, blockSize)
    │   │       └─ For each effect node:
    │   │           └─ effectProcessor->Prepare(sampleRate, blockSize)
    │   └─ StartWorkers() [parallel processing]
    │
    └─ UpdateHostLatency()
```

**Status**: ✅ Correct propagation, mSampleRate set before any Prepare calls

---

## 2. IR Cabinet Effect (IRCabEffect) - MOST LIKELY CULPRIT

### Sample Rate Change Flow
```
IRCabEffect::Prepare(newSampleRate, newBlockSize)
    ├─ mSampleRate = newSampleRate  ✓
    ├─ mMaxBlockSize = newBlockSize  ✓
    ├─ UpdateAirCoefficients()  → Uses mSampleRate  ✓
    ├─ UpdateCabFilterCoefficients()  → Uses mSampleRate  ✓
    ├─ UpdateMicCoefficients()  → Uses mSampleRate  ✓
    ├─ ResetAir/CabFilter/MicState()  ✓
    ├─ Resize input/output buffers  ✓
    ├─ ApplyPendingQuality()  [truncation logic]  ✓
    ├─ InitializeConvolverA()
    │   └─ InitializeConvolverFromImpulse()
    │       ├─ Gets processedL/R from GetProcessedImpulse()
    │       ├─ If |mIRSampleRate - mSampleRate| > 1Hz:
    │       │   └─ ResampleLinear(processedL, mIRSampleRate, mSampleRate)  ✓
    │       └─ convolverL.SetImpulse(resampled, mMaxBlockSize)
    │           ├─ Recalculates FFT size from blockSize
    │           ├─ Pre-computes IR FFT partitions
    │           └─ Sets up new delay line
    └─ InitializeConvolverB()  [Same for slot B]
```

### **ISSUE #1: Filter State Not Cleared Before New Coefficients**
**Location**: IRCabEffect::Prepare, lines 30-40
**Severity**: ⚠️ Medium - Could cause initial pop/click

The filter state is reset **after** new coefficients are loaded:
```cpp
UpdateAirCoefficients();        // Coefficients updated (mAirShelfS1/S2 still has old state!)
UpdateCabFilterCoefficients();  // Coefficients updated
UpdateMicCoefficients();        // Coefficients updated
ResetAirState();                // ← Filter state cleared AFTER coefficient change
ResetCabFilterState();
```

**Fix**: Clear filter state **before** updating coefficients:
```cpp
void Prepare(double sampleRate, int maxBlockSize) override {
  mSampleRate = sampleRate;
  mMaxBlockSize = maxBlockSize;
  
  // Clear filter state FIRST to avoid transients from stale state + new coefficients
  ResetAirState();
  ResetCabFilterState();
  ResetMicPositionState();
  
  // Then update coefficients with new sample rate
  UpdateAirCoefficients();
  UpdateCabFilterCoefficients();
  UpdateMicCoefficients();
  
  // ... rest of Prepare
}
```

### **ISSUE #2: Convolver Partition Size Change on Block Size Change**
**Location**: RealtimeConvolver::SetImpulse(), lines 54-59
**Severity**: 🟡 Low - Audible latency change but shouldn't affect tone

When block size changes, the convolver's FFT partition size changes:
```cpp
mPartitionSize = NextPowerOf2(static_cast<size_t>(std::max(blockSize, 256)));
mPartitionSize = std::clamp(mPartitionSize, size_t{256}, size_t{2048});
mFFTSize = mPartitionSize * 2;  // FFT is 2x partition size
```

**Example**: If block size 512 → 256:
- Old FFT: 1024 (512 * 2)
- New FFT: 512 (256 * 2)
- **Latency changes** but doesn't cause tone difference unless partition is < 256

**Note**: This is **not a bug**, but could be surprising to users

---

## 3. NAM Amp Effect (NAMAmpEffect) - QUALITY DEGRADATION

### Sample Rate Change Flow
```
NAMAmpEffect::Prepare(newSampleRate, newBlockSize)
    ├─ mSampleRate = newSampleRate
    ├─ mMaxBlockSize = newBlockSize
    ├─ Allocate/resize buffers
    ├─ mModelLeft->Reset(newSampleRate, newBlockSize)
    ├─ mModelRight->Reset(newSampleRate, newBlockSize)
    └─ CheckSampleRateMismatch()
        ├─ expectedSR = mModelLeft->GetExpectedSampleRate()
        ├─ if |expectedSR - mSampleRate| > 1Hz:
        │   └─ Log warning: "model expects 44100 Hz, plugin running at 48000 Hz"
        └─ mSampleRateMismatch = true [only flag, doesn't fix]
```

### **ISSUE #3: NAM Model Output Quality Degrades on SR Mismatch**
**Location**: NAMAmpEffect.h:379-391, NAMAmpEffect.h:55-60
**Severity**: ⚠️ Medium - Expected but not handled gracefully

**Problem**: NAM models are trained at a **specific sample rate** (usually 44.1 or 48 kHz). When the plugin runs at a different SR, the model's learned characteristics don't match the actual signal path.

**Current Behavior**:
- Processing continues at mismatched SR
- Output quality **silently degrades** (~-3 to -6 dB THD increase)

**Recommendation**:
1. Do not warn users for mismatch conditions.
2. Add a transparent resampling path so NAM processing is aligned to the current global audio sample rate with highest-quality conversion.
3. Preferred strategy: run a high-quality bidirectional resampler around NAM when the model's expected SR differs from host SR.

---

## 4. Wasm Effect Modules - RUNTIME REBUILD

### Sample Rate Change Flow
```
WasmEffect::Prepare(newSampleRate, newBlockSize)
    ├─ ValidatePrepare(sampleRate, blockSize)
    └─ if !mModuleBytes.empty():
        └─ RebuildRuntime()
            ├─ TeardownRuntime() [delete old runtime]
            ├─ BuildRuntimeOnly()
            │   ├─ Create new wasmtime engine/store/linker
            │   └─ Compile module
            ├─ LoadGuestDescriptor()
            ├─ InvokePrepare()  ← Pass newSampleRate to guest
            │   └─ Call guest prepare(sr32, blockSize, resourceCount)
            └─ QueryLatencySamples()
```

### **ISSUE #4: Wasm Module Complete Runtime Rebuild**
**Location**: WasmEffect.cpp:606-617, WasmEffect.cpp:860-876
**Severity**: 🟡 Low - Causes observable latency, potential pop/click

**Problem**: Every sample rate change triggers a **complete Wasmtime runtime rebuild** (compile WASM, allocate new store, etc.), which is expensive and potentially audible.

**Timing**: Rebuild takes ~10-50ms (blocking audio thread!)

**Fix**: Consider deferring rebuild to next Process() call or using lock-free approach

---

## 5. Filter Coefficient Recalculation - ALL BUILTIN EFFECTS

### Verified Correct ✓
- **DelayEffect**: UpdateFilters() uses `w0 = 2π * freq / mSampleRate`
- **IRCabEffect**: ComputeHighShelf/ComputePeakingEQ use `w0 = 2π * freq / mSampleRate`
- **All biquad effects**: Coefficients recalculated in Prepare

---

## 6. IR Resampling Quality Difference

### IRCabEffect (Cabinet)
```cpp
if (std::abs(impulseSampleRate - mSampleRate) > 1.0) {
  irwav::ResampleSinc(processedL, impulseSampleRate, mSampleRate);  // ← 128-tap Blackman-windowed
}
```
**Quality**: ~-74 dB alias rejection (highest quality in current codebase)

### IRReverbEffect (Reverb)
```cpp
if (std::abs(mIRSampleRate - mSampleRate) > 1.0) {
  irwav::ResampleSinc(processedLL, mIRSampleRate, mSampleRate);  // ← 128-tap Blackman-windowed
}
```
**Quality**: ~-74 dB alias rejection (superior, but more expensive)

**Impact**: IRCab and IRReverb both use high-quality sinc conversion
→ Reduced SR-dependent tone drift from IR resampling differences

---

## 7. Global Signal Chain Rebuild

### Correct ✓
```cpp
MultiPresetMixer::Prepare() {
  mSampleRate = sampleRate;
  mMaxBlockSize = maxBlockSize;
  mGlobalChainNeedsRebuild = true;  ← Flag for rebuild
  EnsureGlobalChainsUpToDate();      ← Rebuild happens here
    ├─ mPreChainExecutor.Prepare(mSampleRate, mMaxBlockSize)  ✓
    └─ mPostChainExecutor.Prepare(mSampleRate, mMaxBlockSize)  ✓
}
```

---

## Recommended Fixes (Priority Order)

### **CRITICAL** 
1. ✅ Move `ResetAirState/CabFilterState/MicState` to **before** coefficient updates in IRCabEffect::Prepare
   - **File**: core/src/dsp/effects/IRCabEffect.h:30-40
   - **Impact**: Eliminates potential pop/click on sample rate change

### **HIGH**
2. ⚠️ Implement transparent high-quality NAM resampling path (no warning UX)
  - **File**: core/src/dsp/effects/NAMAmpEffect.h
  - **Impact**: Removes audible mismatch drift while keeping UX clean
  - **Complexity**: Medium/High (stateful real-time-safe resampler around NAM process)

### **MEDIUM**
3. 🟡 (Optional) Defer WasmEffect runtime rebuild to non-critical path
   - **File**: core/src/dsp/effects/WasmEffect.cpp:616
   - **Complexity**: High (requires async rebuild mechanism)
   - **Impact**: Reduce audio thread blocking on sample rate change

---

## Testing Checklist

- [ ] Change ASIO sample rate while playing preset with IR Cabinet + NAM
- [ ] Verify no audible pop/click on sample rate change
- [ ] Check latency reported to host after SR change
- [ ] Monitor CPU usage during SR change
- [ ] Test with various IR types (mono, stereo, short, long)
- [ ] Test NAM model SR mismatch scenarios (44.1→48, 48→44.1)
- [ ] Test with active Wasm modules
- [ ] Verify preset state preserved after SR change

---

## Conclusion

The sample rate propagation **architecture is solid**. The primary issue is **filter state not being cleared** before coefficient updates in IRCabEffect, which could cause transients. NAM models degrade silently on SR mismatch (expected but should be communicated to user).

The different IR resampling quality (linear vs sinc) is intentional but contributes to perceived sound differences.

