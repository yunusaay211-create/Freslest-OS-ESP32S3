idf_component_register(SRCS "freslest_video.c" "vga_pixels.c" "vga_doom.c"
 INCLUDE_DIRS "include" REQUIRES esp_lcd esp_mm esp_timer esp_driver_gpio nvs_flash)
set_source_files_properties("vga_pixels.c" "vga_doom.c" PROPERTIES COMPILE_OPTIONS -O2)

# LCD builds retain menu-controlled output; all new VGA output uses 25.2 MHz.
set(FRESLEST_VGA_PROFILE "lcd" CACHE STRING "Video startup profile: lcd or vga")
set_property(CACHE FRESLEST_VGA_PROFILE PROPERTY STRINGS lcd vga)
if(FRESLEST_VGA_PROFILE STREQUAL "vga")
 target_compile_definitions(${COMPONENT_LIB} PRIVATE FRESLEST_VGA_PCLK_HZ=25200000 FRESLEST_VGA_PRESET_ID=252)
elseif(NOT FRESLEST_VGA_PROFILE STREQUAL "lcd")
 message(FATAL_ERROR "Invalid FRESLEST_VGA_PROFILE '${FRESLEST_VGA_PROFILE}'; choose lcd or vga")
endif()
