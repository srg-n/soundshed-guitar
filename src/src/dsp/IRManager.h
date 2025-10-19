#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace namguitar
{
class IRManager
{
public:
  IRManager() = default;

  bool LoadImpulseResponse(const std::filesystem::path& filePath, double targetSampleRate);
  [[nodiscard]] std::optional<std::filesystem::path> CurrentImpulseResponse() const;
  [[nodiscard]] const std::vector<float>& Impulse() const noexcept;
  [[nodiscard]] bool HasImpulse() const noexcept;

private:
  bool ParseWavFile(std::ifstream& stream, double targetSampleRate);
  bool ParseRiffHeader(std::ifstream& stream);
  bool ParseFmtChunk(std::ifstream& stream, std::uint16_t& audioFormat, std::uint16_t& channels,
                     std::uint32_t& sampleRate, std::uint16_t& bitsPerSample, std::uint16_t& blockAlign,
                     std::uint32_t chunkSize);
  bool ParseDataChunk(std::ifstream& stream, std::uint16_t bitsPerSample, std::uint16_t channels,
                      std::uint32_t dataSize, double targetSampleRate, std::uint32_t sourceSampleRate);

  std::optional<std::filesystem::path> mCurrentIR;
  std::vector<float> mImpulse;
  double mImpulseSampleRate = 0.0;
};
} // namespace namguitar
