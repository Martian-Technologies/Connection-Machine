# Build the simulator independently of Wasmtime's Cargo workspace. The core has
# a narrow C ABI and uses native Cranelift independently of the Wasm runtime.
function(add_simulator_rust)
	find_program(SIMULATOR_CARGO cargo REQUIRED)
	find_package(Threads REQUIRED)
	set(SIMULATOR_CRATE "${SOURCE_DIR}/backend/evaluator/simulator/rust")
	set(SIMULATOR_TARGET_DIR "${CMAKE_BINARY_DIR}/rust-simulator")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(SIMULATOR_PROFILE debug)
		set(SIMULATOR_PROFILE_FLAGS)
	else()
		set(SIMULATOR_PROFILE release)
		set(SIMULATOR_PROFILE_FLAGS --release)
	endif()
	if(MSVC)
		set(SIMULATOR_LIBRARY "${SIMULATOR_TARGET_DIR}/${SIMULATOR_PROFILE}/cm_simulator.lib")
	else()
		set(SIMULATOR_LIBRARY "${SIMULATOR_TARGET_DIR}/${SIMULATOR_PROFILE}/libcm_simulator.a")
	endif()
	file(GLOB_RECURSE SIMULATOR_RUST_SOURCES CONFIGURE_DEPENDS "${SIMULATOR_CRATE}/src/*.rs")
	add_custom_command(OUTPUT "${SIMULATOR_LIBRARY}"
		COMMAND "${SIMULATOR_CARGO}" build --locked --manifest-path "${SIMULATOR_CRATE}/Cargo.toml"
			--target-dir "${SIMULATOR_TARGET_DIR}" ${SIMULATOR_PROFILE_FLAGS}
		DEPENDS ${SIMULATOR_RUST_SOURCES} "${SIMULATOR_CRATE}/Cargo.toml" "${SIMULATOR_CRATE}/Cargo.lock"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		VERBATIM)
	add_custom_target(cm_simulator_build DEPENDS "${SIMULATOR_LIBRARY}")
	add_library(cm_simulator STATIC IMPORTED GLOBAL)
	set_target_properties(cm_simulator PROPERTIES IMPORTED_LOCATION "${SIMULATOR_LIBRARY}")
	add_dependencies(cm_simulator cm_simulator_build)
	set_property(TARGET cm_simulator PROPERTY INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")
	if(WIN32)
		set_property(TARGET cm_simulator APPEND PROPERTY INTERFACE_LINK_LIBRARIES userenv ws2_32 bcrypt ntdll advapi32)
	endif()
endfunction()
