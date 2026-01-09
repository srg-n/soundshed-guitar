#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace iplug
{
namespace igraphics
{
class IGraphics;
class IWebViewControl;
} // namespace igraphics
} // namespace iplug

namespace guitarfx
{

// Helper to pin the DLL while WebView2 callbacks might be active
class DllPinner
{
public:
  DllPinner();
  ~DllPinner();
  DllPinner(const DllPinner&) = delete;
  DllPinner& operator=(const DllPinner&) = delete;
  
  static bool IsShuttingDown() { return sShuttingDown.load(); }
  static void BeginShutdown() { sShuttingDown.store(true); }
  
private:
  static std::atomic<bool> sShuttingDown;
#ifdef _WIN32
  HMODULE mPinnedModule = nullptr;
#endif
};

class WebUIBridge
{
public:
  using MessageHandler = std::function<void(const std::string&)>;
  using LogHandler = std::function<void(const std::string&)>;

  ~WebUIBridge();

  void Initialize(iplug::igraphics::IGraphics& graphics, const std::filesystem::path& resourceRoot);
  void RegisterMessageHandler(MessageHandler handler);
  void RegisterLogHandler(LogHandler handler);
  void EnqueueMessage(const std::string& message);
  void PumpMessages();

private:
  void LoadWebContent(const std::filesystem::path& htmlPath);
  void SetupJavaScriptBridge();
  void EnqueueIncomingMessage(const std::string& message);
  void ProcessIncomingMessages();

  iplug::igraphics::IWebViewControl* mWebView = nullptr;
  std::mutex mQueueMutex;
  std::queue<std::string> mPendingMessages;
  
  // Separate queue for incoming messages from WebView (thread-safe)
  std::mutex mIncomingQueueMutex;
  std::queue<std::string> mIncomingMessages;
  
  MessageHandler mHandler;
  LogHandler mLogger;
  std::filesystem::path mHtmlPath;
  
  // Prevent DLL unload while WebView2 callbacks might be active
  std::unique_ptr<DllPinner> mDllPinner;
  std::atomic<bool> mInitialized{false};
  
  // Shared validity flag for checking if object is still valid in callbacks
  std::shared_ptr<std::atomic<bool>> mValidityFlag;
};
} // namespace guitarfx
