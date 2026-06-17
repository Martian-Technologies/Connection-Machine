# Find source files
file(GLOB_RECURSE PROJECT_SOURCES
	"${SOURCE_DIR}/*.cpp"
)
string(REGEX REPLACE "([][+.*^$(){}|\\\\])" "\\\\\\1" SOURCE_DIR_REGEX "${SOURCE_DIR}")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/gui\/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/gpu\/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/network\/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/environment\/blockRenderDataFeeder.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/main.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}\/app.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCSOURCE_DIR_REGEXE_DIR}\/util\/rectPacker.cpp")

# ===================================== CREATE APP EXECUTABLE ========================================

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_main_dependencies()

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LINKS})
target_precompile_headers(${PROJECT_NAME} PRIVATE "${SOURCE_DIR}/precompiled.h")

if (RUN_TRACY_PROFILER)
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES VULKAN_HPP_NO_EXCEPTIONS VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=0 "TRACY_PROFILER")
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES VULKAN_HPP_NO_EXCEPTIONS VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=0)
endif()
if (CONNECTION_MACHINE_TRY_CATCH)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "MAIN_TRY_CATCH")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE "PROJECT_VERSION=\"${PROJECT_VERSION}\"")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")
target_compile_definitions(${PROJECT_NAME} PRIVATE "CLI")

# Platform specific business after add_executable
if(APPLE) # MacOS

elseif (WIN32) # Windows
	if (CMAKE_BUILD_TYPE MATCHES Release) # If release build
		set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
	endif ()
endif()
