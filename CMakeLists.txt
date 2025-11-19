cmake_minimum_required(VERSION 3.16)

project(cmakeBuilder LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Concurrent)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/) 

include_directories(3rd)

add_executable(cmakeBuilder
  main.cpp
  cmakebuilder.cpp
  cmakebuilder.h
  cmakebuilder.ui
  res/ico.rc 
  config.h
  config.cpp
)

target_link_libraries(
cmakeBuilder PRIVATE 
Qt${QT_VERSION_MAJOR}::Widgets 
Qt${QT_VERSION_MAJOR}::Core
Qt${QT_VERSION_MAJOR}::Concurrent
)
set_target_properties(cmakeBuilder PROPERTIES WIN32_EXECUTABLE TRUE)
