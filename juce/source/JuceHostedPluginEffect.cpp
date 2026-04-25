#include "JuceHostedPluginEffect.h"

#include "dsp/EffectGuids.h"
#include "dsp/EffectRegistry.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace guitarfx
{
namespace
{
constexpr const char* kPluginStateBase64ConfigKey = "pluginStateBase64";

double Clamp(double value, double minimum, double maximum)
{
    return std::min(maximum, std::max(minimum, value));
}

float DbToLinear(double db)
{
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

juce::String ToJucePath(const std::filesystem::path& path)
{
#if JUCE_WINDOWS
    const auto widePath = path.wstring();
    return juce::String(widePath.c_str());
#else
    return juce::String(path.string());
#endif
}

std::string ToDisplayPath(const std::filesystem::path& path)
{
    return path.string();
}

std::string FromJuceString(const juce::String& value)
{
    return value.toStdString();
}

class HostedPluginEditorWindow final : public juce::DocumentWindow
{
public:
    HostedPluginEditorWindow(const juce::String& title, juce::AudioProcessorEditor* editor)
        : DocumentWindow(title,
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        centreWithSize(std::max(360, getWidth()), std::max(220, getHeight()));
        setResizable(editor != nullptr && editor->isResizable(), true);
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostedPluginEditorWindow)
};
} // namespace

JuceHostedPluginEffect::JuceHostedPluginEffect() = default;

JuceHostedPluginEffect::~JuceHostedPluginEffect()
{
    ClosePluginEditor();
    if (mPlugin)
        mPlugin->releaseResources();
}

void JuceHostedPluginEffect::EnsureFormatsAdded()
{
    if (mFormatsAdded)
        return;

    juce::addDefaultFormatsToManager(mFormatManager);
    mFormatsAdded = true;
}

void JuceHostedPluginEffect::Prepare(double sampleRate, int maxBlockSize)
{
    if (!ValidatePrepare(sampleRate, maxBlockSize))
        return;

    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;
    mPrepared = true;
    mWorkBuffer.setSize(2, maxBlockSize, false, false, true);
    PrepareLoadedPlugin();
}

void JuceHostedPluginEffect::Reset()
{
    mMidiBuffer.clear();
    if (mPlugin)
        mPlugin->reset();
}

void JuceHostedPluginEffect::Process(float** inputs, float** outputs, int numSamples)
{
    if (!inputs || !outputs || numSamples <= 0)
        return;

    if (!mPlugin || !mPrepared || numSamples > mWorkBuffer.getNumSamples())
    {
        Passthrough(inputs, outputs, numSamples);
        return;
    }

    CopyInputToWorkBuffer(inputs, numSamples);
    mMidiBuffer.clear();
    mPlugin->processBlock(mWorkBuffer, mMidiBuffer);
    CopyWorkBufferToOutputs(inputs, outputs, numSamples);
}

void JuceHostedPluginEffect::SetParam(const std::string& key, double value)
{
    if (key == "mix")
        mMix = Clamp(value, 0.0, 1.0);
    else if (key == "inputGain")
        mInputGainDb = Clamp(value, -24.0, 24.0);
    else if (key == "outputGain")
        mOutputGainDb = Clamp(value, -24.0, 24.0);
}

double JuceHostedPluginEffect::GetParam(const std::string& key) const
{
    if (key == "mix")
        return mMix;
    if (key == "inputGain")
        return mInputGainDb;
    if (key == "outputGain")
        return mOutputGainDb;
    return 0.0;
}

void JuceHostedPluginEffect::SetConfig(const std::string& key, const std::string& value)
{
    if (key == "pluginPath")
    {
        if (!value.empty())
            LoadPluginFromPath(std::filesystem::path(value));
        return;
    }

    if (key == "pluginFormat")
    {
        mPluginFormat = value;
        return;
    }

    if (key == "pluginIdentifier")
    {
        mPluginIdentifier = value;
        return;
    }

    if (key == kPluginStateBase64ConfigKey)
    {
        mPluginStateBase64 = value;
        ApplyPendingPluginState();
        return;
    }

    if (key == "showPluginEditor" || key == "openPluginEditor")
    {
        if (value != "0" && value != "false")
            OpenPluginEditor();
        return;
    }
}

std::string JuceHostedPluginEffect::GetConfig(const std::string& key) const
{
    if (key == "pluginPath")
        return ToDisplayPath(mPluginPath);
    if (key == "pluginFormat")
        return mPluginFormat;
    if (key == "pluginIdentifier")
        return mPluginIdentifier;
    if (key == "pluginName")
        return FromJuceString(mPluginDescription.name);
    if (key == kPluginStateBase64ConfigKey)
        return CapturePluginStateBase64();
    if (key == "lastError")
        return mLastError;
    return {};
}

bool JuceHostedPluginEffect::LoadResource(const std::filesystem::path& path)
{
    return LoadPluginFromPath(path);
}

bool JuceHostedPluginEffect::LoadResources(const std::vector<ResourceRef>& refs,
                                           const std::vector<std::filesystem::path>& paths)
{
    for (std::size_t i = 0; i < refs.size(); ++i)
    {
        const auto& ref = refs[i];
        if (ref.resourceType != "plugin" && ref.resourceType != "audio-plugin" && ref.resourceType != "midi-plugin")
            continue;

        if (i < paths.size())
            return LoadPluginFromPath(paths[i]);
    }

    if (!paths.empty())
        return LoadPluginFromPath(paths.front());

    return false;
}

int JuceHostedPluginEffect::GetLatencySamples() const
{
    return mPlugin ? mPlugin->getLatencySamples() : 0;
}

bool JuceHostedPluginEffect::LoadPluginFromPath(const std::filesystem::path& path)
{
    EnsureFormatsAdded();

    const juce::File pluginFile(ToJucePath(path));
    if (!pluginFile.exists())
    {
        SetError("Plugin file does not exist: " + ToDisplayPath(path));
        mPlugin.reset();
        return false;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    const auto fileOrIdentifier = pluginFile.getFullPathName();

    for (int i = 0; i < mFormatManager.getNumFormats(); ++i)
    {
        auto* format = mFormatManager.getFormat(i);
        if (!format)
            continue;

        if (!mPluginFormat.empty() && !format->getName().equalsIgnoreCase(juce::String(mPluginFormat)))
            continue;

        if (format->fileMightContainThisPluginType(fileOrIdentifier) || !mPluginFormat.empty())
            format->findAllTypesForFile(descriptions, fileOrIdentifier);
    }

    if (descriptions.isEmpty())
    {
        SetError("No JUCE-supported plugin types were found in: " + ToDisplayPath(path));
        mPlugin.reset();
        return false;
    }

    juce::PluginDescription* selected = descriptions.getFirst();
    if (!mPluginIdentifier.empty())
    {
        for (auto* description : descriptions)
        {
            if (description && (description->createIdentifierString() == juce::String(mPluginIdentifier)
                || description->fileOrIdentifier == juce::String(mPluginIdentifier)))
            {
                selected = description;
                break;
            }
        }
    }

    if (!selected)
    {
        SetError("Plugin scan returned no selectable plugin descriptions");
        mPlugin.reset();
        return false;
    }

    juce::String error;
    auto instance = mFormatManager.createPluginInstance(*selected, mSampleRate, mMaxBlockSize, error);
    if (!instance)
    {
        SetError(error.isNotEmpty() ? FromJuceString(error) : "JUCE failed to instantiate plugin");
        mPlugin.reset();
        return false;
    }

    if (!ConfigurePluginBuses(*instance))
    {
        SetError("Plugin does not support a mono or stereo main bus layout");
        instance->releaseResources();
        mPlugin.reset();
        return false;
    }

    mPluginDescription = *selected;
    mPluginPath = path;
    mPluginFormat = FromJuceString(selected->pluginFormatName);
    mPluginIdentifier = FromJuceString(selected->createIdentifierString());
    ClosePluginEditor();
    mPlugin = std::move(instance);
    mLastError.clear();
    if (!mPluginStateBase64.empty())
        ApplyPluginStateBase64(mPluginStateBase64);
    PrepareLoadedPlugin();
    return true;
}

bool JuceHostedPluginEffect::ConfigurePluginBuses(juce::AudioPluginInstance& plugin) const
{
    const juce::AudioChannelSet stereo = juce::AudioChannelSet::stereo();
    const juce::AudioChannelSet mono = juce::AudioChannelSet::mono();
    const juce::AudioChannelSet disabled = juce::AudioChannelSet::disabled();

    const bool hasMainInput = plugin.getBusCount(true) > 0;
    const bool hasMainOutput = plugin.getBusCount(false) > 0;
    if (!hasMainOutput)
        return false;

    const juce::AudioProcessor::BusesLayout stereoLayout{
        hasMainInput ? juce::Array<juce::AudioChannelSet>{stereo} : juce::Array<juce::AudioChannelSet>{},
        juce::Array<juce::AudioChannelSet>{stereo}
    };
    if (plugin.checkBusesLayoutSupported(stereoLayout) && plugin.setBusesLayout(stereoLayout))
        return true;

    const juce::AudioProcessor::BusesLayout monoLayout{
        hasMainInput ? juce::Array<juce::AudioChannelSet>{mono} : juce::Array<juce::AudioChannelSet>{},
        juce::Array<juce::AudioChannelSet>{mono}
    };
    if (plugin.checkBusesLayoutSupported(monoLayout) && plugin.setBusesLayout(monoLayout))
        return true;

    auto current = plugin.getBusesLayout();
    if (current.outputBuses.size() > 0 && current.outputBuses.getReference(0).size() > 0)
    {
        for (int i = 1; i < current.inputBuses.size(); ++i)
            current.inputBuses.getReference(i) = disabled;
        for (int i = 1; i < current.outputBuses.size(); ++i)
            current.outputBuses.getReference(i) = disabled;
        return plugin.checkBusesLayoutSupported(current) && plugin.setBusesLayout(current);
    }

    return false;
}

void JuceHostedPluginEffect::PrepareLoadedPlugin()
{
    if (!mPlugin || !mPrepared)
        return;

    mPlugin->setRateAndBufferSizeDetails(mSampleRate, mMaxBlockSize);
    mPlugin->prepareToPlay(mSampleRate, mMaxBlockSize);
    ApplyPendingPluginState();
}

void JuceHostedPluginEffect::CopyInputToWorkBuffer(float** inputs, int numSamples)
{
    const float inputGain = DbToLinear(mInputGainDb);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* dest = mWorkBuffer.getWritePointer(ch);
        const float* source = inputs[ch];
        if (source)
        {
            for (int i = 0; i < numSamples; ++i)
                dest[i] = source[i] * inputGain;
        }
        else
        {
            std::fill(dest, dest + numSamples, 0.0f);
        }
    }
}

void JuceHostedPluginEffect::CopyWorkBufferToOutputs(float** inputs, float** outputs, int numSamples)
{
    const float outputGain = DbToLinear(mOutputGainDb);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (!outputs[ch])
            continue;

        const float* dry = inputs[ch];
        const float* wet = mWorkBuffer.getReadPointer(std::min(ch, mWorkBuffer.getNumChannels() - 1));
        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dry ? dry[i] : 0.0f;
            outputs[ch][i] = static_cast<float>((drySample * (1.0 - mMix)) + (wet[i] * outputGain * mMix));
        }
    }
}

