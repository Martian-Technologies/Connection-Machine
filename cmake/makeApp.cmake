# Find source files
file(GLOB_RECURSE PROJECT_SOURCES
	"${SOURCE_DIR}/*.cpp"
)
string(REGEX REPLACE "([][+.*^$(){}|\\\\])" "\\\\\\1" SOURCE_DIR_REGEX "${SOURCE_DIR}")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR_REGEX}/cli/main.cpp$")

# ===================================== CREATE APP EXECUTABLE ========================================

# Platform specific business before add_executable
if(APPLE) # MacOS
	# Icon
	set(ICON_PATH "${RESOURCES_DIR}/gateIcon.icns")
	set(MACOSX_BUNDLE_ICON_FILE "gateIcon.icns")
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND PROJECT_SOURCES ${ICON_PATH})
	# if(CONNECTION_MACHINE_DISTRIBUTE_APP)
	# 	set(OPENSSL_ROOT_DIR "/Users/ben/Documents/GitHub/Logic-Graph-Creator/lib-binary/libopenssl/")
	# 	set(CMAKE_SYSTEM_IGNORE_PREFIX_PATH "/opt/homebrew/lib;/opt/homebrew/opt")
	# 	set(CMAKE_IGNORE_PREFIX_PATH "/opt/homebrew/lib;/opt/homebrew/opt")
	# endif()
elseif (WIN32) # Windows
	# Icon
	set(ICON_PATH "${RESOURCES_DIR}/icon.rc")
	list(APPEND PROJECT_SOURCES ${ICON_PATH})
endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_main_dependencies()

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LINKS})
target_precompile_headers(${PROJECT_NAME} PRIVATE "${SOURCE_DIR}/precompiled.h")
set_source_files_properties("${SOURCE_DIR}/gui/stbImplementation.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

if (RUN_TRACY_PROFILER)
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES VULKAN_HPP_NO_EXCEPTIONS VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1 "TRACY_PROFILER")
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES VULKAN_HPP_NO_EXCEPTIONS VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
endif()
if (CONNECTION_MACHINE_TRY_CATCH)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "MAIN_TRY_CATCH")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE "BUILDING_APP")
target_compile_definitions(${PROJECT_NAME} PRIVATE "PROJECT_VERSION=\"${PROJECT_VERSION}\"")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")

# Platform specific business after add_executable
if(APPLE) # MacOS
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
		VERBATIM
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
		VERBATIM
	)

	list(APPEND SHADER_PRODUCTS "${compiled_shader}")
endforeach()

add_custom_target(compile-shaders DEPENDS ${SHADER_PRODUCTS})
add_dependencies(${PROJECT_NAME} compile-shaders)

