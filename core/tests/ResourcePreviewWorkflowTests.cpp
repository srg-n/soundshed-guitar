#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "IPluginHost.h"
#include "PluginController.h"
#include "presets/PresetStorage.h"
#include "presets/PresetTypes.h"

namespace fs = std::filesystem;

namespace
{
class TestHost final : public guitarfx::IPluginHost
{
public:
    explicit TestHost(fs::path root)
        : mRoot(std::move(root))
    {
    }

    void SendMessageToUI(const std::string& jsonMessage) override
    {
        messages.push_back(jsonMessage);
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
        return mRoot;
    }

    [[nodiscard]] fs::path GetBundledAssetsPath() const override
    {
        return mRoot;
    }

    [[nodiscard]] double GetSampleRate() const override
    {
        return 48000.0;
    }

    [[nodiscard]] int GetBlockSize() const override
    {
        return 512;
    }

    std::vector<std::string> messages;

private:
    fs::path mRoot;
};

void SetSettingsEnvRoot(const fs::path& root)
{
#ifdef _WIN32
    _putenv_s("APPDATA", root.string().c_str());
#else
    setenv("HOME", root.string().c_str(), 1);
#endif
}

guitarfx::Preset BuildSingleNodeResourcePreset(const std::string& nodeId,
                                               const std::string& resourceType,
                                               const std::string& resourceId,
                                               int resourceSlots = 1)
{
    using namespace guitarfx;

    Preset preset;
    preset.id = "preview-workflow";
    preset.name = "Preview Workflow";

    GraphNode input;
    input.id = "in";
    input.type = kNodeTypeInput;

    GraphNode effect;
    effect.id = nodeId;
    effect.type = "utility";
    effect.enabled = true;
    effect.resources.resize(static_cast<size_t>(resourceSlots));
    effect.resources[0].resourceType = resourceType;
    effect.resources[0].resourceId = resourceId;

    GraphNode output;
    output.id = "out";
    output.type = kNodeTypeOutput;

    preset.graph.nodes = {input, effect, output};
    preset.graph.edges = {
        {"in", nodeId, 0, 0, 1.0},
        {nodeId, "out", 0, 0, 1.0},
    };

    return preset;
}

bool LoadPreset(guitarfx::PluginController& controller, const guitarfx::Preset& preset)
{
    const auto presetJson = nlohmann::json::parse(guitarfx::PresetStorage::SerializeToJson(preset));
    nlohmann::json message;
    message["type"] = "loadPreset";
    message["preset"] = presetJson;
    controller.HandleUIMessage(message.dump());

    return controller.GetActivePreset().has_value();
}

bool TestPreviewApplyAndCancel(const std::string& resourceType)
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / resourceType;
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    const std::string nodeId = resourceType == "ir" ? "cab-node" : "amp-node";
    const std::string originalResourceId = resourceType == "ir" ? "orig-ir" : "orig-nam";

    auto preset = BuildSingleNodeResourcePreset(nodeId, resourceType, originalResourceId, 1);
    if (!LoadPreset(controller, preset))
    {
        std::cerr << "Failed to load initial preset for " << resourceType << " test\n";
        return false;
    }

    nlohmann::json preview;
    preview["type"] = "previewRemoteResource";
    preview["resourceType"] = resourceType;
    preview["tempResourceId"] = "preview-temp-id";
    preview["nodeId"] = nodeId;
    preview["resourceIndex"] = 0;
    preview["isZip"] = false;
    preview["data"] = "AQID";
    controller.HandleUIMessage(preview.dump());

    const auto& activeAfterPreview = controller.GetActivePreset();
    if (!activeAfterPreview)
    {
        std::cerr << "Active preset missing after preview for " << resourceType << "\n";
        return false;
    }

    const auto* nodeAfterPreview = activeAfterPreview->graph.FindNode(nodeId);
    if (!nodeAfterPreview || nodeAfterPreview->resources.empty())
    {
        std::cerr << "Target node missing after preview for " << resourceType << "\n";
        return false;
    }

    const auto& previewRef = nodeAfterPreview->resources[0];
    if (!previewRef.filePath.has_filename() || !fs::exists(previewRef.filePath))
    {
        std::cerr << "Preview file path not set/existing for " << resourceType << "\n";
        return false;
    }

    if (!previewRef.resourceId.empty())
    {
        std::cerr << "Preview should clear resourceId but got: " << previewRef.resourceId << "\n";
        return false;
    }

    nlohmann::json cancel;
    cancel["type"] = "cancelPreviewResource";
    cancel["nodeId"] = nodeId;
    cancel["resourceIndex"] = 0;
    controller.HandleUIMessage(cancel.dump());