void JuceHostedPluginEffect::Passthrough(float** inputs, float** outputs, int numSamples) const
{
    for (int ch = 0; ch < 2; ++ch)
    {
        if (!outputs[ch])
            continue;

        const float* source = inputs[ch];
        if (source)
            std::copy(source, source + numSamples, outputs[ch]);
        else
            std::fill(outputs[ch], outputs[ch] + numSamples, 0.0f);
    }
}

void JuceHostedPluginEffect::ApplyPluginStateBase64(const std::string& value)
{
    if (!mPlugin || value.empty())
        return;

    juce::MemoryOutputStream standardDecoded;
    juce::MemoryBlock state;
    if (juce::Base64::convertFromBase64(standardDecoded, juce::String(value)))
    {
        state = standardDecoded.getMemoryBlock();
    }
    else if (!state.fromBase64Encoding(juce::String(value)))
    {
        SetError("Invalid hosted plugin state encoding");
        return;
    }

    const auto applyState = [this, state]()
    {
        if (!mPlugin)
            return;

        mPlugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyState();
    }
    else if (!juce::MessageManager::callSync(applyState))
    {
        SetError("Failed to restore hosted plugin state on the message thread");
        return;
    }

    mLastError.clear();
}

void JuceHostedPluginEffect::ApplyPendingPluginState()
{
    if (mPlugin && mPrepared && !mPluginStateBase64.empty())
        ApplyPluginStateBase64(mPluginStateBase64);
}

