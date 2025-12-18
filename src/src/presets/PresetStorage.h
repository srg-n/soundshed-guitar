#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/ModelHasher.h"
#include "presets/PresetTypes.h"
#include "util/FileSystem.h"

namespace iplug
{
  class IByteChunk;
}

namespace namguitar
{
  class PresetStorage
  {
  public:
    PresetStorage();
    ~PresetStorage();

    bool Serialize(iplug::IByteChunk &chunk) const;
    int Unserialize(const iplug::IByteChunk &chunk, int startPos);

    void SavePreset(const Preset &preset);
    [[nodiscard]] std::vector<Preset> ListPresets() const;
    [[nodiscard]] std::optional<Preset> FindPreset(const std::string &id) const;

  private:
    void PersistToDisk() const;
    void LoadFromDisk();
    void EnsureDefaultPresets();

    std::vector<Preset> mPresets;
    FileSystem mFileSystem;
    ModelHasher mHasher;
    std::filesystem::path mPresetFile;
  };
} // namespace namguitar
