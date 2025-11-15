#include "wasm.h"

std::optional<wasmtime::Engine> Wasm::engine = std::nullopt;
std::optional<wasmtime::Store> Wasm::store = std::nullopt;

bool Wasm::initialize() {
	try {
		if (!(engine.has_value())) {
			engine.emplace();
			store.emplace(engine.value());
		}
		return true;
	} catch (const std::exception& e) {
		logError("Wasmtime initialization error: {}", "Wasm", e.what());
		return false;
	}
}

wasmtime::Engine* Wasm::getEngine() {
	return engine ? &(engine.value()) : nullptr;
}

wasmtime::Store* Wasm::getStore() {
	return store ? &(store.value()) : nullptr;
}


std::optional<wasmtime::Module> Wasm::loadModule(const std::string& path) {
	if (!engine.has_value()) {
		logError("Engine not initialized.", "Wasm");
		return std::nullopt;
	}

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		logError("Failed to open wasm file: {}", "Wasm", path);
		return std::nullopt;
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string contents(size, '\0');
	if (!file.read(&contents[0], size)) {
		logError("Failed to read wasm file: {}", "Wasm", path);
		return std::nullopt;
	}

	return loadModuleFromString(contents);
}

std::optional<wasmtime::Module> Wasm::loadModuleFromString(const std::string& wasmOrWat) {
	if (!engine.has_value()) {
		logError("Engine not initialized.", "Wasm");
		return std::nullopt;
	}

	try {
		// Try binary first by checking magic bytes
		const std::uint8_t wasmMagic[] = {0x00, 0x61, 0x73, 0x6d};
		bool isBinary = wasmOrWat.size() >= 4 &&
						static_cast<std::uint8_t>(wasmOrWat[0]) == wasmMagic[0] &&
						static_cast<std::uint8_t>(wasmOrWat[1]) == wasmMagic[1] &&
						static_cast<std::uint8_t>(wasmOrWat[2]) == wasmMagic[2] &&
						static_cast<std::uint8_t>(wasmOrWat[3]) == wasmMagic[3];

		wasmtime::Result<wasmtime::Module> result(wasmtime::Error("Not yet initialized."));
		if (isBinary) {
			std::vector<std::uint8_t> buffer(wasmOrWat.begin(), wasmOrWat.end());
			result = wasmtime::Module::compile(*engine, buffer);
		} else {
			result = wasmtime::Module::compile(*engine, wasmOrWat);
		}

		if (!result) {
			logError("Module compilation failed: {}", "Wasm", result.err().message());
			return std::nullopt;
		}

		return result.unwrap();
	} catch (const std::exception& e) {
		logError("Exception during module compilation: {}", "Wasm", e.what());
		return std::nullopt;
	}
}
