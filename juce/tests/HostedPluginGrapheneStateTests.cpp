#include "JuceHostedPluginEffect.h"

#include "IPluginHost.h"
#include "PluginController.h"
#include "dsp/EffectGuids.h"
#include "presets/PresetStorage.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

namespace
{
constexpr auto kSkipCode = 77;
constexpr auto kFailCode = 1;
constexpr auto kSampleRate = 48000.0;
constexpr auto kBlockSize = 512;
constexpr const char* kGraphenePath = R"(C:\Program Files\Common Files\VST3\PolyChrome DSP\Graphene.vst3)";
constexpr const char* kStateConfigKey = "pluginStateBase64";
constexpr const char* kPluginNodeId = "plugin-host-node";

class TestHost final : public guitarfx::IPluginHost
{
public:
    explicit TestHost(fs::path userDataPath, fs::path bundledAssetsPath = {})
        : mUserDataPath(std::move(userDataPath))
        , mBundledAssetsPath(bundledAssetsPath.empty() ? mUserDataPath : std::move(bundledAssetsPath))
    {
    }

    void SendMessageToUI(const std::string& jsonMessage) override
    {
        sentMessages.push_back(jsonMessage);
    }

    void BrowseFileAsync(guitarfx::BrowseFileType,
                         const std::string&,
                         std::function<void(const guitarfx::BrowseFileResult&)> callback) override
    {
        callback(guitarfx::BrowseFileResult{});
    }

    void SaveFileAsync(guitarfx::BrowseFileType,
                       const std::string&,
                       const std::string&,
                       std::function<void(const guitarfx::BrowseFileResult&)> callback) override
    {
        callback(guitarfx::BrowseFileResult{});
    }

    void RunOnMainThread(std::function<void()> fn) override
    {
        fn();
    }

    [[nodiscard]] fs::path GetUserDataPath() const override
    {
        return mUserDataPath;
    }

    [[nodiscard]] fs::path GetBundledAssetsPath() const override
    {
        return mBundledAssetsPath;
    }

    [[nodiscard]] double GetSampleRate() const override
    {
        return kSampleRate;
    }

    [[nodiscard]] int GetBlockSize() const override
    {
        return kBlockSize;
    }

    std::vector<std::string> sentMessages;

private:
    fs::path mUserDataPath;
    fs::path mBundledAssetsPath;
};

void SetSettingsEnvRoot(const fs::path& root)
{
#ifdef _WIN32
    _putenv_s("APPDATA", root.string().c_str());
#else
    setenv("HOME", root.string().c_str(), 1);
#endif
}

template <typename Function>
auto CallOnMessageThread(Function&& fn) -> decltype(fn())
{
    using ReturnType = decltype(fn());

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        return fn();

    if constexpr (std::is_void_v<ReturnType>)
    {
        (void) juce::MessageManager::callSync(std::forward<Function>(fn));
    }
    else
    {
        auto result = juce::MessageManager::callSync(std::forward<Function>(fn));
        if (!result.has_value())
            return ReturnType{};

        return *result;
    }
}

int Fail(const std::string& message)
{
    std::cerr << "[HostedPluginGrapheneStateTests] " << message << std::endl;
    return kFailCode;
}

bool NearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 1.0e-4f;
}

struct PluginSnapshot
{
    std::optional<int> currentProgram;
    std::vector<float> parameterValues;
    std::string stateBase64;
};

bool CompareSnapshots(const PluginSnapshot& lhs, const PluginSnapshot& rhs)
{
    if (lhs.currentProgram != rhs.currentProgram)
        return false;
    if (lhs.parameterValues.size() != rhs.parameterValues.size())
        return false;
    if (lhs.stateBase64 != rhs.stateBase64)
        return false;

    for (size_t index = 0; index < lhs.parameterValues.size(); ++index)
    {
        if (!NearlyEqual(lhs.parameterValues[index], rhs.parameterValues[index]))
            return false;
    }

    return true;
}

