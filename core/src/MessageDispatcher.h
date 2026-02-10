#pragma once

/**
 * MessageDispatcher — Routes incoming JSON messages from the WebView
 * to the appropriate PluginController handler.
 *
 * This replaces the duplicated if-else chains in:
 *   - GuitarFXPlugin::HandleUIMessage (iPlug2, ~380 lines)
 *   - PluginProcessor::handleWebMessage (JUCE, ~430 lines)
 *
 * The canonical message set is the union of all message types from both
 * frameworks, ensuring full feature parity.
 */

#include <string>

#include <nlohmann/json.hpp>

namespace guitarfx
{

class PluginController;

class MessageDispatcher
{
public:
    /**
     * Parse a JSON message string and dispatch to the appropriate
     * PluginController handler based on the "type" field.
     *
     * Thread safety: this is called on the UI/main thread. The handlers
     * may acquire mDSPMutex when modifying DSP state.
     */
    static void Dispatch(PluginController& controller, const std::string& jsonMessage);

private:
    static bool DispatchStateAndLists(PluginController& controller,
                                      const nlohmann::json& msg,
                                      const std::string& type);
    static bool DispatchSettings(PluginController& controller,
                                 const nlohmann::json& msg,
                                 const std::string& type);
    static bool DispatchParameters(PluginController& controller,
                                   const nlohmann::json& msg,
                                   const std::string& type);
    static bool DispatchPresetsAndResources(PluginController& controller,
                                            const nlohmann::json& msg,
                                            const std::string& type);
    static bool DispatchSignalPath(PluginController& controller,
                                   const nlohmann::json& msg,
                                   const std::string& type);
    static bool DispatchMixerAndMonitoring(PluginController& controller,
                                           const nlohmann::json& msg,
                                           const std::string& type);
    static bool DispatchLibraryAndComposite(PluginController& controller,
                                            const nlohmann::json& msg,
                                            const std::string& type);
};

} // namespace guitarfx
