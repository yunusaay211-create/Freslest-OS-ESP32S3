file(GLOB DOOM_SOURCES "../components/prboom/*.c")
file(GLOB TABLE_SOURCES "../components/prboom-wad-tables/*.c")
set(PLATFORM "../components/prboom-esp32-compat")
idf_component_register(
    SRCS "app_main.c" ${DOOM_SOURCES} ${TABLE_SOURCES}
         "${PLATFORM}/i_main.c" "${PLATFORM}/i_system.c"
         "${PLATFORM}/i_video.c" "${PLATFORM}/i_sound.c"
         "${PLATFORM}/i_network.c" "${PLATFORM}/freslest_save.c" "${PLATFORM}/gamepad.c"
         "${PLATFORM}/doom_vga_frame.c" "${PLATFORM}/spi_lcd.c" "${PLATFORM}/freslest_settings.c"
         "${PLATFORM}/os_launcher.c" "${PLATFORM}/usb_doom.c"
    INCLUDE_DIRS "." "compat" "../components/prboom/include"
         "../components/prboom-wad-tables/include" "${PLATFORM}/include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_uart esp_driver_ledc esp_partition esp_timer esp_psram spi_flash nvs_flash app_update freslest_ota freslest_hid freslest_video)
target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_CONFIG_H)
include("${CMAKE_CURRENT_LIST_DIR}/../../common/lcd_profile.cmake")
# Upstream is pre-C99-era C and depends on common-symbol and wrapping math semantics.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fcommon
    -fwrapv
    -fno-strict-aliasing
    -Wno-error=all
    -Wno-error=extra
    -Wno-pointer-sign
)
# Keep legacy engine diagnostics visible without treating these as fatal.
set_source_files_properties(${DOOM_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-Wno-error=char-subscripts;-Wno-error=duplicate-decl-specifier;-Wno-error=format-overflow;-Wno-error=maybe-uninitialized;-Wno-error=misleading-indentation;-Wno-error=unused-but-set-parameter;-Wno-error=unused-const-variable;-Wno-error=unused-value;-Wno-error=nonnull;-Wno-error=parentheses;-Wno-error=address;-Wno-error=dangling-else;-Wno-error=sizeof-pointer-div")

# Compact non-critical code. Retain -O2 for hot rendering and physics.
foreach(source r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things p_tick p_map p_maputl p_mobj p_enemy p_sight p_user p_pspr)
 set_property(SOURCE "../components/prboom/${source}.c" APPEND PROPERTY COMPILE_OPTIONS -O2)
endforeach()
