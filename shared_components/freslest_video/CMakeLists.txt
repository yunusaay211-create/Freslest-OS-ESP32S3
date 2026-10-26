idf_component_register(SRCS "freslest_video.c" "vga_pixels.c"
 INCLUDE_DIRS "include" REQUIRES esp_lcd esp_mm esp_timer esp_driver_gpio nvs_flash)
set_source_files_properties("vga_pixels.c" PROPERTIES COMPILE_OPTIONS -O2)