if (WIN32 AND CONNECTION_MACHINE_DISTRIBUTE_APP)
	set(WINDOWS_DISTRIBUTION_ZIP "Connection-Machine-${PROJECT_VERSION}-Windows.zip")
	set(WINDOWS_DISTRIBUTION_STAGE_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/dist-stage")
	set(WINDOWS_DISTRIBUTION_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")

	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E rm -rf "${WINDOWS_DISTRIBUTION_STAGE_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${WINDOWS_DISTRIBUTION_ZIP}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${WINDOWS_DISTRIBUTION_STAGE_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${WINDOWS_DISTRIBUTION_RESOURCE_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${WINDOWS_DISTRIBUTION_STAGE_DIR}/Connection_Machine.exe"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${WINDOWS_DISTRIBUTION_RESOURCE_DIR}" "${WINDOWS_DISTRIBUTION_STAGE_DIR}/resources"
		COMMAND "${CMAKE_COMMAND}" -E chdir "${WINDOWS_DISTRIBUTION_STAGE_DIR}" "${CMAKE_COMMAND}" -E tar "cf" "../${WINDOWS_DISTRIBUTION_ZIP}" --format=zip "Connection_Machine.exe" "resources"
		COMMAND "${CMAKE_COMMAND}" -E rm -rf "${WINDOWS_DISTRIBUTION_STAGE_DIR}"
		VERBATIM
	)
endif()

if (APPLE AND CONNECTION_MACHINE_DISTRIBUTE_APP)
	set(TEAM_ID "" CACHE STRING "The development team ID for code signing")
	execute_process(
		COMMAND bash -c "security find-identity -v -p codesigning | grep 'Developer ID Application' | sed -n 's/.*\"\\\(.*\\\)\".*/\\1/p'"
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

		set(CPACK_PACKAGE_FILE_NAME "Connection-Machine-${PROJECT_VERSION}-Mac")

		install(FILES ${Vulkan_LIBRARY} DESTINATION "${APP_NAME}.app/Contents/Frameworks")
		include(InstallRequiredSystemLibraries)

		message("To store notarization credentials run:")
		message("xcrun notarytool store-credentials \"CMNotaryProfile\" --apple-id \"you@your.com\" --team-id '${TEAM_ID}' --password \"app-specific-password\"")

		set(PACKAGE_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/package.sh")

		install(CODE "
			include(BundleUtilities)
			fixup_bundle(\"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\" \"\" \"${CMAKE_BINARY_DIR}\")
			fixup_bundle(\"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\" \"\" \"${CMAKE_BINARY_DIR}\")

			file(REMOVE \"${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg\")

			file(WRITE ${PACKAGE_SCRIPT} \"#!/bin/bash\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"cd \\\"${CMAKE_CURRENT_BINARY_DIR}\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"rm -rf \\\"./out\\\"\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"mkdir \\\"./out\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"hdiutil attach \\\"./${CPACK_PACKAGE_FILE_NAME}.dmg\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"cp -R \\\"/Volumes/${CPACK_PACKAGE_FILE_NAME}/${APP_NAME}.app\\\" \\\"./out\\\" \\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"hdiutil detach \\\"/Volumes/${CPACK_PACKAGE_FILE_NAME}\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"rm -r \\\"./${CPACK_PACKAGE_FILE_NAME}.dmg\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"codesign --force -vvv --deep --strict --options runtime --sign \\\"${TEAM_ID}\\\" \\\"./out/${APP_NAME}.app\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"codesign --verify --deep --strict --verbose=2 \\\"./out/${APP_NAME}.app\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"cd \\\"${CMAKE_CURRENT_BINARY_DIR}/out\\\"\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"ditto -c -k --sequesterRsrc --keepParent \\\"${APP_NAME}.app\\\" \\\"${APP_NAME}.app.zip\\\"\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"cd \\\"${CMAKE_CURRENT_BINARY_DIR}\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"xcrun notarytool submit \\\"./out/${APP_NAME}.app.zip\\\" --keychain-profile \\\"CMNotaryProfile\\\" --wait\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"rm -r \\\"./out/${APP_NAME}.app.zip\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"xcrun stapler staple \\\"./out/${APP_NAME}.app\\\"\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"spctl --assess --type execute --verbose \\\"./out/${APP_NAME}.app\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"ln -s /Applications \\\"./out/Applications\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"hdiutil create -volname \\\"${CPACK_PACKAGE_FILE_NAME}\\\" -srcfolder \\\"./out\\\" -ov -format UDZO \\\"./out/${CPACK_PACKAGE_FILE_NAME}.dmg\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"codesign --force -vvv --strict --sign \\\"${TEAM_ID}\\\" \\\"./out/${CPACK_PACKAGE_FILE_NAME}.dmg\\\"\\n\")

			file(APPEND ${PACKAGE_SCRIPT} \"xcrun notarytool submit \\\"./out/${CPACK_PACKAGE_FILE_NAME}.dmg\\\" --keychain-profile \\\"CMNotaryProfile\\\" --wait\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"xcrun stapler staple \\\"./out/${CPACK_PACKAGE_FILE_NAME}.dmg\\\"\\n\")
			file(APPEND ${PACKAGE_SCRIPT} \"spctl --assess --type install --verbose \\\"./out/${APP_NAME}.app\\\"\\n\")

			message(\"\")
			message(\"\")
			message(\"bash \\\"${PACKAGE_SCRIPT}\\\"\")
			message(\"\")
			message(\"\")
		")

		# file(APPEND ${PACKAGE_SCRIPT} \"xcrun notarytool submit \\\"./out/${CPACK_PACKAGE_FILE_NAME}.dmg\\\" --keychain-profile \\\"CMNotaryProfile\\\" --wait\\n\")
		# file(APPEND ${PACKAGE_SCRIPT} \"spctl --assess --type execute --verbose \\\"./out/${APP_NAME}.app\\\"\\n\")
		# file(APPEND ${PACKAGE_SCRIPT} \"spctl --assess --type install --verbose \\\"./out/${APP_NAME}.app\\\"\\n\")
		# execute_process(COMMAND codesign --force -vvv --deep --strict --options runtime --sign \"${TEAM_ID}\" \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
		# execute_process(COMMAND codesign --verify --deep --strict --verbose=2 \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")

		# message(\"\")
		# execute_process(
		# 	COMMAND ditto -c -k --sequesterRsrc --keepParent \"${APP_NAME}.app\" \"${APP_NAME}.app.zip\"
		# 	WORKING_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}\"
		# )
		# execute_process(COMMAND xcrun notarytool submit \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app.zip\" --keychain-profile \"CMNotaryProfile\" --wait)
		# execute_process(COMMAND xcrun stapler staple \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
		# execute_process(COMMAND spctl --assess --type execute --verbose \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")

		# message(\"\")
		# execute_process(COMMAND hdiutil create -volname \"Connection-Machine\" -srcfolder \"${TEAM_ID}\" \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\" -ov -format UDZO \"${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg\")
		# execute_process(COMMAND codesign --force -vvv --strict --sign \"TEAM_ID\" --timestamp \"${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg\")

		# message(\"\")
		# message(\"\")
		# message(\"To sign the .dmg run:\")
		# message(\"codesign --force -vvv --deep --strict --options runtime --sign \"${TEAM_ID}\" --timestamp '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
		# message(\"To verify the .dmg is signed run:\")
		# message(\"codesign --verify --deep --strict --verbose=2 '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
		# message(\"To notarize the .dmg is signed run:\")
		# execute_process(COMMAND xcrun notarytool submit '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg' --keychain-profile \"CMNotaryProfile\" --wait)
		# message(\"spctl --assess --type execute --verbose\")
		# execute_process(COMMAND xcrun stapler staple '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg')

		set(CPACK_GENERATOR "DragNDrop")
		include(CPack)
	endif()
endif()