std::string JuceHostedPluginEffect::CapturePluginStateBase64() const
{
    if (!mPlugin)
        return mPluginStateBase64;

    const auto captureState = [this]() -> std::string
    {
        if (!mPlugin)
            return mPluginStateBase64;

        juce::MemoryBlock state;
        mPlugin->getStateInformation(state);
        if (state.isEmpty())
            return {};

        return FromJuceString(juce::Base64::toBase64(state.getData(), state.getSize()));
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        return captureState();

    if (auto captured = juce::MessageManager::callSync(captureState))
        return *captured;

    return {};
}

void JuceHostedPluginEffect::OpenPluginEditor()
{
    if (!mPlugin)
    {
        SetError("Cannot open hosted plugin UI before a plugin is loaded");
        return;
    }

    auto open = [this]()
    {
        if (!mPlugin)
            return;

        if (mEditorWindow)
        {
            mEditorWindow->setVisible(true);
            mEditorWindow->toFront(true);
            return;
        }

        auto* editor = mPlugin->hasEditor()
            ? mPlugin->createEditorIfNeeded()
            : static_cast<juce::AudioProcessorEditor*>(new juce::GenericAudioProcessorEditor(*mPlugin));
        if (!editor)
        {
            SetError("Hosted plugin did not provide an editor");
            return;
        }

        const auto title = mPluginDescription.name.isNotEmpty()
            ? mPluginDescription.name
            : mPlugin->getName();
        mEditorWindow = std::make_unique<HostedPluginEditorWindow>(title, editor);
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        open();
    else
        juce::MessageManager::callAsync(std::move(open));
}

void JuceHostedPluginEffect::ClosePluginEditor()
{
    if (!mEditorWindow)
        return;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        mEditorWindow.reset();
        return;
    }

    auto* window = mEditorWindow.release();
    juce::MessageManager::callAsync([window]() { delete window; });
}

void JuceHostedPluginEffect::SetError(const std::string& message)
{
    mLastError = message;
    std::cerr << "[JuceHostedPluginEffect] " << message << std::endl;
}

void RegisterJuceHostedPluginEffect()
{
    EffectTypeInfo info;
    info.type = EffectGuids::kPluginHost;
    info.aliases = {"plugin_host", "juce_plugin_host"};
    info.displayName = "Plugin Host";
    info.category = "utility";
    info.description = "Host an external JUCE-supported audio plugin inside the signal path";
    info.requiresResource = true;
    info.resourceType = "plugin";
    info.parameters = {
        {"mix", "Mix", 1.0, 0.0, 1.0, "", "", false, 0.01},
        {"inputGain", "Input", 0.0, -24.0, 24.0, "dB"},
        {"outputGain", "Output", 0.0, -24.0, 24.0, "dB"}
    };
    info.exposedResources = {
        {"plugin", "Plugin", "", "plugin", 0, true}
    };

    EffectRegistry::Instance().Register(info.type, info, []()
    {
        return std::make_unique<JuceHostedPluginEffect>();
    });
}

} // namespace guitarfx
