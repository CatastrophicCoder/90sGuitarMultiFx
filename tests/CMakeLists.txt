include(Catch)

# Links only the engine: proves the signal chain builds and runs without JUCE or a plugin host.
add_executable(a5_engine_tests
    Unit/A5ProcessorTests.cpp
    Unit/SmoothingTests.cpp)

target_link_libraries(a5_engine_tests PRIVATE a5_engine Catch2::Catch2WithMain)
catch_discover_tests(a5_engine_tests TEST_PREFIX "engine: ")

# Drives the real AudioProcessor through JUCE's API, as a host would, by linking the plugin's
# shared-code archive, which already contains the compiled JUCE modules. Linking the modules again
# would compile a second copy, so only their include paths and definitions are borrowed.
add_executable(a5_plugin_tests
    Integration/PluginProcessorTests.cpp)

target_include_directories(a5_plugin_tests PRIVATE
    $<TARGET_PROPERTY:A5Plugin,INCLUDE_DIRECTORIES>)
target_compile_definitions(a5_plugin_tests PRIVATE
    $<TARGET_PROPERTY:A5Plugin,COMPILE_DEFINITIONS>)
target_link_libraries(a5_plugin_tests PRIVATE A5Plugin Catch2::Catch2WithMain)
catch_discover_tests(a5_plugin_tests TEST_PREFIX "plugin: ")