    const auto& activeAfterCancel = controller.GetActivePreset();
    if (!activeAfterCancel)
    {
        std::cerr << "Active preset missing after cancel for " << resourceType << "\n";
        return false;
    }

    const auto* nodeAfterCancel = activeAfterCancel->graph.FindNode(nodeId);
    if (!nodeAfterCancel || nodeAfterCancel->resources.empty())
    {
        std::cerr << "Target node missing after cancel for " << resourceType << "\n";
        return false;
    }

    const auto& restoredRef = nodeAfterCancel->resources[0];
    if (restoredRef.resourceId != originalResourceId)
    {
        std::cerr << "Cancel did not restore original resourceId for " << resourceType << "\n";
        return false;
    }

    if (!restoredRef.filePath.empty())
    {
        std::cerr << "Cancel should restore library ref without file path for " << resourceType << "\n";
        return false;
    }

    return true;
}

bool TestPreviewHonorsResourceIndex()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / "index";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    auto preset = BuildSingleNodeResourcePreset("multi-slot", "nam", "slot-0", 2);
    preset.graph.FindNode("multi-slot")->resources[1].resourceType = "nam";
    preset.graph.FindNode("multi-slot")->resources[1].resourceId = "slot-1";

    if (!LoadPreset(controller, preset))
    {
        std::cerr << "Failed to load multi-slot preset\n";
        return false;
    }

    nlohmann::json preview;
    preview["type"] = "previewRemoteResource";
    preview["resourceType"] = "nam";
    preview["tempResourceId"] = "preview-index";
    preview["nodeId"] = "multi-slot";
    preview["resourceIndex"] = 1;
    preview["isZip"] = false;
    preview["data"] = "AQID";
    controller.HandleUIMessage(preview.dump());

    const auto& activeAfterPreview = controller.GetActivePreset();
    if (!activeAfterPreview)
        return false;

    const auto* node = activeAfterPreview->graph.FindNode("multi-slot");
    if (!node || node->resources.size() < 2)
        return false;

    const auto& slot0 = node->resources[0];
    const auto& slot1 = node->resources[1];

    if (slot0.resourceId != "slot-0" || !slot0.filePath.empty())
    {
        std::cerr << "Preview unexpectedly modified slot 0\n";
        return false;
    }

    if (!slot1.resourceId.empty() || slot1.filePath.empty())
    {
        std::cerr << "Preview did not apply to selected slot index\n";
        return false;
    }

    nlohmann::json cancel;
    cancel["type"] = "cancelPreviewResource";
    cancel["nodeId"] = "multi-slot";
    cancel["resourceIndex"] = 1;
    controller.HandleUIMessage(cancel.dump());

    const auto& activeAfterCancel = controller.GetActivePreset();
    if (!activeAfterCancel)
        return false;

    const auto* nodeAfterCancel = activeAfterCancel->graph.FindNode("multi-slot");
    if (!nodeAfterCancel || nodeAfterCancel->resources.size() < 2)
        return false;

    if (nodeAfterCancel->resources[1].resourceId != "slot-1" || !nodeAfterCancel->resources[1].filePath.empty())
    {
        std::cerr << "Cancel did not restore selected slot\n";
        return false;
    }

    return true;
}

bool TestLibraryPreviewCloseRevertsOriginal()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / "library-revert";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    auto preset = BuildSingleNodeResourcePreset("amp-node", "nam", "original-lib-id", 1);
    if (!LoadPreset(controller, preset))
    {
        std::cerr << "Failed to load preset for library preview revert test\n";
        return false;
    }

    nlohmann::json previewSelect;
    previewSelect["type"] = "updateNodeResource";
    previewSelect["nodeId"] = "amp-node";
    previewSelect["resourceType"] = "nam";
    previewSelect["resourceId"] = "preview-lib-id";
    previewSelect["filePath"] = "";
    previewSelect["resourceIndex"] = 0;
    controller.HandleUIMessage(previewSelect.dump());

    const auto& activeAfterPreview = controller.GetActivePreset();
    if (!activeAfterPreview)
        return false;

    const auto* previewNode = activeAfterPreview->graph.FindNode("amp-node");
    if (!previewNode || previewNode->resources.empty())
        return false;

    if (previewNode->resources[0].resourceId != "preview-lib-id")
    {
        std::cerr << "Preview selection did not update to temporary library resource\n";
        return false;
    }

    // Simulate modal close without confirm: UI reverts with updateNodeResource to original ID.
    nlohmann::json revert;
    revert["type"] = "updateNodeResource";
    revert["nodeId"] = "amp-node";
    revert["resourceType"] = "nam";
    revert["resourceId"] = "original-lib-id";
    revert["filePath"] = "";
    revert["resourceIndex"] = 0;
    controller.HandleUIMessage(revert.dump());

    const auto& activeAfterRevert = controller.GetActivePreset();
    if (!activeAfterRevert)
        return false;

    const auto* revertedNode = activeAfterRevert->graph.FindNode("amp-node");
    if (!revertedNode || revertedNode->resources.empty())
        return false;

    if (revertedNode->resources[0].resourceId != "original-lib-id")
    {
        std::cerr << "Library preview close did not restore original resource\n";
        return false;
    }

    if (!revertedNode->resources[0].filePath.empty())
    {
        std::cerr << "Reverted library resource should not keep filePath override\n";
        return false;
    }

    return true;
}

