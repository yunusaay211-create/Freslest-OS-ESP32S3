file(GLOB_RECURSE srcs "src/*.c")
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "src" REQUIRES esp_psram)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -Wno-format -Wno-unused-variable -Wno-unused-function -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -fcommon -fno-strict-aliasing)
