/**
 * @file IRConvolutionTests.cpp
 * @brief Unit tests for IR (Impulse Response) convolution algorithm
 *
 * Tests the NAMDSPManager::ApplyImpulseResponse function with known inputs and expected outputs
 * to verify the convolution implementation is mathematically correct.
 */

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "dsp/NAMDSPManager.h"

namespace
{
  constexpr double kEpsilon = 1e-6;
  constexpr double kSampleRate = 44100.0;
  constexpr int kBlockSize = 512;

  bool ApproxEqual(double a, double b, double epsilon = kEpsilon)
  {
    return std::abs(a - b) < epsilon;
  }

  /**
   * @brief Reference implementation of direct-form FIR convolution
   *
   * This is a simple, obviously-correct implementation for test comparison.
   * output[n] = sum(input[n-k] * impulse[k]) for k = 0 to IR_length-1
   */
  std::vector<double> ReferenceConvolve(const std::vector<double>& input,
                                        const std::vector<float>& impulse)
  {
    if (impulse.empty())
    {
      return input;
    }

    std::vector<double> output(input.size(), 0.0);
    const std::size_t irLength = impulse.size();

    for (std::size_t n = 0; n < input.size(); ++n)
    {
      double sum = 0.0;
      for (std::size_t k = 0; k < irLength; ++k)
      {
        // For sample index n-k, if it would be negative, use 0 (zero-padding)
        if (n >= k)
        {
          sum += input[n - k] * static_cast<double>(impulse[k]);
        }
      }
      output[n] = sum;
    }

    return output;
  }

  /**
   * @brief Helper class to test NAMDSPManager's IR convolution
   */
  class IRConvolutionTester
  {
  public:
    IRConvolutionTester()
    {
      mDSPManager = std::make_unique<namguitar::NAMDSPManager>();
      mDSPManager->Prepare(kSampleRate, kBlockSize);
    }

    void SetImpulse(const std::vector<float>& impulse)
    {
      mDSPManager->SetImpulseResponseForTest(impulse);
    }

    void Convolve(std::vector<double>& samples, int channel = 0)
    {
      mDSPManager->ApplyImpulseResponseForTest(samples, channel);
    }

    void Reset()
    {
      mDSPManager->Reset();
    }

  private:
    std::unique_ptr<namguitar::NAMDSPManager> mDSPManager;
  };

