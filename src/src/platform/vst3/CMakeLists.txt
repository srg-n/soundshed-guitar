# VST3 wrapper target for NAMGuitarFX plugin

if(NOT DEFINED iPlug2_SOURCE_DIR)
  message(FATAL_ERROR "iPlug2 source directory is not available; ensure dependencies are fetched before enabling the VST3 target.")
endif()

# VST3 SDK source files
set(_vst3_sdk_sources
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/baseiids.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/fbuffer.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/fdebug.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/fobject.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/fstreamer.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/fstring.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source/updatehandler.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/thread/source/flock.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/pluginterfaces/base/conststringtable.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/pluginterfaces/base/coreiids.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/pluginterfaces/base/funknown.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/pluginterfaces/base/ustring.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/common/commoniids.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/common/memorystream.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/common/pluginview.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstbus.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstcomponent.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstcomponentbase.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstinitiids.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstparameters.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/vst/vstsinglecomponenteffect.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/main/pluginfactory.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source/main/dllmain.cpp
)

# iPlug2 core sources
set(_iplug2_core_sources
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugAPIBase.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugParameter.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugPaths.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugPluginBase.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugProcessor.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/IPlugTimer.cpp
)

# iPlug2 VST3-specific sources
set(_iplug2_vst3_sources
  ${iPlug2_SOURCE_DIR}/IPlug/VST3/IPlugVST3.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/VST3/IPlugVST3_ProcessorBase.cpp
)

# iPlug2 IGraphics sources
set(_igraphics_sources
  ${iPlug2_SOURCE_DIR}/IGraphics/IControl.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/IGraphics.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/IGraphicsEditorDelegate.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/Platforms/IGraphicsWin.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/Controls/IControls.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/Controls/IPopupMenuControl.cpp
  ${iPlug2_SOURCE_DIR}/IGraphics/Controls/ITextEntryControl.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/Extras/WebView/IPlugWebViewEditorDelegate.cpp
  ${iPlug2_SOURCE_DIR}/IPlug/Extras/WebView/IPlugWebView_win.cpp
)

set(_nam_vst3_sources
  NAMGuitarFX_VST3.cpp
  ../../NAMGuitarPlugin.cpp
  ${_vst3_sdk_sources}
  ${_iplug2_core_sources}
  ${_iplug2_vst3_sources}
  ${_igraphics_sources}
)

add_library(NAMGuitarFX_VST3 MODULE ${_nam_vst3_sources})

target_compile_definitions(NAMGuitarFX_VST3 PRIVATE VST3_API)

# Include all paths needed for compiling NAMGuitarPlugin.cpp with VST3_API
target_include_directories(NAMGuitarFX_VST3 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../config
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../resources
  ${iPlug2_SOURCE_DIR}/IPlug
  ${iPlug2_SOURCE_DIR}/IPlug/APP
  ${iPlug2_SOURCE_DIR}/IPlug/Extras
  ${iPlug2_SOURCE_DIR}/IPlug/Extras/WebView
  ${iPlug2_SOURCE_DIR}/IPlug/VST3
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/source
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/base/thread/include
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/pluginterfaces
  ${iPlug2_SOURCE_DIR}/IPlug/VST3_SDK/public.sdk/source
  ${iPlug2_SOURCE_DIR}/IGraphics
  ${iPlug2_SOURCE_DIR}/IGraphics/Controls
  ${iPlug2_SOURCE_DIR}/IGraphics/Drawing
  ${iPlug2_SOURCE_DIR}/IGraphics/Platforms
  ${iPlug2_SOURCE_DIR}/WDL
  ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/STB
  ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/NanoSVG/src
  ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/NanoVG/src
  ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/glad_GL3/include
  ${iPlug2_SOURCE_DIR}/Dependencies/IGraphics/glad_GL3/src
  ${NeuralAmpModelerCore_SOURCE_DIR}
  ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
)

target_link_libraries(NAMGuitarFX_VST3
  PRIVATE
    NAMGuitarFXCore
    WebView2::Loader
    wil::headers
    shlwapi
    comctl32
    ole32
    oleaut32
    uuid
    shell32
    user32
    gdi32
    advapi32
    wininet
)

set_target_properties(NAMGuitarFX_VST3 PROPERTIES
  OUTPUT_NAME NAMGuitarFX
  FOLDER NAMGuitarFX/Formats
)
