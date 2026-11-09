FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.5.3
  GIT_SHALLOW TRUE
  SYSTEM)
FetchContent_MakeAvailable(doctest)

add_executable(sonos_tests
  test_main.cpp
  test_soap.cpp
  test_didl.cpp
  test_client.cpp
  test_controller.cpp
  test_ssdp.cpp
  test_logo.cpp
  test_update.cpp
  test_websocket.cpp
  test_hcu.cpp
  test_home_controller.cpp
)
target_compile_definitions(sonos_tests PRIVATE SONOS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(sonos_tests PRIVATE sonos_core sonos_ui sonos_platform_linux doctest::doctest sonos_warnings)

add_test(NAME sonos_tests COMMAND sonos_tests)
