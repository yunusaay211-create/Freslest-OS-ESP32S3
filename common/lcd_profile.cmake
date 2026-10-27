# Included after idf_component_register in both firmware projects.
set(FRESLEST_LCD_MODEL "9488" CACHE STRING "LCD controller: 9488 or 9341")
set_property(CACHE FRESLEST_LCD_MODEL PROPERTY STRINGS 9488 9341)
if(NOT FRESLEST_LCD_MODEL MATCHES "^(9488|9341)$")
    message(FATAL_ERROR "FRESLEST_LCD_MODEL must be 9488 or 9341")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE FRESLEST_LCD_MODEL=${FRESLEST_LCD_MODEL})
