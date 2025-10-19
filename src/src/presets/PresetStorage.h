#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "models/ModelHasher.h"
#include "util/FileSystem.h"

namespace iplug
{
class IByteChunk;
}

namespace namguitar
{
struct PresetAttachment
{
  std::string type;
  std::filesystem::path filePath;
  std::string hash;
};

struct PresetParameter
{
  std::string id;
  double value = 0.0;
};

struct Preset
{
  std::string id;
  std::string name;
  std::string category;
  std::string description;
  std::string namModelId;
  std::string irId;
  std::vector<std::string> fxChain;
  std::vector<PresetAttachment> attachments;
  std::vector<PresetParameter> parameters;
};

class PresetStorage
{
public:
  PresetStorage();
  ~PresetStorage();

  bool Serialize(iplug::IByteChunk& chunk) const;
  int Unserialize(const iplug::IByteChunk& chunk, int startPos);

  void SavePreset(const Preset& preset);
  [[nodiscard]] std::vector<Preset> ListPresets() const;
  [[nodiscard]] std::optional<Preset> FindPreset(const std::string& id) const;

private:
  void PersistToDisk() const;
  void LoadFromDisk();

  std::vector<Preset> mPresets;
  FileSystem mFileSystem;
  ModelHasher mHasher;
  std::filesystem::path mPresetFile;
};
} // namespace namguitar
