#pragma once

#include <filesystem>
#include <string>

namespace namguitar
{
class ModelHasher
{
public:
  [[nodiscard]] std::string HashFile(const std::filesystem::path& filePath) const;
};
} // namespace namguitar
