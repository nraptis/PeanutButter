cmake_minimum_required(VERSION 3.21)
project(PeanutButter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release profile: max optimization and dedicated artifact folder.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
  )
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT PEANUTBUTTER_IPO_SUPPORTED OUTPUT PEANUTBUTTER_IPO_ERROR)
if (PEANUTBUTTER_IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
  message(STATUS "IPO/LTO not supported: ${PEANUTBUTTER_IPO_ERROR}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")

# Active code only: files under src/Deprecated are intentionally excluded from normal builds.
set(PEANUTBUTTER_CORE_SOURCES
  src/Globals.cpp
  src/ConfigLoader.cpp
  src/FileCompare.cpp
  src/IO/FileReaderImplementation.cpp
  src/IO/FileWriterImplementation.cpp
  src/IO/FileReaderMock.cpp
  src/IO/FileWriterMock.cpp
  src/PeanutButterImplementation.cpp
  src/SnowStorm/SnowStormEngine.cpp
  src/LayeredEncryption/LayeredEncryptionImplementation.cpp
  src/LayeredEncryption/LayeredEncryptionMockEmpty.cpp
  src/SnowStorm/SnowStormUtils.cpp
  src/SnowStorm/SnowStormCountResult.cpp
  src/SnowStorm/SnowStormBundleSealerForward.cpp
  src/SnowStorm/SnowStormBundleSealerBackward.cpp
  src/SnowStorm/SnowStormBundlePackerForward.cpp
  src/SnowStorm/SnowStormBundlePackerBackward.cpp
  src/SandStorm/EncryptionLayer.cpp
  src/SandStorm/PasswordCiper.cpp
  src/SandStorm/ReverseCipher.cpp
  src/SandStorm/SplintCipher.cpp
  src/SandStorm/WeaveCipher.cpp
)

add_library(peanutbutter_core STATIC ${PEANUTBUTTER_CORE_SOURCES})
target_include_directories(peanutbutter_core PUBLIC src)

add_library(peanutbutter_core_tests STATIC ${PEANUTBUTTER_CORE_SOURCES})
target_include_directories(peanutbutter_core_tests PUBLIC src)
target_compile_definitions(peanutbutter_core_tests PRIVATE PEANUTBUTTER_UNIT_TEST_MODE=1)

add_executable(peanutbutter_runner src/main_runner.cpp)
target_link_libraries(peanutbutter_runner PRIVATE peanutbutter_core)

add_executable(recovery_runner src/recovery_runner.cpp)
target_link_libraries(recovery_runner PRIVATE peanutbutter_core)

enable_testing()
add_executable(peanutbutter_tests tests/snowstorm_engine_tests.cpp)
target_compile_definitions(peanutbutter_tests PRIVATE PEANUTBUTTER_UNIT_TEST_MODE=1)
target_link_libraries(peanutbutter_tests PRIVATE peanutbutter_core_tests)
add_test(NAME peanutbutter_tests COMMAND peanutbutter_tests)

add_executable(TestMockFileWriterAndReader tests/TestMockFileWriterAndReader.cpp)
target_compile_definitions(TestMockFileWriterAndReader PRIVATE PEANUTBUTTER_UNIT_TEST_MODE=1)
target_link_libraries(TestMockFileWriterAndReader PRIVATE peanutbutter_core_tests)
add_test(NAME TestMockFileWriterAndReader COMMAND TestMockFileWriterAndReader)

add_executable(Test100RealRunsNoEncryption tests/Test100RealRunsNoEncryption.cpp)
target_compile_definitions(Test100RealRunsNoEncryption PRIVATE PEANUTBUTTER_UNIT_TEST_MODE=1)
target_compile_definitions(Test100RealRunsNoEncryption PRIVATE PEANUTBUTTER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(Test100RealRunsNoEncryption PRIVATE peanutbutter_core_tests)
add_test(NAME Test100RealRunsNoEncryption COMMAND Test100RealRunsNoEncryption)

add_executable(Test100MockRunsNoEncryption tests/Test100MockRunsNoEncryption.cpp)
target_compile_definitions(Test100MockRunsNoEncryption PRIVATE PEANUTBUTTER_UNIT_TEST_MODE=1)
target_compile_definitions(Test100MockRunsNoEncryption PRIVATE PEANUTBUTTER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(Test100MockRunsNoEncryption PRIVATE peanutbutter_core_tests)
add_test(NAME Test100MockRunsNoEncryption COMMAND Test100MockRunsNoEncryption)

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
  add_executable(peanutbutter src/main.cpp)
  target_link_libraries(peanutbutter PRIVATE peanutbutter_core Qt6::Widgets)
else()
  message(WARNING "Qt6 Widgets not found. Building CLI app as peanutbutter. Install Qt6 to enable the GUI target.")
  add_executable(peanutbutter src/main_runner.cpp)
  target_link_libraries(peanutbutter PRIVATE peanutbutter_core)
endif()