  // Test 1: Simple identity IR [1.0] should pass through unchanged
  bool TestIdentityIR()
  {
    std::cout << "Test: Identity IR [1.0]... ";

    std::vector<float> impulse = { 1.0f };
    std::vector<double> input = { 0.5, 1.0, -0.5, 0.25, 0.0 };
    std::vector<double> expected = { 0.5, 1.0, -0.5, 0.25, 0.0 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 2: Simple gain IR [0.5] should halve all samples
  bool TestGainIR()
  {
    std::cout << "Test: Gain IR [0.5]... ";

    std::vector<float> impulse = { 0.5f };
    std::vector<double> input = { 1.0, 2.0, -1.0, 0.5 };
    std::vector<double> expected = { 0.5, 1.0, -0.5, 0.25 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 3: Two-tap IR [0.5, 0.5] - averaging filter
  // output[n] = 0.5 * input[n] + 0.5 * input[n-1]
  bool TestTwoTapAverageIR()
  {
    std::cout << "Test: Two-tap averaging IR [0.5, 0.5]... ";

    std::vector<float> impulse = { 0.5f, 0.5f };
    std::vector<double> input = { 1.0, 0.0, 1.0, 0.0, 1.0 };

    // Expected: output[n] = 0.5 * input[n] + 0.5 * input[n-1]
    // n=0: 0.5*1.0 + 0.5*0 = 0.5  (input[-1] is 0 from zero-initialized buffer)
    // n=1: 0.5*0.0 + 0.5*1.0 = 0.5
    // n=2: 0.5*1.0 + 0.5*0.0 = 0.5
    // n=3: 0.5*0.0 + 0.5*1.0 = 0.5
    // n=4: 0.5*1.0 + 0.5*0.0 = 0.5
    std::vector<double> expected = { 0.5, 0.5, 0.5, 0.5, 0.5 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 4: Delay IR [0, 1] - one sample delay
  // output[n] = 0 * input[n] + 1 * input[n-1] = input[n-1]
  bool TestDelayIR()
  {
    std::cout << "Test: Delay IR [0, 1] (one sample delay)... ";

    std::vector<float> impulse = { 0.0f, 1.0f };
    std::vector<double> input = { 1.0, 2.0, 3.0, 4.0 };

    // Expected: output = input delayed by 1 sample (first sample is 0 from buffer init)
    // n=0: 0*1.0 + 1*0 = 0
    // n=1: 0*2.0 + 1*1.0 = 1
    // n=2: 0*3.0 + 1*2.0 = 2
    // n=3: 0*4.0 + 1*3.0 = 3
    std::vector<double> expected = { 0.0, 1.0, 2.0, 3.0 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 5: Three-tap IR with coefficients [1, 2, 3]
  // output[n] = 1 * input[n] + 2 * input[n-1] + 3 * input[n-2]
  bool TestThreeTapIR()
  {
    std::cout << "Test: Three-tap IR [1, 2, 3]... ";

    std::vector<float> impulse = { 1.0f, 2.0f, 3.0f };
    std::vector<double> input = { 1.0, 0.0, 0.0, 0.0 };

    // Impulse response to a unit impulse:
    // n=0: 1*1 + 2*0 + 3*0 = 1
    // n=1: 1*0 + 2*1 + 3*0 = 2
    // n=2: 1*0 + 2*0 + 3*1 = 3
    // n=3: 1*0 + 2*0 + 3*0 = 0
    std::vector<double> expected = { 1.0, 2.0, 3.0, 0.0 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 6: Compare NAMDSPManager with reference implementation
  bool TestAgainstReference()
  {
    std::cout << "Test: NAMDSPManager vs reference implementation... ";

    // Use a more complex IR and input
    std::vector<float> impulse = { 0.3f, 0.5f, 0.2f, -0.1f, 0.1f };
    std::vector<double> input = { 1.0, -0.5, 0.25, 0.75, -1.0, 0.5, 0.0, 0.25 };

    // Get reference result
    std::vector<double> referenceOutput = ReferenceConvolve(input, impulse);

    // Get NAMDSPManager result
    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> dspOutput = input;
    tester.Convolve(dspOutput);

    for (std::size_t i = 0; i < input.size(); ++i)
    {
      if (!ApproxEqual(dspOutput[i], referenceOutput[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (reference=" << referenceOutput[i]
                  << ", dsp=" << dspOutput[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 7: Multiple blocks - verify state is maintained across calls
  bool TestMultipleBlocks()
  {
    std::cout << "Test: Multiple blocks (state continuity)... ";

    std::vector<float> impulse = { 0.5f, 0.5f };

    // Process as two separate blocks
    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> block1 = { 1.0, 0.0 };
    std::vector<double> block2 = { 1.0, 0.0 };

    tester.Convolve(block1);
    tester.Convolve(block2);

    // Process as single block for comparison
    IRConvolutionTester testerRef;
    testerRef.SetImpulse(impulse);
    std::vector<double> fullBlock = { 1.0, 0.0, 1.0, 0.0 };
    testerRef.Convolve(fullBlock);

    // Results should match
    bool match = ApproxEqual(block1[0], fullBlock[0]) &&
                 ApproxEqual(block1[1], fullBlock[1]) &&
                 ApproxEqual(block2[0], fullBlock[2]) &&
                 ApproxEqual(block2[1], fullBlock[3]);

    if (!match)
    {
      std::cout << "FAILED - block results don't match full processing\n";
      std::cout << "  Block1: [" << block1[0] << ", " << block1[1] << "]\n";
      std::cout << "  Block2: [" << block2[0] << ", " << block2[1] << "]\n";
      std::cout << "  Full:   [" << fullBlock[0] << ", " << fullBlock[1]
                << ", " << fullBlock[2] << ", " << fullBlock[3] << "]\n";
      return false;
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 8: Longer IR to stress circular buffer wrapping
  bool TestLongIR()
  {
    std::cout << "Test: Long IR (circular buffer wrapping)... ";

    // Create an IR longer than input to ensure wrapping works
    std::vector<float> impulse(10, 0.1f);  // 10 taps, each 0.1
    std::vector<double> input = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

    // Get reference result
    std::vector<double> expected = ReferenceConvolve(input, impulse);

    // Get NAMDSPManager result
    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> result = input;
    tester.Convolve(result);

    for (std::size_t i = 0; i < input.size(); ++i)
    {
      if (!ApproxEqual(result[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << result[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 9: Empty impulse should pass through unchanged
  bool TestEmptyIR()
  {
    std::cout << "Test: Empty IR (passthrough)... ";

    std::vector<float> impulse = {};
    std::vector<double> input = { 1.0, 2.0, 3.0 };
    std::vector<double> expected = { 1.0, 2.0, 3.0 };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);
    std::vector<double> testSamples = input;
    tester.Convolve(testSamples);

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      if (!ApproxEqual(testSamples[i], expected[i]))
      {
        std::cout << "FAILED at index " << i
                  << " (expected " << expected[i] << ", got " << testSamples[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

  // Test 10: Stereo channels - verify each channel has independent state
  bool TestStereoChannels()
  {
    std::cout << "Test: Stereo channels (independent state)... ";

    std::vector<float> impulse = { 0.5f, 0.5f };

    IRConvolutionTester tester;
    tester.SetImpulse(impulse);

    // Process different data on each channel
    std::vector<double> channelL = { 1.0, 0.0, 0.0 };
    std::vector<double> channelR = { 0.0, 1.0, 0.0 };

    tester.Convolve(channelL, 0);
    tester.Convolve(channelR, 1);

    // Left channel: impulse at t=0, so output = [0.5, 0.5, 0]
    // Right channel: impulse at t=1, so output = [0, 0.5, 0.5]
    std::vector<double> expectedL = { 0.5, 0.5, 0.0 };
    std::vector<double> expectedR = { 0.0, 0.5, 0.5 };

    for (std::size_t i = 0; i < expectedL.size(); ++i)
    {
      if (!ApproxEqual(channelL[i], expectedL[i]))
      {
        std::cout << "FAILED on L channel at index " << i
                  << " (expected " << expectedL[i] << ", got " << channelL[i] << ")\n";
        return false;
      }
      if (!ApproxEqual(channelR[i], expectedR[i]))
      {
        std::cout << "FAILED on R channel at index " << i
                  << " (expected " << expectedR[i] << ", got " << channelR[i] << ")\n";
        return false;
      }
    }

    std::cout << "OK\n";
    return true;
  }

} // anonymous namespace

int main()
{
  std::cout << "IR Convolution Tests (using NAMDSPManager)\n";
  std::cout << "===========================================\n\n";

  int passed = 0;
  int failed = 0;

  auto runTest = [&](bool (*test)()) {
    if (test())
    {
      ++passed;
    }
    else
    {
      ++failed;
    }
  };

  runTest(TestIdentityIR);
  runTest(TestGainIR);
  runTest(TestTwoTapAverageIR);
  runTest(TestDelayIR);
  runTest(TestThreeTapIR);
  runTest(TestAgainstReference);
  runTest(TestMultipleBlocks);
  runTest(TestLongIR);
  runTest(TestEmptyIR);
  runTest(TestStereoChannels);

  std::cout << "\n===========================================\n";
  std::cout << "Results: " << passed << "/" << (passed + failed) << " tests passed.\n";

  if (failed > 0)
  {
    std::cout << "\nSome tests FAILED.\n";
    return 1;
  }

  std::cout << "\nAll tests PASSED.\n";
  return 0;
}
