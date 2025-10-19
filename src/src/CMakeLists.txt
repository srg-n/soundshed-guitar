set(NAMGUITAR_CORE_SRC
  NAMGuitarPlugin.h
  presets/PresetTypes.h
  dsp/NAMDSPManager.cpp
  dsp/NAMDSPManager.h
  dsp/IRManager.cpp
  dsp/IRManager.h
  models/ModelHasher.cpp
  models/ModelHasher.h
  ui/WebUIBridge.cpp
  ui/WebUIBridge.h
  util/FileSystem.cpp
  util/FileSystem.h
)

add_library(NAMGuitarFXCore STATIC ${NAMGUITAR_CORE_SRC})

set_target_properties(NAMGuitarFXCore PROPERTIES
  FOLDER NAMGuitarFX
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

target_compile_definitions(NAMGuitarFXCore
  PUBLIC
    IPLUG_EDITOR=1
    IPLUG_DSP=1
    WIN32
    _WIN32
    NOMINMAX
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
    IGRAPHICS_NANOVG
    IGRAPHICS_GL2
    IGRAPHICS_WEBVIEW
  JSON_SKIP_LIBRARY_VERSION_CHECK
)

# iPlug2 provides the plugin entry points and needs to be included before
# building platform specific targets. We only link against headers here and
# structure specific wrappers in a dedicated subdirectory.

target_include_directories(NAMGuitarFXCore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../config
  ${CMAKE_CURRENT_SOURCE_DIR}/../resources
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/presets
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

if(DEFINED iPlug2_SOURCE_DIR)
  target_include_directories(NAMGuitarFXCore
    PUBLIC
      ${iPlug2_SOURCE_DIR}/IPlug
  ${iPlug2_SOURCE_DIR}/IPlug/APP
      ${iPlug2_SOURCE_DIR}/IGraphics
      ${iPlug2_SOURCE_DIR}/IGraphics/Controls
      ${iPlug2_SOURCE_DIR}/IGraphics/Drawing
      ${iPlug2_SOURCE_DIR}/IGraphics/Platforms
      ${iPlug2_SOURCE_DIR}/IGraphics/Extras
      ${iPlug2_SOURCE_DIR}/WDL
      ${iPlug2_SOURCE_DIR}/WDL/libpng
      ${iPlug2_SOURCE_DIR}/WDL/zlib
      ${iPlug2_SOURCE_DIR}/IPlug/VST3
      ${iPlug2_SOURCE_DIR}/IPlug/Extras
    ${iPlug2_SOURCE_DIR}/IPlug/Extras/WebView
    ${iPlug2_SOURCE_DIR}/Dependencies/IPlug/RTAudio/include
    ${iPlug2_SOURCE_DIR}/Dependencies/IPlug/RTAudio
    ${iPlug2_SOURCE_DIR}/Dependencies/IPlug/RTMidi
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/NanoVG/src
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/NanoSVG/src
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/STB
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/yoga
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/yoga/yoga
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/glad_GL2/include
      ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/glad_GL2/src
  )
endif()

set(_vst3_sdk_hints "")
if(DEFINED VST3_SDK_ROOT)
  list(APPEND _vst3_sdk_hints "${VST3_SDK_ROOT}")
endif()
if(DEFINED ENV{VST3_SDK_ROOT})
  list(APPEND _vst3_sdk_hints "$ENV{VST3_SDK_ROOT}")
endif()
if(DEFINED iPlug2_SOURCE_DIR)
  list(APPEND _vst3_sdk_hints "${iPlug2_SOURCE_DIR}/Dependencies/IPlug/VST3_SDK")
endif()
list(APPEND _vst3_sdk_hints "${CMAKE_SOURCE_DIR}/_deps/iplug2-src/Dependencies/IPlug/VST3_SDK")

if(NOT VST3_SDK_ROOT)
  find_path(VST3_SDK_ROOT
    NAMES pluginterfaces/vst/ivstcontextmenu.h
    HINTS ${_vst3_sdk_hints}
    DOC "Root directory of the Steinberg VST3 SDK"
  )
endif()

if(NOT VST3_SDK_ROOT)
  message(FATAL_ERROR
    "VST3 SDK headers not found. Set VST3_SDK_ROOT to the extracted Steinberg VST3 SDK or place the SDK under ${iPlug2_SOURCE_DIR}/Dependencies/IPlug/VST3_SDK.")
endif()

target_include_directories(NAMGuitarFXCore PUBLIC ${VST3_SDK_ROOT})

if(DEFINED NeuralAmpModelerCore_SOURCE_DIR)
  target_include_directories(NAMGuitarFXCore
    PUBLIC
      ${NeuralAmpModelerCore_SOURCE_DIR}
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM
      ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies
      ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/eigen
      ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
  )

  target_sources(NAMGuitarFXCore
    PRIVATE
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/activations.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/convnet.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/dsp.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/get_dsp.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/lstm.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/util.cpp
      ${NeuralAmpModelerCore_SOURCE_DIR}/NAM/wavenet.cpp
  )
endif()

if(TARGET iplug2)
  target_link_libraries(NAMGuitarFXCore PUBLIC iplug2)
endif()

if(TARGET NeuralAmpModelerCore)
  target_link_libraries(NAMGuitarFXCore PUBLIC NeuralAmpModelerCore)
endif()

add_subdirectory(platform)