std::string DescribeSnapshotDifference(const PluginSnapshot& expected, const PluginSnapshot& actual)
{
    if (expected.currentProgram != actual.currentProgram)
    {
        return "current program mismatch: expected "
            + (expected.currentProgram.has_value() ? std::to_string(*expected.currentProgram) : std::string{"<none>"})
            + ", got "
            + (actual.currentProgram.has_value() ? std::to_string(*actual.currentProgram) : std::string{"<none>"});
    }

    if (expected.parameterValues.size() != actual.parameterValues.size())
    {
        return "parameter count mismatch: expected " + std::to_string(expected.parameterValues.size())
            + ", got " + std::to_string(actual.parameterValues.size());
    }

    for (size_t index = 0; index < expected.parameterValues.size(); ++index)
    {
        if (!NearlyEqual(expected.parameterValues[index], actual.parameterValues[index]))
        {
            return "parameter[" + std::to_string(index) + "] mismatch: expected "
                + std::to_string(expected.parameterValues[index]) + ", got " + std::to_string(actual.parameterValues[index]);
        }
    }

    if (expected.stateBase64 != actual.stateBase64)
    {
        return "state blob mismatch after restore: expected length " + std::to_string(expected.stateBase64.size())
            + ", got length " + std::to_string(actual.stateBase64.size());
    }

    return {};
}

std::string CaptureState(guitarfx::JuceHostedPluginEffect& effect)
{
    return effect.GetConfig(kStateConfigKey);
}

juce::AudioPluginInstance* RequirePlugin(guitarfx::JuceHostedPluginEffect& effect)
{
    return effect.GetHostedPluginForTesting();
}

guitarfx::JuceHostedPluginEffect* RequireHostedEffect(guitarfx::PluginController& controller,
                                                      const std::string& presetId,
                                                      const std::string& nodeId)
{
    auto* processor = controller.GetMixer().GetNodeProcessor(presetId, nodeId);
    return dynamic_cast<guitarfx::JuceHostedPluginEffect*>(processor);
}

std::optional<int> GetCurrentProgram(juce::AudioPluginInstance& plugin)
{
    return CallOnMessageThread([&plugin]() -> std::optional<int>
    {
        if (plugin.getNumPrograms() <= 1)
            return std::nullopt;

        return plugin.getCurrentProgram();
    });
}

PluginSnapshot CaptureSnapshot(guitarfx::JuceHostedPluginEffect& effect, juce::AudioPluginInstance& plugin)
{
    PluginSnapshot snapshot;
    snapshot.currentProgram = GetCurrentProgram(plugin);
    snapshot.stateBase64 = CaptureState(effect);
    snapshot.parameterValues = CallOnMessageThread([&plugin]() {
        std::vector<float> values;
        const auto& parameters = plugin.getParameters();
        values.reserve(parameters.size());
        for (auto* parameter : parameters)
        {
            if (parameter == nullptr)
                continue;

            values.push_back(parameter->getValue());
        }
        return values;
    });
    return snapshot;
}

struct PluginMutationSummary
{
    bool changedProgram = false;
    bool changedParameter = false;
};

PluginMutationSummary MutatePluginState(juce::AudioPluginInstance& plugin)
{
    PluginMutationSummary summary;

    summary.changedProgram = CallOnMessageThread([&plugin]() {
        if (plugin.getNumPrograms() <= 1)
            return false;

        const int currentProgram = plugin.getCurrentProgram();
        const int targetProgram = currentProgram == 0 ? 1 : 0;
        plugin.setCurrentProgram(targetProgram);
        return targetProgram != currentProgram;
    });

    if (summary.changedProgram)
        return summary;

    summary.changedParameter = CallOnMessageThread([&plugin]() {
        const auto& parameters = plugin.getParameters();
        for (int index = 0; index < static_cast<int>(parameters.size()); ++index)
        {
            auto* parameter = parameters[static_cast<size_t>(index)];
            if (parameter == nullptr || parameter->isMetaParameter())
                continue;

            const float currentValue = parameter->getValue();
            const float targetValue = currentValue < 0.5f ? 0.75f : 0.25f;
            if (NearlyEqual(parameter->getValue(), targetValue))
                continue;

            parameter->setValueNotifyingHost(targetValue);
            return true;
        }

        return false;
    });

    return summary;
}

bool LoadPreparedPlugin(guitarfx::JuceHostedPluginEffect& effect)
{
    effect.Prepare(kSampleRate, kBlockSize);
    return effect.LoadResource(fs::path{kGraphenePath});
}

