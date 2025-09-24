# Find source files
file(GLOB_RECURSE PROJECT_SOURCES
	"${SOURCE_DIR}/*.cpp"
)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/gui\/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/gpu\/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/environment\/blockRenderDataFeeder.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/main.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${SOURCE_DIR}\/app.cpp")

# ===================================== CREATE APP EXECUTABLE ========================================

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_main_dependencies()

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LINKS})
target_precompile_headers(${PROJECT_NAME} PRIVATE "${SOURCE_DIR}/precompiled.h")

if (RUN_TRACY_PROFILER)
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES "TRACY_PROFILER")
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")
target_compile_definitions(${PROJECT_NAME} PRIVATE "CLI")

# Platform specific business after add_executable
if(APPLE) # MacOS

elseif (WIN32) # Windows
	if (CMAKE_BUILD_TYPE MATCHES Release) # If release build
		# Set WIN32_EXECUTABLE (Disables Console)
		set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
	endif ()
endif()

# if (APPLE AND CONNECTION_MACHINE_DISTRIBUTE_APP)
# 	set(TEAM_ID "" CACHE STRING "The development team ID for code signing")
# 	execute_process(
# 		COMMAND bash -c "security find-identity -v -p codesigning | grep 'Developer ID Application' | sed -n 's/.*(\\(.*\\)).*/\\1/p'"
# 		OUTPUT_VARIABLE TEAM_ID
# 		OUTPUT_STRIP_TRAILING_WHITESPACE
# 	)
# 	if(TEAM_ID STREQUAL "")
# 		message("If you are trying distribute this build you need a Developer ID Application. If not ignore this.")
# 	else()
# 		message("Found TEAM_ID")
# 		set(XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
# 		set(XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${TEAM_ID})
# 		set(XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
# 		set(XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED TRUE)

# 		set(CMAKE_MACOSX_BUNDLE YES)
# 		set_target_properties(${PROJECT_NAME} PROPERTIES
# 			XCODE_ATTRIBUTE_CODE_SIGN_STYLE ${XCODE_ATTRIBUTE_CODE_SIGN_STYLE}
# 			XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}
# 			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ${XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}
# 			XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED ${XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED}
# 			XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--timestamp=http://timestamp.apple.com/ts01  --options=runtime,library"
# 			XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO"
# 			MACOSX_BUNDLE TRUE
# 		)

# 		set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

# 		install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION ".")

# 		set(CPACK_PACKAGE_FILE_NAME "Connection-Machine-${PROJECT_VERSION}-Mac-universal")

# 		install(FILES ${Vulkan_LIBRARY} DESTINATION "${APP_NAME}.app/Contents/Frameworks")
# 		include(InstallRequiredSystemLibraries)

# 		install(CODE "
# 			include(BundleUtilities)
# 			fixup_bundle(\"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\" \"\" \"${CMAKE_BINARY_DIR}\")
# 			execute_process(COMMAND codesign --force --deep --sign ${TEAM_ID} \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
# 			execute_process(COMMAND codesign --verify --verbose=2 \"${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app\")
# 			message(\"To sign the .dgm run\")
# 			message(\"codesign --force --verbose=2 --sign $(security find-identity -v -p codesigning | grep 'Developer ID Application' | sed -n 's/.*(\\\\(.*\\\\)).*/\\\\1/p') '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
# 			message(\"To verify the .dgm is signed run\")
# 			message(\"codesign --verify --verbose=2 '${CMAKE_CURRENT_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.dmg'\")
# 		")

# 		set(CPACK_GENERATOR "DragNDrop")
# 		include(CPack)
# 	endif()
# endif()
