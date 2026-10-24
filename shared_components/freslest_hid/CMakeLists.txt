idf_component_register(SRCS "freslest_hid.c" "freslest_hid_state.c" "freslest_hid_map.c" INCLUDE_DIRS "include" REQUIRES usb_host_hid usb esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)
