cmake_minimum_required(VERSION 3.25)
project(sonos_display LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(SONOS_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

# --- Third-party dependencies -------------------------------------------------

# LVGL: configured through our own lv_conf.h, examples/demos/thorvg not needed
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.2.2
  GIT_SHALLOW TRUE
  SYSTEM)

set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinyxml2
  GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
  GIT_TAG 11.0.0
  GIT_SHALLOW TRUE
  SYSTEM)

# JSON for the Homematic IP Connect API (header-only, also used on the ESP32)
FetchContent_Declare(ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
  SYSTEM)

FetchContent_MakeAvailable(lvgl tinyxml2 ArduinoJson)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# LVGL's SDL driver includes <SDL2/SDL.h>
target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)

# --- Warnings for our own code only ------------------------------------------

add_library(sonos_warnings INTERFACE)
target_compile_options(sonos_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion>)

# --- Portable core ------------------------------------------------------------

# Sonos protocol + app logic: no LVGL, testable on its own
add_library(sonos_core STATIC
  core/platform/Log.cpp
  core/sonos/Types.cpp
  core/sonos/XmlUtil.cpp
  core/sonos/SoapBuilder.cpp
  core/sonos/SoapParser.cpp
  core/sonos/DidlParser.cpp
  core/sonos/DeviceDescription.cpp
  core/sonos/SonosClient.cpp
  core/sonos/Ssdp.cpp
  core/sonos/LogoUrl.cpp
  core/app/Commands.cpp
  core/app/SonosService.cpp
  core/app/AppController.cpp
  core/app/UpdateManifest.cpp
  core/hcu/WebSocketClient.cpp
  core/hcu/SocketHttp.cpp
  core/hcu/HcuClient.cpp
  core/home/HomeService.cpp
  core/home/HomeController.cpp
)
target_include_directories(sonos_core PUBLIC core)
target_link_libraries(sonos_core PUBLIC tinyxml2::tinyxml2 ArduinoJson PRIVATE sonos_warnings)

# UI font: LVGL's Montserrat-Medium.ttf embedded as C array, rendered by Tiny TTF
set(SONOS_FONT_TTF ${lvgl_SOURCE_DIR}/scripts/built_in_font/Montserrat-Medium.ttf)
set(SONOS_FONT_C ${CMAKE_CURRENT_BINARY_DIR}/generated/montserrat_medium_ttf.c)
add_custom_command(
  OUTPUT ${SONOS_FONT_C}
  COMMAND ${CMAKE_COMMAND} -DINPUT=${SONOS_FONT_TTF} -DOUTPUT=${SONOS_FONT_C}
          -DSYMBOL=montserrat_medium_ttf -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFile.cmake
  DEPENDS ${SONOS_FONT_TTF} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFile.cmake
  COMMENT "Embedding Montserrat-Medium.ttf")

# LVGL user interface
add_library(sonos_ui STATIC
  core/ui/Fonts.cpp
  core/ui/Styles.cpp
  core/ui/MainScreen.cpp
  core/ui/HomePage.cpp
  core/ui/LogoDecoder.cpp
  ${SONOS_FONT_C}
)
target_link_libraries(sonos_ui PUBLIC sonos_core lvgl PRIVATE sonos_warnings)

# --- Linux platform -----------------------------------------------------------

add_library(sonos_platform_linux STATIC
  platform_linux/LinuxPlatform.cpp
  platform_linux/CurlHttpClient.cpp
  platform_linux/PosixSsdpDiscovery.cpp
  platform_linux/MockSonos.cpp
  platform_linux/MockHome.cpp
  platform_linux/OpenSslSocket.cpp
)
target_include_directories(sonos_platform_linux PUBLIC platform_linux)
target_link_libraries(sonos_platform_linux
  PUBLIC sonos_core CURL::libcurl OpenSSL::SSL Threads::Threads PRIVATE sonos_warnings)

add_executable(sonos_display platform_linux/main.cpp)
target_link_libraries(sonos_display PRIVATE sonos_ui sonos_platform_linux sonos_warnings)

# --- Tests --------------------------------------------------------------------

if(SONOS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
