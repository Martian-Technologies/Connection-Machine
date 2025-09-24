# Find source files
file(GLOB_RECURSE PROJECT_SOURCES
	"${SOURCE_DIR}/*.cpp"
)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}/cli/.*")

# ===================================== CREATE APP EXECUTABLE ========================================

# Platform specific business before add_executable
if(APPLE) # MacOS
	# Icon
	set(ICON_PATH "${RESOURCES_DIR}/gateIcon.icns")
	set(MACOSX_BUNDLE_ICON_FILE "gateIcon.icns")
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND PROJECT_SOURCES ${ICON_PATH})

elseif (WIN32) # Windows
	# Icon
	set(ICON_PATH "${RESOURCES_DIR}/icon.rc")
	list(APPEND PROJECT_SOURCES ${ICON_PATH})
endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_main_dependencies()
add_app_dependencies()

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LINKS})
target_precompile_headers(${PROJECT_NAME} PRIVATE "${SOURCE_DIR}/precompiled.h")
set_source_files_properties("${SOURCE_DIR}/gui/stbImplementation.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

if (RUN_TRACY_PROFILER)
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES "TRACY_PROFILER")
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")

# Platform specific business after add_executable
if(APPLE) # MacOS
	# set(CMAKE_MACOSX_BUNDLE YES)

	set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
	# Bundle Properties
	set_target_properties(${PROJECT_NAME} PROPERTIES
		MACOSX_BUNDLE TRUE
		# indentification
		MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.connection-machine.app"
		MACOSX_BUNDLE_NAME "${APP_NAME}"
		MACOSX_BUNDLE_COPYRIGHT "MIT License" # this needs changing
		# version info
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
		MACOSX_BUNDLE_VERSION "${PROJECT_VERSION}"
		MACOSX_BUNDLE_INFO_VERSION "6.0"
		# Dev Region
		MACOSX_BUNDLE_DEVELOPMENT_REGION "English"
		# Doc Types that macos can handle
		MACOSX_BUNDLE_DOCUMENT_TYPES "json;JSON File;Editor;${MACOSX_BUNDLE_GUI_IDENTIFIER}.json;txt;Text File;Editor;${MACOSX_BUNDLE_GUI_IDENTIFIER}.txt;circuit;Circuit File;Editor;${MACOSX_BUNDLE_GUI_IDENTIFIER}.cir;circuit;Circuit File;Editor;${MACOSX_BUNDLE_GUI_IDENTIFIER}.circuit;"
		# Application category
		MACOSX_BUNDLE_INFO_STRING "Logic Gate Sandbox Application"
		MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
	)
	set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "")
elseif (WIN32) # Windows
	if (CMAKE_BUILD_TYPE MATCHES Release) # If release build
		# Set WIN32_EXECUTABLE (Disables Console)
		set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
	endif ()
endif()

# ======================================= RESOURCE COPYING =========================================
# We copy the resources directory to the location of the output executable. This is our solution for "resource installing" until we fully set up cpack solution

# Get resource files relative to resources dir
file(GLOB_RECURSE RESOURCE_FILES RELATIVE "${RESOURCES_DIR}" "${RESOURCES_DIR}/*")
set(RESOURCE_FILES_COPIED)

foreach(resource_path_relative IN LISTS RESOURCE_FILES)
	set(original_resource "${RESOURCES_DIR}/${resource_path_relative}")
	if(APPLE)
		set(copied_resource "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/MacOS/resources/${resource_path_relative}")
	else()
		set(copied_resource "${CMAKE_CURRENT_BINARY_DIR}/resources/${resource_path_relative}")
	endif()
	get_filename_component(copied_resource_dir "${copied_resource}" DIRECTORY)

	# Custom command which makes the parent directory and copies the resource
	add_custom_command(
		OUTPUT "${copied_resource}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${copied_resource_dir}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${original_resource}" "${copied_resource}"
		DEPENDS "${original_resource}"
		COMMENT "Copying resource: ${resource_path_relative}"
	)

	list(APPEND RESOURCE_FILES_COPIED "${copied_resource}")
endforeach()

add_custom_target(copy-resources DEPENDS ${RESOURCE_FILES_COPIED})
add_dependencies(${PROJECT_NAME} copy-resources)

# ===================================== SHADER COMPILATION =========================================
if(APPLE)
	set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/MacOS/resources/shaders")
else()
	set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders")
endif()
file(GLOB_RECURSE SHADER_SOURCE_FILES
	"${SOURCE_DIR}/*.vert"
	"${SOURCE_DIR}/*.frag"
)
set(SHADER_PRODUCTS)

foreach(shader_source IN LISTS SHADER_SOURCE_FILES)
	cmake_path(GET shader_source FILENAME shader_name)
	set(compiled_shader "${SHADER_BINARY_DIR}/${shader_name}.spv")

	# Custom command which makes the parent directory and compiles the shader
	add_custom_command(
		OUTPUT "${compiled_shader}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_BINARY_DIR}"
		COMMAND Vulkan::glslc "${shader_source}" "-o" "${compiled_shader}"
		DEPENDS "${shader_source}"
		COMMENT "Compiling shader: ${shader_source}"
	)

	list(APPEND SHADER_PRODUCTS "${compiled_shader}")
endforeach()

add_custom_target(compile-shaders DEPENDS ${SHADER_PRODUCTS})
add_dependencies(${PROJECT_NAME} compile-shaders)

if (APPLE AND CONNECTION_MACHINE_DISTRIBUTE_APP)
	set(TEAM_ID "" CACHE STRING "The development team ID for code signing")
	execute_process(
		COMMAND bash -c "security find-identity -v -p codesigning | grep 'Developer ID Application' | sed -n 's/.*(\\(.*\\)).*/\\1/p'"
		OUTPUT_VARIABLE TEAM_ID
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(TEAM_ID STREQUAL "")
		message("If you are trying distribute this build you need a Developer ID Application. If not ignore this.")
	else()
		message("Found TEAM_ID")
		set(XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
		set(XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${TEAM_ID})
		set(XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
		set(XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED TRUE)

		set(CMAKE_MACOSX_BUNDLE YES)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			XCODE_ATTRIBUTE_CODE_SIGN_STYLE ${XCODE_ATTRIBUTE_CODE_SIGN_STYLE}
			XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}
			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ${XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}
			XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED ${XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED}
			XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--timestamp=http://timestamp.apple.com/ts01  --options=runtime,library"
			XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO"
			MACOSX_BUNDLE TRUE
		)

		set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

		install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION ".")

		set(CPACK_PACKAGE_FILE_NAME "Connection-Machine-${PROJECT_VERSION}-Mac-universal")

		install(FILES ${Vulkan_LIBRARY} DESTINATION "${APP_NAME}.app/Contents/Frameworks")
		include(InstallRequiredSystemLibraries)

		install(CODE "
			include(BundleUtilities)
			fixup_bundle(\"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\" \"\" \"${CMAKE_BINARY_DIR}\")
			execute_process(COMMAND codesign --force --deep --sign ${TEAM_ID} \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
			execute_process(COMMAND codesign --verify --verbose=2 \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
			message(\"To sign the .dgm run\")
			message(\"codesign --force --verbose=2 --sign $(security find-identity -v -p codesigning | grep 'Developer ID Application' | sed -n 's/.*(\\\\(.*\\\\)).*/\\\\1/p') '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
			message(\"To verify the .dgm is signed run\")
			message(\"codesign --verify --verbose=2 '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
		")

		set(CPACK_GENERATOR "DragNDrop")
		include(CPack)
	endif()
endif()
