include_guard(GLOBAL)

include(GuitarfxIPP)

guitarfx_detect_ipp()

if(NOT GUITARFX_IPP_AVAILABLE)
    set(IPP_FOUND FALSE)
    return()
endif()

function(_guitarfx_ipp_import target_name base_name)
    if(TARGET ${target_name})
        return()
    endif()

    set(_candidates "")
    if(WIN32)
        list(APPEND _candidates
            "${IPP_LIB}/${base_name}mt.lib"
            "${IPP_LIB}/${base_name}.lib")
    else()
        list(APPEND _candidates
            "${IPP_LIB}/lib${base_name}.a"
            "${IPP_LIB}/lib${base_name}.so"
            "${IPP_LIB}/${base_name}.lib")
    endif()

    set(_imported_location "")
    foreach(_candidate IN LISTS _candidates)
        if(EXISTS "${_candidate}")
            set(_imported_location "${_candidate}")
            break()
        endif()
    endforeach()

    if(_imported_location STREQUAL "")
        set(IPP_FOUND FALSE PARENT_SCOPE)
        return()
    endif()

    add_library(${target_name} UNKNOWN IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${_imported_location}"
        INTERFACE_INCLUDE_DIRECTORIES "${IPP_INC}")
endfunction()

_guitarfx_ipp_import(IPP::ippcore ippcore)
_guitarfx_ipp_import(IPP::ipps ipps)
_guitarfx_ipp_import(IPP::ippi ippi)

if(TARGET IPP::ipps)
    set_property(TARGET IPP::ipps APPEND PROPERTY INTERFACE_LINK_LIBRARIES IPP::ippcore)
endif()
if(TARGET IPP::ippi)
    set_property(TARGET IPP::ippi APPEND PROPERTY INTERFACE_LINK_LIBRARIES "IPP::ipps;IPP::ippcore")
endif()

if(TARGET IPP::ippcore AND TARGET IPP::ipps AND TARGET IPP::ippi)
    set(IPP_FOUND TRUE)
else()
    set(IPP_FOUND FALSE)
endif()