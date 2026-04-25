#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/EffectProcessor.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace guitarfx
{
class JuceHostedPluginEffect final : public EffectProcessor
{
public:
    JuceHostedPluginEffect();
    ~JuceHostedPluginEffect() override;

    void Prepare(double sampleRate, int maxBlockSize) override;
    void Reset() override;
    void Process(float** inputs, float** outputs, int numSamples) override;

    void SetParam(const std::string& key, double value) override;
    [[nodiscard]] double GetParam(const std::string& key) const override;

    void SetConfig(const std::string& key, const std::string& value) override;
    [[nodiscard]] std::string GetConfig(const std::string& key) const override;

    bool LoadResource(const std::filesystem::path& path) override;
    bool LoadResources(const std::vector<ResourceRef>& refs,
                       const std::vector<std::filesystem::path>& paths) override;
    [[nodiscard]] bool RequiresResource() const override { return true; }
    [[nodiscard]] bool HasResource() const override { return mPlugin != nullptr; }
    [[nodiscard]] std::filesystem::path GetResourcePath() const override { return mPluginPath; }
    [[nodiscard]] int GetLatencySamples() const override;

    [[nodiscard]] std::string GetType() const override { return "plugin_host"; }
    [[nodiscard]] std::string GetCategory() const override { return "utility"; }

#if defined(GUITARFX_ENABLE_PLUGIN_HOST_TEST_API)
    [[nodiscard]] juce::AudioPluginInstance* GetHostedPluginForTesting() const { return mPlugin.get(); }
#endif

private:
    void EnsureFormatsAdded();
    bool LoadPluginFromPath(const std::filesystem::path& path);
    bool ConfigurePluginBuses(juce::AudioPluginInstance& plugin) const;
    void PrepareLoadedPlugin();
    void CopyInputToWorkBuffer(float** inputs, int numSamples);
    void CopyWorkBufferToOutputs(float** inputs, float** outputs, int numSamples);
    void Passthrough(float** inputs, float** outputs, int numSamples) const;
    void ApplyPluginStateBase64(const std::string& value);
    void ApplyPendingPluginState();
    [[nodiscard]] std::string CapturePluginStateBase64() const;
    void OpenPluginEditor();
    void ClosePluginEditor();
    void SetError(const std::string& message);

    juce::AudioPluginFormatManager mFormatManager;
    juce::AudioBuffer<float> mWorkBuffer;
    juce::MidiBuffer mMidiBuffer;
    std::unique_ptr<juce::AudioPluginInstance> mPlugin;
    std::unique_ptr<juce::DocumentWindow> mEditorWindow;
    juce::PluginDescription mPluginDescription;

    std::filesystem::path mPluginPath;
    std::string mPluginFormat;
    std::string mPluginIdentifier;
    std::string mPluginStateBase64;
    std::string mLastError;

    double mMix = 1.0;
    double mInputGainDb = 0.0;
    double mOutputGainDb = 0.0;
    bool mFormatsAdded = false;
    bool mPrepared = false;
};

void RegisterJuceHostedPluginEffect();

} // namespace guitarfx
