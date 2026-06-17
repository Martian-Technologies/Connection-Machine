#include "wasmCompiler.h"

#include "backend/wasm/wasm.h"
#include "computerAPI/directoryManager.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

std::optional<wasmtime::Module> WasmCompiler::compileFile(const std::string& file) {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	std::filesystem::path wasmCompileCommandPath = std::filesystem::absolute(DirectoryManager::getResourceDirectory() / "wasmCompileCommand.json");
	std::ifstream wasmCompileCommand(wasmCompileCommandPath);
	if (wasmCompileCommand.is_open()) {
		try {
			nlohmann::json jsonData;
			wasmCompileCommand >> jsonData;
			if (jsonData.is_object() && jsonData.contains("command") && jsonData.at("command").is_array()) {
				nlohmann::json args = jsonData.at("command");
				const std::filesystem::path& compiledWasmPath = DirectoryManager::getConfigDirectory() / "compiled_wasm";
				std::filesystem::create_directories(compiledWasmPath);
				const std::filesystem::path& wasmFile  = compiledWasmPath / (std::filesystem::path(file).stem().string() + ".wasm");
				std::string command;
				for (unsigned int i = 0; i < args.size(); i++) {
					if (i > 0) {
						command += " ";
					}
					if (args.at(i) == "source") {
						command += "\"" + file + "\"";
					} else if (args.at(i) == "target") {
						command += "\"" + wasmFile.string() + "\"";
					} else if (i == 0) {
						command += "\"" + wasmCompileCommandPath.parent_path().string() + "/" + args.at(i).get<std::string>() + "\"";
					} else {
						command += "\"" + args.at(i).get<std::string>() + "\"";
					}
				}
				logInfo(command);
				std::system(command.c_str());
				return Wasm::loadModule(wasmFile.string());
			}
			logError("Failed to load compile commands.", "WasmCompiler");
		} catch (const nlohmann::json::parse_error& e) {
			logError("Failed to load compile commands. Error: {}", "WasmCompiler", e.what());
		}
	} else {
		logError("Failed to open compile commands from \"{}\"", "WasmCompiler", wasmCompileCommandPath.string());
	}
	return std::nullopt;
}
