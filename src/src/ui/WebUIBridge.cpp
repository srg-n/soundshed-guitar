#include "WebUIBridge.h"

#include <iostream> // For std::cout

#include "IControls.h"
#include "IWebViewControl.h"

namespace namguitar
{

// Static member initialization
std::atomic<bool> DllPinner::sShuttingDown{false};

DllPinner::DllPinner()
{
#ifdef _WIN32
  // Pin the DLL to prevent it from being unloaded while WebView2 callbacks are active
  // This prevents crashes when WebView2 async callbacks execute after the plugin is "closed"
  HMODULE hModule = nullptr;
  if (GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&DllPinner::sShuttingDown),
        &hModule))
  {
    mPinnedModule = hModule;
    std::cout << "[WebUI] DLL pinned successfully" << std::endl;
  }
  else
  {
    std::cerr << "[WebUI] Failed to pin DLL, error: " << GetLastError() << std::endl;
  }
#endif
}

DllPinner::~DllPinner()
{
#ifdef _WIN32
  // Note: We intentionally do NOT unpin the DLL because WebView2 callbacks
  // might still be pending. The DLL will remain loaded until the process exits.
  // This is the safest approach for WebView2 in plugin environments.
  std::cout << "[WebUI] DllPinner destroyed (DLL remains pinned)" << std::endl;
#endif
}

