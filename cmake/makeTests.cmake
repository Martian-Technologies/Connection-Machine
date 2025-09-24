if(CONNECTION_MACHINE_CODE_COVERAGE)
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		message(STATUS "Code coverage enabled")
		add_compile_options(--coverage -O0 -g)
		add_link_options(--coverage)
	elseif(MSVC)
		message(WARNING "Code coverage does nothing with MSVC. Use normal Tests build, and use opencpp_coverage.cmd")
	else()
		message(WARNING "Code coverage not working with: \"${CMAKE_CXX_COMPILER_ID}\"")
	endif()
endif()


# Google Testing
CPMAddPackage(
	NAME gtest
	GITHUB_REPOSITORY google/googletest
	GIT_TAG v1.17.0
	SOURCE_DIR "${EXTERNAL_DIR}/gtest"
	OPTIONS "gtest_force_shared_crt ON"
)

set(TEST_DIR "${CMAKE_SOURCE_DIR}/tests")
set(TEST_FILES)
file(GLOB_RECURSE TEST_FILES
	"${SOURCE_DIR}/backend/*.cpp"
	"${SOURCE_DIR}/computerAPI/*.cpp"
	"${SOURCE_DIR}/logging/*.cpp"
	"${SOURCE_DIR}/util/*.cpp"
	"${TEST_DIR}/*.cpp"
)

add_main_dependencies()

set(EXTERNAL_LINKS ${EXTERNAL_LINKS} gtest gtest_main)

message("EXTERNAL_LINKS: ${EXTERNAL_LINKS}")

if(APPLE)
	# Link CoreFoundation explicitly on macOS
	list(APPEND EXTERNAL_LINKS "-framework CoreFoundation")
endif()

add_executable(${PROJECT_NAME}_tests ${TEST_FILES})

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${SOURCE_DIR} ${TEST_DIR} "${EXTERNAL_DIR}/wasmtime")
target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${EXTERNAL_LINKS})

target_precompile_headers(${PROJECT_NAME}_tests PRIVATE "${SOURCE_DIR}/precompiled.h")