guitarfx::Preset BuildHostedPluginPreset(const std::string& presetId, const std::string& presetName)
{
    guitarfx::Preset preset;
    preset.id = presetId;
    preset.name = presetName;
    preset.version = 2;
    preset.category = "Test";

    guitarfx::GraphNode inputNode;
    inputNode.id = "__input__";
    inputNode.type = guitarfx::kNodeTypeInput;

    guitarfx::GraphNode outputNode;
    outputNode.id = "__output__";
    outputNode.type = guitarfx::kNodeTypeOutput;

    guitarfx::GraphNode pluginNode;
    pluginNode.id = kPluginNodeId;
    pluginNode.type = guitarfx::EffectGuids::kPluginHost;
    pluginNode.category = "utility";
    pluginNode.label = "Graphene";

    guitarfx::ResourceRef resource;
    resource.resourceType = "plugin";
    resource.filePath = fs::path{kGraphenePath};
    pluginNode.resources.push_back(resource);

    preset.graph.nodes = {inputNode, pluginNode, outputNode};
    preset.graph.edges = {
        {"__input__", kPluginNodeId, 0, 0, 1.0},
        {kPluginNodeId, "__output__", 0, 0, 1.0},
    };
    guitarfx::NormalizePresetScenes(preset);
    return preset;
}

bool TestHostedPluginSnapshotRoundTrip()
{
    guitarfx::JuceHostedPluginEffect sourceEffect;
    if (!LoadPreparedPlugin(sourceEffect))
    {
        Fail("Failed to load Graphene in source effect");
        return false;
    }

    auto* sourcePlugin = RequirePlugin(sourceEffect);
    if (sourcePlugin == nullptr)
    {
        Fail("Source effect did not expose a hosted plugin instance");
        return false;
    }

    const PluginSnapshot baselineSnapshot = CaptureSnapshot(sourceEffect, *sourcePlugin);
    if (baselineSnapshot.stateBase64.empty())
    {
        Fail("Baseline Graphene state capture returned an empty blob");
        return false;
    }

    const auto mutation = MutatePluginState(*sourcePlugin);
    if (!mutation.changedProgram && !mutation.changedParameter)
    {
        Fail("Graphene exposed neither mutable programs nor mutable parameters for state mutation");
        return false;
    }

    const PluginSnapshot mutatedSnapshot = CaptureSnapshot(sourceEffect, *sourcePlugin);
    if (mutatedSnapshot.stateBase64.empty())
    {
        Fail("Mutated Graphene state capture returned an empty blob");
        return false;
    }

    if (CompareSnapshots(mutatedSnapshot, baselineSnapshot))
    {
        Fail("Graphene full plugin snapshot did not change after mutation");
        return false;
    }

    guitarfx::JuceHostedPluginEffect restoredEffect;
    restoredEffect.Prepare(kSampleRate, kBlockSize);
    restoredEffect.SetConfig(kStateConfigKey, mutatedSnapshot.stateBase64);
    if (!restoredEffect.LoadResource(fs::path{kGraphenePath}))
    {
        Fail("Failed to load Graphene in restored effect");
        return false;
    }

    auto* restoredPlugin = RequirePlugin(restoredEffect);
    if (restoredPlugin == nullptr)
    {
        Fail("Restored effect did not expose a hosted plugin instance");
        return false;
    }

    const PluginSnapshot restoredSnapshot = CaptureSnapshot(restoredEffect, *restoredPlugin);
    if (!CompareSnapshots(mutatedSnapshot, restoredSnapshot))
    {
        Fail("Restored Graphene snapshot did not match the captured full plugin snapshot: "
            + DescribeSnapshotDifference(mutatedSnapshot, restoredSnapshot));
        return false;
    }

    return true;
}

