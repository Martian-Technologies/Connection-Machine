# ======================================== CMAKE SETUP ========================================
cmake_minimum_required(VERSION 3.10)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake)

if(APPLE)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/apple.cmake)
	PreProjectAppleSetup()
endif()

set(VERBOSE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

# generate compile commands json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set c++ standards as global variables
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ======================================== PROJECT SETUP ========================================
set(BUILD_SHARED_LIBS OFF) # Don't build shared libs
project(Connection_Machine VERSION 0.11.0)
if (APPLE)
	set(APP_NAME "Connection Machine") # name of the bundle
else()
	set(APP_NAME ${PROJECT_NAME})
endif()

# Set directories
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/external")
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")

option(CONNECTION_MACHINE_BUILD_APP "Build Connection Machine App" ON)
option(CONNECTION_MACHINE_BUILD_CLI_APP "Build Connection Machine App" ON)
option(CONNECTION_MACHINE_DISTRIBUTE_APP "Distribute App" OFF)
option(CONNECTION_MACHINE_BUILD_TESTS "Build Connection Machine Tests" OFF)
option(CONNECTION_MACHINE_CODE_COVERAGE "Enable code coverage reporting" OFF)
option(RUN_TRACY_PROFILER "Enable runtime profiler" OFF)

if(CONNECTION_MACHINE_BUILD_CLI_APP AND CONNECTION_MACHINE_BUILD_APP)
	message("Do not build both cli and non cli app at once")
	set(CONNECTION_MACHINE_BUILD_CLI_APP OFF)
endif()

if(CONNECTION_MACHINE_BUILD_CLI_APP)
	list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/gui\/.*")
	list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/gpu\/.*")
	list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/environment\/blockRenderDataFeeder.cpp")
	list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/main.cpp")
	list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/app.cpp")
endif()

if(MSVC)
	add_compile_options(/W1)
endif()

if (CONNECTION_MACHINE_BUILD_APP)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/makeApp.cmake)
elseif(CONNECTION_MACHINE_BUILD_CLI_APP)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/makeCLI.cmake)
elseif(CONNECTION_MACHINE_BUILD_TESTS)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/makeTests.cmake)
endif()
