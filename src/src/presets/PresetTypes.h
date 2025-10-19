#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace namguitar
{
struct PresetAttachment
{
  std::string type;
  std::filesystem::path filePath;
  std::string hash;
  std::string data; // optional inline payload (e.g. base64) supplied by the UI
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
} // namespace namguitar