bool TestPreviewMissingDataNoMutation()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / "missing-data";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    auto preset = BuildSingleNodeResourcePreset("amp-node", "nam", "original-lib-id", 1);
    if (!LoadPreset(controller, preset))
        return false;

    nlohmann::json preview;
    preview["type"] = "previewRemoteResource";
    preview["resourceType"] = "nam";
    preview["tempResourceId"] = "preview-missing-data";
    preview["nodeId"] = "amp-node";
    preview["resourceIndex"] = 0;
    preview["isZip"] = false;
    preview["data"] = "";
    controller.HandleUIMessage(preview.dump());

    const auto& active = controller.GetActivePreset();
    if (!active)
        return false;

    const auto* node = active->graph.FindNode("amp-node");
    if (!node || node->resources.empty())
        return false;

    return node->resources[0].resourceId == "original-lib-id" && node->resources[0].filePath.empty();
}

bool TestPreviewMissingNodeIdNoMutation()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / "missing-node-id";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    auto preset = BuildSingleNodeResourcePreset("cab-node", "ir", "original-ir-id", 1);
    if (!LoadPreset(controller, preset))
        return false;

    nlohmann::json preview;
    preview["type"] = "previewRemoteResource";
    preview["resourceType"] = "ir";
    preview["tempResourceId"] = "preview-missing-node";
    preview["resourceIndex"] = 0;
    preview["isZip"] = false;
    preview["data"] = "AQID";
    controller.HandleUIMessage(preview.dump());

    const auto& active = controller.GetActivePreset();
    if (!active)
        return false;

    const auto* node = active->graph.FindNode("cab-node");
    if (!node || node->resources.empty())
        return false;

    return node->resources[0].resourceId == "original-ir-id" && node->resources[0].filePath.empty();
}

bool TestCancelWithoutActivePreviewNoMutation()
{
    const fs::path sandbox = fs::temp_directory_path() / "guitarfx-preview-workflow-tests" / "cancel-no-preview";
    std::error_code ec;
    fs::remove_all(sandbox, ec);
    fs::create_directories(sandbox, ec);
    SetSettingsEnvRoot(sandbox);

    TestHost host(sandbox);
    guitarfx::PluginController controller(host);

    auto preset = BuildSingleNodeResourcePreset("amp-node", "nam", "steady-lib-id", 1);
    if (!LoadPreset(controller, preset))
        return false;

    nlohmann::json cancel;
    cancel["type"] = "cancelPreviewResource";
    cancel["nodeId"] = "amp-node";
    cancel["resourceIndex"] = 0;
    controller.HandleUIMessage(cancel.dump());

    const auto& active = controller.GetActivePreset();
    if (!active)
        return false;

    const auto* node = active->graph.FindNode("amp-node");
    if (!node || node->resources.empty())
        return false;

    return node->resources[0].resourceId == "steady-lib-id" && node->resources[0].filePath.empty();
}

} // namespace

int main()
{
    int passed = 0;
    int failed = 0;

    const auto run = [&](const std::string& name, bool ok) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (ok) ++passed; else ++failed;
    };

    run("NAM preview apply/cancel", TestPreviewApplyAndCancel("nam"));
    run("IR preview apply/cancel", TestPreviewApplyAndCancel("ir"));
    run("Preview resourceIndex targeting", TestPreviewHonorsResourceIndex());
    run("Library preview close reverts", TestLibraryPreviewCloseRevertsOriginal());
    run("Preview missing data is no-op", TestPreviewMissingDataNoMutation());
    run("Preview missing nodeId is no-op", TestPreviewMissingNodeIdNoMutation());
    run("Cancel without preview is no-op", TestCancelWithoutActivePreviewNoMutation());

    std::cout << "\nResource preview workflow tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
