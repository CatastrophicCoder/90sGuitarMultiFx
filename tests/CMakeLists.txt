include(Catch)

# Links only the engine: proves the signal chain builds and runs without JUCE or a plugin host.
add_executable(fivea_engine_tests
    Common/WindowsErrorReporting.cpp
    Unit/AllocationGuard.cpp
    Unit/AllocationTests.cpp
    Unit/BypassCrossfadeTests.cpp
    Unit/ChainTests.cpp
    Unit/CompressorTests.cpp
    Unit/DenormalGuardTests.cpp
    Unit/DriveTests.cpp
    Unit/FactoryProgramsTests.cpp
    Unit/FiveAProcessorTests.cpp
    Unit/ModulationTests.cpp
    Unit/NoiseReductionTests.cpp
    Unit/OversamplerTests.cpp
    Unit/ProgramBankTests.cpp
    Unit/SmoothingTests.cpp
    Unit/StepMappingTests.cpp
    Unit/ThreeBandEqTests.cpp
    Unit/TimeEffectsTests.cpp)

target_link_libraries(fivea_engine_tests PRIVATE fivea_engine Catch2::Catch2WithMain)
# A per-test limit, so a hang (or a test far too slow for a checked Debug build) fails visibly
# instead of keeping a CI job busy for hours.
catch_discover_tests(fivea_engine_tests TEST_PREFIX "engine: " PROPERTIES TIMEOUT 300)

# Drives the real AudioProcessor through JUCE's API, as a host would, by linking the plugin's
# shared-code archive, which already contains the compiled JUCE modules. Linking the modules again
# would compile a second copy, so only their include paths and definitions are borrowed.
add_executable(fivea_plugin_tests
    Common/WindowsErrorReporting.cpp
    Integration/PanelTests.cpp
    Integration/PluginProcessorTests.cpp)

target_include_directories(fivea_plugin_tests PRIVATE
    $<TARGET_PROPERTY:FiveAPlugin,INCLUDE_DIRECTORIES>)
target_compile_definitions(fivea_plugin_tests PRIVATE
    $<TARGET_PROPERTY:FiveAPlugin,COMPILE_DEFINITIONS>)
target_link_libraries(fivea_plugin_tests PRIVATE FiveAPlugin Catch2::Catch2WithMain)
catch_discover_tests(fivea_plugin_tests TEST_PREFIX "plugin: " PROPERTIES TIMEOUT 300)
