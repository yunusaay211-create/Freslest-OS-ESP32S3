idf_component_register(SRCS "freslest_video.c" "video_pixels.c" "video_slots.c"
 INCLUDE_DIRS "include" REQUIRES freslest_av esp_mm esp_timer esp_driver_gpio nvs_flash)

# The waveform loop has a 16.7 ms field deadline; optimize this hot path.
set_source_files_properties("video_pixels.c" PROPERTIES COMPILE_OPTIONS -O2)
