# Audio Unit wrapper placeholder. macOS specific sources will be introduced
# during customization. This target is skipped automatically on non-Apple
# platforms.

add_library(NAMGuitarFX_AU MODULE NAMGuitarFX_AU.cpp)

target_link_libraries(NAMGuitarFX_AU PRIVATE NAMGuitarFX)

set_target_properties(NAMGuitarFX_AU PROPERTIES
  OUTPUT_NAME NAMGuitarFX
  FOLDER NAMGuitarFX/Formats
)
