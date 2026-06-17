include(cmake/CPM.cmake)
include(ExternalProject)

function(add_main_dependencies)
	message("adding main dependencies")

	if (APPLE)
		find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
		list(APPEND EXTERNAL_LINKS ${COREFOUNDATION_FRAMEWORK})
	endif()

	# cfgpath
	CPMAddPackage(
		NAME cfgpath
		GITHUB_REPOSITORY sisyffe/cfgpath
		GIT_TAG 4a48955f894a5d1ef292230f33a9e6caacfed572
		EXCLUDE_FROM_ALL YES
		DOWNLOAD_ONLY YES
		SOURCE_DIR "${EXTERNAL_DIR}/cfgpath"
	)
	if (WIN32)
		file(REMOVE "${EXTERNAL_DIR}/cfgpath/shlobj.h")
	endif()
	# add_library(cfgpath INTERFACE "${EXTERNAL_DIR}/cfgpath/cfgpath.h")
	add_library(cfgpath INTERFACE)
	target_include_directories(cfgpath INTERFACE "${EXTERNAL_DIR}/cfgpath")
	list(APPEND EXTERNAL_LINKS cfgpath)

	# JSON
	CPMAddPackage(
		NAME nlohmann_json
		GITHUB_REPOSITORY nlohmann/json
		GIT_TAG v3.12.0
		OPTIONS
			"JSON_BuildTests OFF"
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/json"
	)
	list(APPEND EXTERNAL_LINKS nlohmann_json)

	# brotli
	CPMAddPackage(
		NAME brotli
		GITHUB_REPOSITORY google/brotli
		GIT_TAG v1.2.0
		EXCLUDE_FROM_ALL YES
		OPTIONS
			"BUILD_SHARED_LIBS OFF"
			"BROTLI_BUILD_TOOLS OFF"
		SOURCE_DIR "${EXTERNAL_DIR}/brotli"
	)
	if (brotli_ADDED)
		add_library(Brotli::common ALIAS brotlicommon)
		add_library(Brotli::encoder ALIAS brotlienc)
		add_library(Brotli::decoder ALIAS brotlidec)
	endif()
	list(APPEND EXTERNAL_LINKS brotlienc)

	# fmt
	CPMAddPackage(
		NAME fmt
		GITHUB_REPOSITORY fmtlib/fmt
		GIT_TAG 12.1.0
		EXCLUDE_FROM_ALL YES
		# DOWNLOAD_ONLY YES
		OPTIONS
			"FMT_INSTALL OFF"
		SOURCE_DIR "${EXTERNAL_DIR}/fmt"
	)
	# add_library(fmt INTERFACE)
	# target_include_directories(fmt INTERFACE "${EXTERNAL_DIR}/fmt/include")
	list(APPEND EXTERNAL_LINKS fmt)

	# CPPLocate (they have extreme cmake goofyness)
	CPMAddPackage(
		NAME cpplocate
		GITHUB_REPOSITORY cginternals/cpplocate
		GIT_TAG v2.3.0
		OPTIONS
			"OPTION_BUILD_TESTS OFF"
			"CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true"
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/cpplocate"
	)
	list(APPEND EXTERNAL_LINKS cpplocate::cpplocate)
	list(APPEND EXTERNAL_LINKS cpplocate::liblocate)

	# wasmtime
	if (APPLE)
		set(ENV{MACOSX_DEPLOYMENT_TARGET} "15.0")
	endif()
	if (APPLE AND CONNECTION_MACHINE_DISTRIBUTE_APP)
		add_library(wasmtime STATIC IMPORTED GLOBAL)
		CPMAddPackage(
			NAME wasmtime
			GITHUB_REPOSITORY bytecodealliance/wasmtime
			GIT_TAG v37.0.2
			DOWNLOAD_ONLY YES
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/wasmtime"
		)
		set_target_properties(wasmtime PROPERTIES
			IMPORTED_LOCATION "${EXTERNAL_DIR}/wasmtime/target/aarch64-apple-darwin/release/libwasmtime.a"
			INTERFACE_INCLUDE_DIRECTORIES "${EXTERNAL_DIR}/wasmtime/crates/c-api"  # where .h file is
		)
	else()
		CPMAddPackage(
			NAME wasmtime
			GITHUB_REPOSITORY bytecodealliance/wasmtime
			GIT_TAG v37.0.2
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/wasmtime"
		)
		add_subdirectory("${EXTERNAL_DIR}/wasmtime/crates/c-api")
	endif()
	list(APPEND EXTERNAL_LINKS wasmtime)

	# parallel hashmap
	CPMAddPackage(
		NAME parallel-hashmap
		GITHUB_REPOSITORY greg7mdp/parallel-hashmap
		GIT_TAG v2.0.0
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/parallel-hashmap"
	)
	if(parallel-hashmap_ADDED)
		add_library(parallel-hashmap INTERFACE)
		target_include_directories(parallel-hashmap INTERFACE ${parallel-hashmap_SOURCE_DIR})
	endif()
	list(APPEND EXTERNAL_LINKS parallel-hashmap)

	# tracy
	if (RUN_TRACY_PROFILER)
		message("tracy is ON")
		CPMAddPackage(
			NAME TracyClient
			GITHUB_REPOSITORY wolfpld/tracy
			GIT_TAG v0.13.0
			OPTIONS
				"TRACY_ENABLE ON"
				"TRACY_ON_DEMAND ON"
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/tracy"
		)
		list(APPEND EXTERNAL_LINKS TracyClient)

		# add_subdirectory("${EXTERNAL_DIR}/tracy/profiler/")
	endif()

	set(EXTERNAL_LINKS "${EXTERNAL_LINKS}" PARENT_SCOPE)

	# adding all deps because code spltting became too complex

	# Find vulkan: provides glslc, the Vulkan::Headers target (with vulkan.hpp), and headers for vulkan.hpp/vma-hpp.
	find_package(Vulkan REQUIRED COMPONENTS glslc)
	list(APPEND EXTERNAL_LINKS Vulkan::Headers)

	# Volk vulkan meta loader
	CPMAddPackage(
		NAME volk
		GITHUB_REPOSITORY itchytrack/volk
		GIT_TAG b131b168d833f4d6bbe2e5bc8d41fb9792f818f8
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/volk"
	)
	list(APPEND EXTERNAL_LINKS volk)

	# VMA
	CPMAddPackage(
		NAME VulkanMemoryAllocator
		GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
		GIT_TAG v3.3.0
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/VulkanMemoryAllocator"
	)
	get_target_property(compile_opts VulkanMemoryAllocator INTERFACE_COMPILE_OPTIONS)
	if (compile_opts)
		list(APPEND compile_opts "$<$<CXX_COMPILER_ID:AppleClang>:-Wno-nullability-completeness>")
	else()
		set(compile_opts "$<$<CXX_COMPILER_ID:AppleClang>:-Wno-nullability-completeness>")
	endif()
	set_target_properties(VulkanMemoryAllocator PROPERTIES INTERFACE_COMPILE_OPTIONS "${compile_opts}")
	list(APPEND EXTERNAL_LINKS VulkanMemoryAllocator)

	# VMA-Hpp (C++ wrapper for VMA; separate repo from the C library)
	CPMAddPackage(
		NAME VulkanMemoryAllocator-Hpp
		GITHUB_REPOSITORY YaaZ/VulkanMemoryAllocator-Hpp
		GIT_TAG v3.3.0+3
		EXCLUDE_FROM_ALL YES
		DOWNLOAD_ONLY YES
		SOURCE_DIR "${EXTERNAL_DIR}/VulkanMemoryAllocator-Hpp"
	)
	add_library(VulkanMemoryAllocator-Hpp INTERFACE)
	target_include_directories(VulkanMemoryAllocator-Hpp INTERFACE "${EXTERNAL_DIR}/VulkanMemoryAllocator-Hpp/include")
	target_link_libraries(VulkanMemoryAllocator-Hpp INTERFACE VulkanMemoryAllocator Vulkan::Headers)
	target_compile_options(VulkanMemoryAllocator-Hpp INTERFACE "$<$<CXX_COMPILER_ID:AppleClang>:-Wno-nullability-completeness>")
	list(APPEND EXTERNAL_LINKS VulkanMemoryAllocator-Hpp)

	# Vk Bootstrap
	CPMAddPackage(
		NAME vk-bootstrap
		GITHUB_REPOSITORY charles-lunarg/vk-bootstrap
		GIT_TAG 4ac01fd98e05bf8a277ca099a63449f6e5c9cedf#v1.4.323
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/vk-bootstrap"
	)
	list(APPEND EXTERNAL_LINKS vk-bootstrap::vk-bootstrap)

	# GLM
	CPMAddPackage(
		NAME glm
		GITHUB_REPOSITORY g-truc/glm
		GIT_TAG 1.0.1
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/glm"
	)
	list(APPEND EXTERNAL_LINKS glm)

	# STB Image
	CPMAddPackage(
		NAME stb_image
		GITHUB_REPOSITORY nothings/stb
		GIT_TAG f58f558c120e9b32c217290b80bad1a0729fbb2c
		EXCLUDE_FROM_ALL YES
		SOURCE_DIR "${EXTERNAL_DIR}/stb"
	)
	if(stb_image_ADDED)
		add_library(stb_image INTERFACE)
		target_include_directories(stb_image INTERFACE ${stb_image_SOURCE_DIR})
	endif()
	list(APPEND EXTERNAL_LINKS stb_image)

	# Freetype
	if(APPLE)
		CPMAddPackage(
			NAME freetype
			GITHUB_REPOSITORY libsdl-org/freetype
			GIT_TAG VER-2-13-3
			OPTIONS
				"FT_DISABLE_HARFBUZZ ON"
				"FT_WITH_HARFBUZZ OFF"
				"FT_DISABLE_BROTLI ON"
				"FT_ENABLE_ERROR_STRINGS ON"
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/freetype"
		)
	else()
		CPMAddPackage(
			NAME freetype
			GITHUB_REPOSITORY libsdl-org/freetype
			GIT_TAG VER-2-13-3
			OPTIONS
			"FT_DISABLE_HARFBUZZ ON"
			"FT_WITH_HARFBUZZ OFF"
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/freetype"
			)
	endif()
	add_library(Freetype::Freetype ALIAS freetype)
	list(APPEND EXTERNAL_LINKS freetype)

	# SDL
	# if (CONNECTION_MACHINE_BUILD_TESTS) # hack to allow SDL to build without window system on linux
		# set(SDL_UNIX_CONSOLE_BUILD ON)
	# endif()
	if (CONNECTION_MACHINE_BUILD_TESTS OR CONNECTION_MACHINE_BUILD_CLI_APP)
		CPMAddPackage(
			NAME SDL3
			GITHUB_REPOSITORY libsdl-org/SDL
			GIT_TAG d9ca0457b5c9f819c3af5f156880219c8d41da40
			OPTIONS
				"SDL_STATIC ON"
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/SDL"
		)
	else()
		CPMAddPackage(
			NAME SDL3
			GITHUB_REPOSITORY libsdl-org/SDL
			GIT_TAG d9ca0457b5c9f819c3af5f156880219c8d41da40
			OPTIONS
				"SDL_STATIC ON"
				"SDL_UNIX_CONSOLE_BUILD ON"
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/SDL"
		)
	endif()
	add_library(SDL3::SDL3 ALIAS SDL3-static)
	list(APPEND EXTERNAL_LINKS SDL3::SDL3)

	# httplib
	if (CONNECTION_MACHINE_DISTRIBUTE_APP)
		CPMAddPackage(
			NAME httplib
			GITHUB_REPOSITORY yhirose/cpp-httplib
			GIT_TAG v0.26.0
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/cpp-httplib"
			OPTIONS
				"HTTPLIB_REQUIRE_OPENSSL ON"
				"OPENSSL_USE_STATIC_LIBS TRUE"
				"HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF"
		)
	else()
		CPMAddPackage(
			NAME httplib
			GITHUB_REPOSITORY yhirose/cpp-httplib
			GIT_TAG v0.26.0
			EXCLUDE_FROM_ALL YES
			SOURCE_DIR "${EXTERNAL_DIR}/cpp-httplib"
			OPTIONS
				"HTTPLIB_REQUIRE_OPENSSL ON"
				"HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF"
		)
	endif()
	list(APPEND EXTERNAL_LINKS httplib)

	# readline This will be added at some point!
	# CPMAddPackage(
	# 	NAME readline
	# 	GIT_REPOSITORY git://git.savannah.gnu.org/readline.git
	# 	GIT_TAG readline-8.3
	# 	DOWNLOAD_ONLY YES
	# 	SOURCE_DIR "${EXTERNAL_DIR}/readline"
	# )
	# file(GLOB_RECURSE readline_SOURCES
	# 	"${readline_SOURCE_DIR}/*.cpp"
	# )
	# # readline_SOURCE_DIR
	# # readline_BINARY_DIR
	# add_custom_command(
	# 	OUTPUT "${copied_resource}"
	# 	COMMAND "${CMAKE_COMMAND}" -E make_directory "${copied_resource_dir}"
	# 	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${original_resource}" "${copied_resource}"
	# 	DEPENDS "${original_resource}"
	# 	COMMENT "Copying resource: ${resource_path_relative}"
	# 	VERBATIM
	# )

	set(EXTERNAL_LINKS "${EXTERNAL_LINKS}" PARENT_SCOPE)
endfunction()
