cmake_minimum_required(VERSION 3.22.1)
project(toastchat C)

# app/ -> android/ -> platform/ -> repo root
get_filename_component(TC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

set(TC_HOST "breadtoasting.com" CACHE STRING "server hostname")
set(TC_PORT "443"               CACHE STRING "server port")
option(TC_TLS      "speak wss:// via bundled mbedtls" ON)
option(TC_AUTONAME "skip the name screen (smoke tests only)" OFF)

# NativeActivity glue ships as SOURCE with the NDK, not as a prebuilt library.
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(toastchat SHARED
    ${TC_ROOT}/core/tc_sha1.c
    ${TC_ROOT}/core/tc_base64.c
    ${TC_ROOT}/core/tc_ws.c
    ${TC_ROOT}/core/tc_tls.c
    ${TC_ROOT}/core/tc_draw.c
    ${TC_ROOT}/core/tc_ui.c
    ${TC_ROOT}/core/tc_png.c
    ${TC_ROOT}/core/tc_send.c
    ${TC_ROOT}/core/tc_json.c
    ${TC_ROOT}/core/tc_imgdec.c
    ${TC_ROOT}/core/tc_recv.c
    ${TC_ROOT}/core/tc_keyboard.c
    ${TC_ROOT}/core/tc_lobby.c
    ${TC_ROOT}/core/tc_app.c
    ${TC_ROOT}/platform/android/tc_gesture.c
    ${TC_ROOT}/platform/android/plat_android.c
    ${TC_ROOT}/platform/android/main.c)

target_compile_definitions(toastchat PRIVATE
    TC_HOST="${TC_HOST}" TC_PORT=${TC_PORT})
target_compile_options(toastchat PRIVATE -std=gnu99 -Wall -Wextra)

if(TC_AUTONAME)
    target_compile_definitions(toastchat PRIVATE TC_AUTONAME)
endif()

if(TC_TLS)
    if(NOT EXISTS ${TC_ROOT}/third_party/mbedtls/CMakeLists.txt)
        message(FATAL_ERROR
            "third_party/mbedtls is missing - run tools/build-android.sh, "
            "which fetches it, or configure with -DTC_TLS=OFF")
    endif()
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${TC_ROOT}/third_party/mbedtls mbedtls EXCLUDE_FROM_ALL)
    target_compile_definitions(toastchat PRIVATE TC_HAVE_TLS TC_TLS=1)
    target_link_libraries(toastchat mbedtls mbedx509 mbedcrypto)
else()
    target_compile_definitions(toastchat PRIVATE TC_TLS=0)
endif()

target_link_libraries(toastchat native_app_glue android log m)

# ANativeActivity_onCreate lives in the glue and nothing in our code calls it,
# so without this the linker drops it and the activity dies at startup with
# "Unable to find native library entry point".
set_target_properties(toastchat PROPERTIES
    LINK_FLAGS "-u ANativeActivity_onCreate")
