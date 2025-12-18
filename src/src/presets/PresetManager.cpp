#include "PresetManager.h"

#include "PresetStorage.h"

namespace namguitar
{

  PresetManager::PresetManager()
      : mStorage(std::make_unique<PresetStorage>()), mServiceClient(std::make_shared<PresetServiceClient>())
  {
  }

  PresetManager::~PresetManager() = default;

  bool PresetManager::Serialize(iplug::IByteChunk &chunk) const
  {
    if (!mStorage)
    {
      return false;
    }

    return mStorage->Serialize(chunk);
  }

  int PresetManager::Unserialize(const iplug::IByteChunk &chunk, int startPos)
  {
    if (!mStorage)
    {
      return startPos;
    }

    return mStorage->Unserialize(chunk, startPos);
  }

  void PresetManager::SavePreset(const Preset &preset)
  {
    if (mStorage)
    {
      mStorage->SavePreset(preset);
    }
  }

  std::vector<Preset> PresetManager::ListPresets() const
  {
    if (!mStorage)
    {
      return {};
    }

    return mStorage->ListPresets();
  }

  void PresetManager::SearchRemotePresets(const PresetSearchRequest &request, PresetServiceClient::ResultCallback callback)
  {
    if (!mServiceClient)
    {
      callback({});
      return;
    }

    mServiceClient->SetBaseUrl(mRemoteBaseUrl);
    mServiceClient->SearchPresets(request, std::move(callback));
  }

  void PresetManager::DownloadRemotePreset(const std::string &presetId, PresetServiceClient::ResultCallback callback)
  {
    if (!mServiceClient)
    {
      callback({});
      return;
    }

    mServiceClient->SetBaseUrl(mRemoteBaseUrl);
    mServiceClient->DownloadPreset(presetId, std::move(callback));
  }

  void PresetManager::SetRemoteBaseUrl(std::string baseUrl)
  {
    mRemoteBaseUrl = std::move(baseUrl);
  }

} // namespace namguitar
