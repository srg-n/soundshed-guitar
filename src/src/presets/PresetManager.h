#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "network/PresetServiceClient.h"

namespace iplug
{
  class IByteChunk;
}

namespace namguitar
{
  struct Preset;
  class PresetStorage;

  class PresetManager
  {
  public:
    PresetManager();
    ~PresetManager();

    bool Serialize(iplug::IByteChunk &chunk) const;
    int Unserialize(const iplug::IByteChunk &chunk, int startPos);

    void SavePreset(const Preset &preset);
    [[nodiscard]] std::vector<Preset> ListPresets() const;
    void SearchRemotePresets(const PresetSearchRequest &request, PresetServiceClient::ResultCallback callback);
    void DownloadRemotePreset(const std::string &presetId, PresetServiceClient::ResultCallback callback);
    void SetRemoteBaseUrl(std::string baseUrl);

  private:
    std::unique_ptr<PresetStorage> mStorage;
    std::shared_ptr<PresetServiceClient> mServiceClient;
    std::string mRemoteBaseUrl;
  };
} // namespace namguitar
