cmake_minimum_required(VERSION 3.21)
project(PeanutButter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PEANUTBUTTER_CORE_SOURCES
  src/Globals.cpp
  src/ConfigLoader.cpp
  src/SnowStorm/SnowStorm.cpp
  src/SnowStorm/SnowStormUtils.cpp
  src/SnowStorm/SnowStormCountResult.cpp
  src/SnowStorm/SnowStormBundleWriter.cpp
  src/SnowStorm/SnowStormBundleReader.cpp
  src/SandStorm/SandStorm.cpp
  src/SandStorm/TaylorCipher.cpp
)

add_library(peanutbutter_core STATIC ${PEANUTBUTTER_CORE_SOURCES})
target_include_directories(peanutbutter_core PUBLIC src)

add_executable(peanutbutter_runner src/main_runner.cpp)
target_link_libraries(peanutbutter_runner PRIVATE peanutbutter_core)

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
  add_executable(peanutbutter src/main.cpp)
  target_link_libraries(peanutbutter PRIVATE peanutbutter_core Qt6::Widgets)
else()
  message(WARNING "Qt6 Widgets not found. Building CLI app as peanutbutter. Install Qt6 to enable the GUI target.")
  add_executable(peanutbutter src/main_runner.cpp)
  target_link_libraries(peanutbutter PRIVATE peanutbutter_core)
endif()
