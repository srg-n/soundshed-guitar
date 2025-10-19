# AAX wrapper placeholder. Actual SDK integration will be added later; this
# target is currently a stub for scaffolding purposes.

add_library(NAMGuitarFX_AAX MODULE NAMGuitarFX_AAX.cpp)

target_link_libraries(NAMGuitarFX_AAX PRIVATE NAMGuitarFX)

set_target_properties(NAMGuitarFX_AAX PROPERTIES
  OUTPUT_NAME NAMGuitarFX
  FOLDER NAMGuitarFX/Formats
)