namespace
{
std::string EscapeForJavaScriptString(const std::string& input)
{
  std::string escaped;
  escaped.reserve(input.size() + 16);
  for (char c : input)
  {
    switch (c)
    {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}
} // namespace

WebUIBridge::~WebUIBridge()
{
  std::cout << "[WebUI] WebUIBridge destructor called" << std::endl;
  
  // Signal that we're shutting down so WebView2 callbacks will bail out early
  DllPinner::BeginShutdown();
  
  // Invalidate the validity flag so callbacks know this object is being destroyed
  if (mValidityFlag) {
    mValidityFlag->store(false);
  }
  
  // Clear the initialized flag
  mInitialized.store(false);
  
  // Clear handlers to prevent any callbacks
  mHandler = nullptr;
  mLogger = nullptr;
  
  // Note: mWebView is owned by IGraphics and will be cleaned up by it
  // We just null our pointer
  mWebView = nullptr;
  
  std::cout << "[WebUI] WebUIBridge destructor completed" << std::endl;
}

void WebUIBridge::Initialize(iplug::igraphics::IGraphics& graphics, const std::filesystem::path& resourceRoot)
{
  using namespace iplug::igraphics;

  std::cout << "[WebUI] Initialize called" << std::endl;
  std::cout.flush();

  try {
    // Pin the DLL before creating WebView to prevent unload during async callbacks
    mDllPinner = std::make_unique<DllPinner>();
    
    const std::filesystem::path htmlPath = resourceRoot / "ui" / "index.html";
    IRECT bounds = graphics.GetBounds();

    std::cout << "[WebUI] Initializing WebView with bounds: " << bounds.W() << "x" << bounds.H() << std::endl;
    std::cout << "[WebUI] HTML path: " << htmlPath.generic_string() << std::endl;
    std::cout.flush();

    if (mLogger) {
      mLogger("Initializing WebView with bounds: " + std::to_string(bounds.W()) + "x" + std::to_string(bounds.H()));
      mLogger("HTML path: " + htmlPath.generic_string());
    }

    // Store htmlPath in a member to avoid capturing by value
    mHtmlPath = htmlPath;
    
    // Store a weak indicator that this object is valid
    // We use a shared_ptr to a bool that we can check in callbacks
    auto validityFlag = std::make_shared<std::atomic<bool>>(true);
    mValidityFlag = validityFlag;

    mWebView = new IWebViewControl(
      bounds,
      true,
      [this, validityFlag](IWebViewControl*) {
        // Check if we're shutting down or object is invalid before accessing any members
        if (!validityFlag || !validityFlag->load() || DllPinner::IsShuttingDown()) {
          return;
        }
        std::cout << "[WebUI] WebView ready callback triggered" << std::endl;
        std::cout.flush();
        if (mLogger) {
          mLogger("WebView ready callback triggered");
        }
        LoadWebContent(mHtmlPath);
      },
      [this, validityFlag](IWebViewControl*, const char* jsonMsg) {
        // Check if we're shutting down or object is invalid before accessing any members
        if (!validityFlag || !validityFlag->load() || DllPinner::IsShuttingDown()) {
          return;
        }
        // Queue incoming messages for processing on the main thread
        // This is called from the WebView2 callback which may be on a different thread
        try {
          EnqueueIncomingMessage(jsonMsg ? jsonMsg : "");
        } catch (...) {
          std::cerr << "[WebUI] Exception in message callback" << std::endl;
        }
      },
      true, // enable dev tools
      false);

    std::cout << "[WebUI] WebView control created, attaching to graphics" << std::endl;
    std::cout.flush();
    
    if (mLogger) {
      mLogger("WebView control created, attaching to graphics");
    }

    graphics.AttachControl(mWebView);
    mInitialized.store(true);
    
    std::cout << "[WebUI] Initialize completed successfully" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "[WebUI] Exception during Initialize: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "[WebUI] Unknown exception during Initialize" << std::endl;
  }
}

void WebUIBridge::RegisterMessageHandler(MessageHandler handler)
{
  mHandler = std::move(handler);
}

void WebUIBridge::RegisterLogHandler(LogHandler handler)
{
  mLogger = std::move(handler);
}

void WebUIBridge::EnqueueMessage(const std::string& message)
{
  std::lock_guard<std::mutex> lock(mQueueMutex);
  mPendingMessages.push(message);
}

void WebUIBridge::EnqueueIncomingMessage(const std::string& message)
{
  std::lock_guard<std::mutex> lock(mIncomingQueueMutex);
  mIncomingMessages.push(message);
}

void WebUIBridge::ProcessIncomingMessages()
{
  if (!mHandler || DllPinner::IsShuttingDown())
  {
    return;
  }

  std::queue<std::string> messages;
  {
    std::lock_guard<std::mutex> lock(mIncomingQueueMutex);
    std::swap(messages, mIncomingMessages);
  }

  while (!messages.empty() && !DllPinner::IsShuttingDown())
  {
    const auto& message = messages.front();
    mHandler(message);
    messages.pop();
  }
}

void WebUIBridge::PumpMessages()
{
  if (!mWebView || !mInitialized.load() || DllPinner::IsShuttingDown())
  {
    return;
  }

  // First, process incoming messages from the WebView on the main thread
  ProcessIncomingMessages();

  // Then, send outgoing messages to the WebView
  std::queue<std::string> messages;
  {
    std::lock_guard<std::mutex> lock(mQueueMutex);
    std::swap(messages, mPendingMessages);
  }

  while (!messages.empty())
  {
    const auto& message = messages.front();
    // Call IPlugReceiveData directly instead of postMessage since the UI expects it
    std::string script = "if (window.IPlugReceiveData) { window.IPlugReceiveData(\"" + EscapeForJavaScriptString(message) + "\"); }";
    mWebView->EvaluateJavaScript(script.c_str());
    if (mLogger)
    {
      mLogger("Sent UI message: " + message);
    }
    messages.pop();
  }
}

void WebUIBridge::LoadWebContent(const std::filesystem::path& htmlPath)
{
  std::cout << "[WebUI] LoadWebContent called" << std::endl;
  if (!mWebView)
  {
    if (mLogger) {
      mLogger("WebView is null, cannot load content");
    }
    return;
  }

  if (!std::filesystem::exists(htmlPath))
  {
    if (mLogger)
    {
      mLogger("Failed to open UI html at " + htmlPath.generic_string() + " - file does not exist");
    }
    return;
  }

  if (mLogger) {
    mLogger("HTML file found, loading: " + htmlPath.generic_string());
  }

  mHtmlPath = htmlPath;
  const std::string pathString = htmlPath.generic_string();
  mWebView->LoadFile(pathString.c_str());

  if (mLogger) {
    mLogger("LoadFile called successfully");
  }

  // Set up JavaScript bridge after a short delay to ensure content is loaded
  // Use a timer or just call it after LoadFile
  SetupJavaScriptBridge();
}

void WebUIBridge::SetupJavaScriptBridge()
{
  if (!mWebView)
  {
    return;
  }

  if (mLogger) {
    mLogger("Setting up JavaScript bridge");
  }

  // Set up the JavaScript bridge
  const std::string bridgeScript = R"(
    window.NAMBridge = {
      postMessage: function(message) {
        IPlugSendMsg(JSON.stringify(message));
      }
    };
  )";

  mWebView->EvaluateJavaScript(bridgeScript.c_str(), [this](const char* result) {
    if (mLogger) {
      mLogger("JavaScript bridge setup completed");
    }
  });
}

} // namespace namguitar
