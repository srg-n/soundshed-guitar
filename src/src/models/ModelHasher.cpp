#include "ModelHasher.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace namguitar
{

  std::string ModelHasher::HashFile(const std::filesystem::path &filePath) const
  {
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
    {
      return {};
    }

    constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    std::uint64_t hash = kFnvOffset;
    std::array<char, 4096> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
    {
      const std::streamsize bytesRead = input.gcount();
      for (std::streamsize i = 0; i < bytesRead; ++i)
      {
        hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(i)]);
        hash *= kFnvPrime;
      }
    }

    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
  }

} // namespace namguitar
