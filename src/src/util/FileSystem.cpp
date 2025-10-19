#include "FileSystem.h"

#include <cstdlib>

namespace namguitar
{

std::filesystem::path FileSystem::ResolvePresetDirectory() const
{
  return std::filesystem::path{"presets"};
}

std::filesystem::path FileSystem::ResolveCacheDirectory() const
{
  return std::filesystem::path{"cache"};
}

std::optional<std::filesystem::path> FileSystem::EnsureDirectory(const std::filesystem::path& dir) const
{
  std::error_code ec;
  if (std::filesystem::create_directories(dir, ec) || std::filesystem::exists(dir))
  {
    return dir;
  }

  return std::nullopt;
}

} // namespace namguitar
