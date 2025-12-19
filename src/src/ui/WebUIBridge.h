#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

namespace iplug
{
namespace igraphics
{
class IGraphics;
class IWebViewControl;
} // namespace igraphics
} // namespace iplug

namespace namguitar
{
class WebUIBridge
{
public:
  using MessageHandler = std::function<void(const std::string&)>;
  using LogHandler = std::function<void(const std::string&)>;

  void Initialize(iplug::igraphics::IGraphics& graphics, const std::filesystem::path& resourceRoot);
  void RegisterMessageHandler(MessageHandler handler);
  void RegisterLogHandler(LogHandler handler);
  void EnqueueMessage(const std::string& message);
  void PumpMessages();

private:
  void LoadWebContent(const std::filesystem::path& htmlPath);
  void SetupJavaScriptBridge();

  iplug::igraphics::IWebViewControl* mWebView = nullptr;
  std::mutex mQueueMutex;
  std::queue<std::string> mPendingMessages;
  MessageHandler mHandler;
  LogHandler mLogger;
  std::filesystem::path mHtmlPath;
};
} // namespace namguitar
