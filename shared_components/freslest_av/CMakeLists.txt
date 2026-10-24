idf_component_register(SRCS "av_signal.c" "av_ownership.c" "av_engine.c" "av_lab.c"
 INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "${IDF_PATH}/components/esp_lcd/priv_include"
 REQUIRES esp_lcd esp_hw_support esp_driver_gpio esp_timer esp_mm esp_pm)
set_source_files_properties("av_signal.c" PROPERTIES COMPILE_OPTIONS -O2)