bool TestControllerPresetSaveLoadRoundTrip()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-hosted-plugin-tests" / "controller-save-load";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    guitarfx::RegisterJuceHostedPluginEffect();

    TestHost sourceHost(sandbox);
    guitarfx::PluginController sourceController(sourceHost);
    sourceController.Initialize();
    sourceController.Prepare(kSampleRate, kBlockSize);

    const auto sourcePreset = BuildHostedPluginPreset("graphene-controller-source", "Graphene Controller Source");
    sourceController.HandleUIMessage(nlohmann::json{
        {"type", "loadPreset"},
        {"presetId", sourcePreset.id},
        {"preset", nlohmann::json::parse(guitarfx::PresetStorage::SerializeToJson(sourcePreset))}
    }.dump());

    auto* sourceEffect = RequireHostedEffect(sourceController, sourcePreset.id, kPluginNodeId);
    if (sourceEffect == nullptr)
    {
        Fail("Controller source preset did not create a hosted plugin effect");
        return false;
    }

    auto* sourcePlugin = RequirePlugin(*sourceEffect);
    if (sourcePlugin == nullptr)
    {
        Fail("Controller source preset did not expose a hosted plugin instance");
        return false;
    }

    const auto mutation = MutatePluginState(*sourcePlugin);
    if (!mutation.changedProgram && !mutation.changedParameter)
    {
        Fail("Controller source preset could not mutate Graphene state");
        return false;
    }

    const PluginSnapshot mutatedSnapshot = CaptureSnapshot(*sourceEffect, *sourcePlugin);
    if (mutatedSnapshot.stateBase64.empty())
    {
        Fail("Controller source preset captured an empty hosted plugin state blob");
        return false;
    }

    const std::string savedPresetId = "graphene-controller-saved";
    sourceController.HandleUIMessage(nlohmann::json{
        {"type", "savePreset"},
        {"name", "Graphene Controller Saved"},
        {"category", "Unit"},
        {"description", "Hosted plugin preset save/load round-trip"},
        {"presetId", savedPresetId}
    }.dump());

    const fs::path savedPresetPath = sandbox / "Soundshed Guitar" / "data" / "v1" / "presets" / "user" / (savedPresetId + ".json");
    const auto savedPreset = guitarfx::PresetStorage::LoadFromFile(savedPresetPath);
    if (!savedPreset)
    {
        Fail("Saved preset file was not written for the controller round-trip test");
        return false;
    }

    const auto* savedNode = savedPreset->graph.FindNode(kPluginNodeId);
    if (savedNode == nullptr)
    {
        Fail("Saved preset is missing the hosted plugin node");
        return false;
    }

    const auto savedStateIt = savedNode->config.find(kStateConfigKey);
    if (savedStateIt == savedNode->config.end() || savedStateIt->second.empty())
    {
        Fail("Saved preset did not persist the hosted plugin state blob");
        return false;
    }

    if (savedStateIt->second != mutatedSnapshot.stateBase64)
    {
        Fail("Saved preset hosted plugin state blob did not match the mutated plugin snapshot");
        return false;
    }

    TestHost restoredHost(sandbox);
    guitarfx::PluginController restoredController(restoredHost);
    restoredController.Initialize();
    restoredController.Prepare(kSampleRate, kBlockSize);
    restoredController.HandleUIMessage(nlohmann::json{
        {"type", "loadPreset"},
        {"presetId", savedPreset->id},
        {"preset", nlohmann::json::parse(guitarfx::PresetStorage::SerializeToJson(*savedPreset))}
    }.dump());

    auto* restoredEffect = RequireHostedEffect(restoredController, savedPreset->id, kPluginNodeId);
    if (restoredEffect == nullptr)
    {
        Fail("Restored controller preset did not create a hosted plugin effect");
        return false;
    }

    auto* restoredPlugin = RequirePlugin(*restoredEffect);
    if (restoredPlugin == nullptr)
    {
        Fail("Restored controller preset did not expose a hosted plugin instance");
        return false;
    }

    const PluginSnapshot restoredSnapshot = CaptureSnapshot(*restoredEffect, *restoredPlugin);
    if (!CompareSnapshots(mutatedSnapshot, restoredSnapshot))
    {
        Fail("Controller preset save/load did not restore the full Graphene plugin snapshot: "
            + DescribeSnapshotDifference(mutatedSnapshot, restoredSnapshot));
        return false;
    }

    return true;
}
} // namespace

int main()
{
    if (!fs::exists(fs::path{kGraphenePath}))
    {
        std::cout << "Skipping Graphene hosted plugin state test; plugin not found at " << kGraphenePath << std::endl;
        return kSkipCode;
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    int failed = 0;
    const auto run = [&failed](const char* name, bool ok)
    {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
        if (!ok)
            ++failed;
    };

    run("Hosted effect full snapshot round-trip", TestHostedPluginSnapshotRoundTrip());
    run("Controller preset save/load full snapshot round-trip", TestControllerPresetSaveLoadRoundTrip());

    if (failed != 0)
        return kFailCode;

    std::cout << "Graphene hosted plugin snapshot round-trip tests passed" << std::endl;
    return 0;
}