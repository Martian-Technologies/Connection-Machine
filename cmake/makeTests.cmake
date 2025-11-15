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
set(PROJECT_SOURCES)
file(GLOB_RECURSE PROJECT_SOURCES
	"${SOURCE_DIR}/*.cpp"
	"${TEST_DIR}/*.cpp"
)
string(REGEX REPLACE "([][+.*^$(){}|\\\\])" "\\\\\\1" SOURCE_DIR_REGEX "${SOURCE_DIR}")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}/cli/main.cpp$")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/main.cpp")

add_main_dependencies()

set(EXTERNAL_LINKS ${EXTERNAL_LINKS} gtest gtest_main)

message("EXTERNAL_LINKS: ${EXTERNAL_LINKS}")

if(APPLE)
	# Link CoreFoundation explicitly on macOS
	list(APPEND EXTERNAL_LINKS "-framework CoreFoundation")
endif()

add_executable(${PROJECT_NAME}_tests ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${SOURCE_DIR} ${TEST_DIR} "${EXTERNAL_DIR}/wasmtime")
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE "PROJECT_VERSION=\"${PROJECT_VERSION}\"")

target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${EXTERNAL_LINKS})

target_precompile_headers(${PROJECT_NAME}_tests PRIVATE "${SOURCE_DIR}/precompiled.h")
