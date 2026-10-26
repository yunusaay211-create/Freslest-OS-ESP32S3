# Includes the Files USB storage service alongside the other OS apps.
file(GLOB_RECURSE ui_srcs "../freslest/*.cpp")
# Ignore a removed Touch app if a previous package was extracted over this one.
list(FILTER ui_srcs EXCLUDE REGEX "/app_touch\\.cpp$")
idf_component_register(SRCS "entry.cpp" ${ui_srcs} "../../common/usb_update/freslest_update.c" "../../common/usb_update/freslest_update_package.c" INCLUDE_DIRS "../../common/usb_update" "../freslest" "../freslest/src/freslest" REQUIRES arduino arduino_tinyusb LovyanGFX TJpgDecoder nofrendo joltwallet__littlefs app_update esp_http_client bt esp_hid esp_timer esp_driver_ledc freslest_ota freslest_hid freslest_video usb_host_msc json mbedtls bootloader_support)
target_compile_definitions(${COMPONENT_LIB} PUBLIC BOARD_HAS_PSRAM=1)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/lcd_profile.cmake")

# Keep per-function stack budgets for the OS main-task audit.
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)

# Keep room for both LCD profiles in the existing A/B slots. These cold menu
# paths favour code size; gameplay and display transfer optimization is retained.
set_source_files_properties(
    "../freslest/src/freslest/settings_pages.cpp"
    "../freslest/src/freslest/menus_sub.cpp"
    PROPERTIES COMPILE_OPTIONS "-Oz")
