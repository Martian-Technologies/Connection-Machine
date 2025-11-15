#include "wasmProceduralCircuit.h"

#include "generatedCircuit.h"
#include "backend/wasm/wasm.h"
#include "../circuit/circuitManager.h"
#include "computerAPI/circuits/circuitFileManager.h"


WasmProceduralCircuit::WasmInstance::WasmInstance(wasmtime::Module module, CircuitManager& circuitManager, CircuitFileManager& fileManager) : circuitManager(&circuitManager), fileManager(&fileManager), thisPtr(std::make_unique<WasmInstance*>(this)) {
	WasmInstance** thisPtrPtr = thisPtr.get();
	wasmtime::Func importFileFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t fileStrOffset) -> int32_t {
			std::string path = (*thisPtrPtr)->wasmToString(fileStrOffset);
			if (std::filesystem::path(path).is_relative()) {
				const std::string* thisPath = (*thisPtrPtr)->fileManager->getSavePath((*thisPtrPtr)->UUID);
				path = std::filesystem::path(*thisPath).replace_filename(std::filesystem::path(path)).generic_string();
			}
			return (*thisPtrPtr)->fileManager->loadFromFile(path).size();
		});

	wasmtime::Func getParameterFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t keyStrOffset) -> int32_t {
			const std::map<std::string, int>& parameters = (*thisPtrPtr)->parameters->parameters;
			auto iter = parameters.find((*thisPtrPtr)->wasmToString(keyStrOffset));
			return (iter == parameters.end()) ? 0 : iter->second;
		});

	wasmtime::Func getPrimitiveTypeFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t nameStrOffset) -> int32_t {
			std::string blockName = (*thisPtrPtr)->wasmToString(nameStrOffset);
			if (blockName == "AND") return BlockType::AND;
			else if (blockName == "OR") return BlockType::OR;
			else if (blockName == "XOR") return BlockType::XOR;
			else if (blockName == "NAND") return BlockType::NAND;
			else if (blockName == "NOR") return BlockType::NOR;
			else if (blockName == "XNOR") return BlockType::XNOR;
			else if (blockName == "BUFFER") return BlockType::BUFFER;
			else if (blockName == "NOT") return BlockType::NOT;
			else if (blockName == "JUNCTION") return BlockType::JUNCTION;
			else if (blockName == "TRISTATE_BUFFER") return BlockType::TRISTATE_BUFFER;
			else if (blockName == "BUTTON") return BlockType::BUTTON;
			else if (blockName == "TICK_BUTTON") return BlockType::TICK_BUTTON;
			else if (blockName == "SWITCH") return BlockType::SWITCH;
			else if (blockName == "CONSTANT_OFF") return BlockType::CONSTANT_OFF;
			else if (blockName == "CONSTANT_ON") return BlockType::CONSTANT_ON;
			else if (blockName == "CONSTANT_Z") return BlockType::CONSTANT_Z;
			else if (blockName == "CONSTANT_X") return BlockType::CONSTANT_X;
			else if (blockName == "LIGHT") return BlockType::LIGHT;
			return BlockType::NONE;
		});

	wasmtime::Func getNonPrimitiveTypeFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t UUIDStrOffset) -> int32_t {
			std::string UUID = (*thisPtrPtr)->wasmToString(UUIDStrOffset);
			SharedCircuit circuit = (*thisPtrPtr)->circuitManager->getCircuit(UUID);
			if (circuit) {
				return circuit->getBlockType();
			}
			SharedProceduralCircuit proceduralCircuit = (*thisPtrPtr)->circuitManager->getProceduralCircuitManager().getProceduralCircuit(UUID);
			if (proceduralCircuit) {
				logError("To get the BlockType of a ProceduralCircuit you need to use \"getProceduralCircuitType\" not \"getNonPrimitiveType\"", "WasmProceduralCircuit::WasmInstance");
			}
			logError("Failed to find Circuit with UUID", "WasmInstance", UUID);
			return BlockType::NONE;
		});

	wasmtime::Func getProceduralCircuitTypeFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t UUIDStrOffset, int32_t parametersStrOffset) -> int32_t {
			std::string UUID = (*thisPtrPtr)->wasmToString(UUIDStrOffset);
			SharedProceduralCircuit proceduralCircuit = (*thisPtrPtr)->circuitManager->getProceduralCircuitManager().getProceduralCircuit(UUID);
			if (proceduralCircuit) {
				std::stringstream ss((*thisPtrPtr)->wasmToString(parametersStrOffset));
				return proceduralCircuit->getBlockType(ProceduralCircuitParameters(ss));
			}
			SharedCircuit circuit = (*thisPtrPtr)->circuitManager->getCircuit(UUID);
			if (circuit) {
				logError("To get the BlockType of a Circuit you need to use \"getNonPrimitiveType\" not \"getProceduralCircuitType\".", "WasmProceduralCircuit::WasmInstance");
			}
			logError("Failed to find ProceduralCircuit with UUID", "WasmInstance", UUID);
			return BlockType::NONE;
		});

	wasmtime::Func getBusBlockFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t bitWidth) -> int32_t {
			if (bitWidth <= 0) {
				logError("getBusBlock expects a positive bit width but received {}", "WasmProceduralCircuit::WasmInstance", bitWidth);
				return static_cast<int32_t>(BlockType::NONE);
			}
			BlockType blockType = (*thisPtrPtr)->circuitManager->getBlockDataManager().getBusBlock(static_cast<unsigned int>(bitWidth));
			return static_cast<int32_t>(blockType);
		});

	wasmtime::Func getBusBlockAdvancedFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](
			int32_t numInputs,
			int32_t numOutputs,
			int32_t inputLaneWidth,
			int32_t outputLaneWidth
		) -> int32_t {
			if (numInputs <= 0 || numOutputs <= 0 || inputLaneWidth <= 0 || outputLaneWidth <= 0) {
				logError(
					"getBusBlockAdvanced expects positive values for all parameters but received: numInputs={}, numOutputs={}, inputLaneWidth={}, outputLaneWidth={}",
					"WasmProceduralCircuit::WasmInstance",
					numInputs, numOutputs, inputLaneWidth, outputLaneWidth
				);
				return static_cast<int32_t>(BlockType::NONE);
			}
			BlockType blockType = (*thisPtrPtr)->circuitManager->getBlockDataManager().getBusBlock(
				static_cast<unsigned int>(numInputs),
				static_cast<unsigned int>(numOutputs),
				static_cast<unsigned int>(inputLaneWidth),
				static_cast<unsigned int>(outputLaneWidth)
			);
			return static_cast<int32_t>(blockType);
		});

	wasmtime::Func createBlockFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t blockType) -> int32_t {
			return (*thisPtrPtr)->generatedCircuit->addBlock((BlockType)blockType);
		});

	wasmtime::Func createBlockAtPositionFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t x, int32_t y, int32_t rotation, int32_t blockType) -> int32_t {
			return (*thisPtrPtr)->generatedCircuit->addBlock(Position(x, y), (Rotation)rotation, (BlockType)blockType);
		});

	wasmtime::Func createConnectionFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t outputBlockId, int32_t outputPortId, int32_t inputBlockId, int32_t inputPortId) {
			(*thisPtrPtr)->generatedCircuit->addConnection(outputBlockId, connection_end_id_t(outputPortId), inputBlockId, connection_end_id_t(inputPortId));
		});

	wasmtime::Func addConnectionInputFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t portX, int32_t portY, int32_t internalBlockId, int32_t internalBlockPortId) {
			(*thisPtrPtr)->generatedCircuit->addConnectionPort(true, connection_end_id_t((*thisPtrPtr)->portId), Vector(portX, portY), internalBlockId, connection_end_id_t(internalBlockPortId), "Port" + std::to_string((*thisPtrPtr)->portId));
			++((*thisPtrPtr)->portId);
		});

	wasmtime::Func addConnectionOutputFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t portX, int32_t portY, int32_t internalBlockId, int32_t internalBlockPortId) {
			(*thisPtrPtr)->generatedCircuit->addConnectionPort(false, connection_end_id_t((*thisPtrPtr)->portId), Vector(portX, portY), internalBlockId, connection_end_id_t(internalBlockPortId), "Port" + std::to_string((*thisPtrPtr)->portId));
			++((*thisPtrPtr)->portId);
		});

	wasmtime::Func setSizeFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t width, int32_t height) {
			(*thisPtrPtr)->generatedCircuit->setSize(Size(width, height));
		});

	wasmtime::Func logInfoFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t strOffset) {
			logInfo((*thisPtrPtr)->wasmToString(strOffset), "WasmProceduralCircuit > WasmCode");
		});

	wasmtime::Func logErrorFunc = wasmtime::Func::wrap(*Wasm::getStore(),
		[thisPtrPtr](int32_t strOffset) {
			logError((*thisPtrPtr)->wasmToString(strOffset), "WasmProceduralCircuit > WasmCode");
		});

	// Linker to associate "env" functions
	wasmtime::Linker linker(*Wasm::getEngine());
	wasmtime::Result<std::monostate> linkerResult = wasmtime::Result<std::monostate>(nullptr);
	linkerResult = linker.define(*Wasm::getStore(), "env", "importFile", importFileFunc);
	if (!linkerResult) {
		logError("could not create link to env.importFile", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getParameter", getParameterFunc);
	if (!linkerResult) {
		logError("could not create link to env.getParameter", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getPrimitiveType", getPrimitiveTypeFunc);
	if (!linkerResult) {
		logError("could not create link to env.getPrimitiveType", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getNonPrimitiveType", getNonPrimitiveTypeFunc);
	if (!linkerResult) {
		logError("could not create link to env.getNonPrimitiveType", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getProceduralCircuitType", getProceduralCircuitTypeFunc);
	if (!linkerResult) {
		logError("could not create link to env.getProceduralCircuitType", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getBusBlock", getBusBlockFunc);
	if (!linkerResult) {
		logError("could not create link to env.getBusBlock", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "getBusBlockAdvanced", getBusBlockAdvancedFunc);
	if (!linkerResult) {
		logError("could not create link to env.getBusBlockAdvanced", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "createBlock", createBlockFunc);
	if (!linkerResult) {
		logError("could not create link to env.createBlockFunc", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "createBlockAtPosition", createBlockAtPositionFunc);
	if (!linkerResult) {
		logError("could not create link to env.createBlockAtPosition", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "createConnection", createConnectionFunc);
	if (!linkerResult) {
		logError("could not create link to env.createConnection", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "addConnectionInput", addConnectionInputFunc);
	if (!linkerResult) {
		logError("could not create link to env.addConnectionInput", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "addConnectionOutput", addConnectionOutputFunc);
	if (!linkerResult) {
		logError("could not create link to env.addConnectionOutput", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "setSize", setSizeFunc);
	if (!linkerResult) {
		logError("could not create link to env.setSize", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "logInfo", logInfoFunc);
	if (!linkerResult) {
		logError("could not create link to env.logInfo", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	linkerResult = linker.define(*Wasm::getStore(), "env", "logError", logErrorFunc);
	if (!linkerResult) {
		logError("could not create link to env.logError", "WasmProceduralCircuit::WasmInstance");
		return;
	}

	// Instantiate the module
	auto instanceResult = linker.instantiate(*Wasm::getStore(), module);
	if (!instanceResult) {
		logError("Failed to instantiate WASM module: {}", "WasmProceduralCircuit::WasmInstance", instanceResult.err().message());
		return;
	}
	instance.emplace(std::move(instanceResult.unwrap()));
	std::optional<wasmtime::Extern> memoryExport = instance.value().get(*Wasm::getStore(), "memory");
	if (!memoryExport) {
		logError("Failed to get WASM memory.", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	memory.emplace(std::get<wasmtime::Memory>(memoryExport.value()));

	std::optional<wasmtime::Extern> uuidExtern(instance.value().get(*Wasm::getStore(), "getUUID"));
	if (!uuidExtern) {
		logError("Failed to get getUUID function.", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	auto uuidFunc = std::get<wasmtime::Func>(uuidExtern.value());
	auto uuidResults = uuidFunc.call(*Wasm::getStore(), {}).unwrap();
	UUID = wasmToString(uuidResults.front().i32());

	std::optional<wasmtime::Extern> nameExtern(instance.value().get(*Wasm::getStore(), "getName"));
	if (!nameExtern) {
		logError("Failed to get getName function.", "WasmProceduralCircuit::WasmInstance");
		return;
	}
	auto nameFunc = std::get<wasmtime::Func>(nameExtern.value());
	auto nameResults = nameFunc.call(*Wasm::getStore(), {}).unwrap();
	name = wasmToString(nameResults.front().i32());

	valid = true;

	std::optional<wasmtime::Extern> defaultParametersExtern(instance.value().get(*Wasm::getStore(), "getDefaultParameters"));
	if (!defaultParametersExtern) {
		return; // No default parameters is valid!
	}
	auto defaultParametersFunc = std::get<wasmtime::Func>(defaultParametersExtern.value());
	auto defaultParametersResults = defaultParametersFunc.call(*Wasm::getStore(), {}).unwrap();
	std::stringstream ss(wasmToString(defaultParametersResults.front().i32()));
	defaultParameters = ProceduralCircuitParameters(ss);
}

WasmProceduralCircuit::WasmInstance::WasmInstance(WasmInstance&& wasmInstance) :
	instance(std::move(wasmInstance.instance.value())),
	memory(std::move(wasmInstance.memory.value())),
	name(std::move(wasmInstance.name)),
	UUID(std::move(wasmInstance.UUID)),
	defaultParameters(std::move(wasmInstance.defaultParameters)),
	thisPtr(std::move(wasmInstance.thisPtr)),
	circuitManager(wasmInstance.circuitManager),
	fileManager(wasmInstance.fileManager) {
	*thisPtr = this;
}

WasmProceduralCircuit::WasmInstance& WasmProceduralCircuit::WasmInstance::operator=(WasmInstance&& wasmInstance) {
	if (this != &wasmInstance) {
		instance = std::move(wasmInstance.instance.value());
		memory = std::move(wasmInstance.memory.value());
		name = std::move(wasmInstance.name);
		UUID = std::move(wasmInstance.UUID);
		defaultParameters = std::move(wasmInstance.defaultParameters);
		thisPtr = std::move(wasmInstance.thisPtr);
		circuitManager = wasmInstance.circuitManager;
		fileManager = wasmInstance.fileManager;
		*thisPtr = this;
	}
	return *this;
}

void WasmProceduralCircuit::WasmInstance::makeCircuit(const ProceduralCircuitParameters& parameters, GeneratedCircuit& generatedCircuit) {
	if (!instance.has_value()) return;

	// saved in case we are calling this recursively
	const ProceduralCircuitParameters* tmpParameters = this->parameters;
	GeneratedCircuit* tmpCircuit = this->generatedCircuit;
	unsigned int tmpPortId = portId;
	unsigned int tmpBlockId = blockId;

	this->parameters = &parameters;
	this->generatedCircuit = &generatedCircuit;
	portId = 0;
	blockId = 0;

	auto func = std::get<wasmtime::Func>(instance.value().get(*Wasm::getStore(), "generateCircuit").value());
	auto results = func.call(*Wasm::getStore(), {}).unwrap();
	bool success = results.front().i32();

	this->parameters = tmpParameters;
	this->generatedCircuit = tmpCircuit;
	portId = tmpPortId;
	blockId = tmpBlockId;
}

std::string WasmProceduralCircuit::WasmInstance::wasmToString(int32_t wasmPtr) {
	auto memSpan = memory.value().data(*Wasm::getStore());
	const char* str = (const char*)(memSpan.data() + wasmPtr);
	return std::string(str, strnlen(str, memSpan.size() - wasmPtr));
}

WasmProceduralCircuit::WasmProceduralCircuit(
	CircuitManager& circuitManager,
	DataUpdateEventManager& dataUpdateEventManager,
	WasmInstance&& wasmInstance
) : ProceduralCircuit(circuitManager, dataUpdateEventManager, wasmInstance.getName(), wasmInstance.getUUID()), wasmInstance(std::move(wasmInstance)) {
	setParameterDefaults(this->wasmInstance.getDefaultParameters());
}

WasmProceduralCircuit::WasmProceduralCircuit(WasmProceduralCircuit&& other) : ProceduralCircuit(static_cast<ProceduralCircuit&&>(other)), wasmInstance(std::move(other.wasmInstance)) { }

void WasmProceduralCircuit::setWasm(WasmInstance&& wasmInstance) {
	if (!wasmInstance.isValid()) {
		logError("setWasm() Failed! WasmInstance was not valid.", "WasmProceduralCircuit");
		return;
	}
	if (wasmInstance.getUUID() != this->wasmInstance.getUUID()) {
		logError(
			"setWasm() Failed! Tried to update the uuid of a WasmProceduralCircuit. DO NOT DO THIS. Old UUID: {}. New UUID: {}",
			 "WasmProceduralCircuit", this->wasmInstance.getUUID(), wasmInstance.getUUID()
		);
		return;
	}
	setProceduralCircuitName(wasmInstance.getName());
	setParameterDefaults(wasmInstance.getDefaultParameters());

	this->wasmInstance = std::move(wasmInstance);

	regenerateAll();
}

void WasmProceduralCircuit::makeCircuit(const ProceduralCircuitParameters& parameters, GeneratedCircuit& generatedCircuit) {
	wasmInstance.makeCircuit(parameters, generatedCircuit);
}

nlohmann::json WasmProceduralCircuit::dumpStateInherited() const {
	nlohmann::json stateJson;
	stateJson["wasmInstance"] = wasmInstance.dumpState();
	return stateJson;
}

nlohmann::json WasmProceduralCircuit::WasmInstance::dumpState() const {
	nlohmann::json stateJson;
	stateJson["valid"] = valid;
	stateJson["name"] = name;
	stateJson["UUID"] = UUID;
	stateJson["defaultParameters"] = defaultParameters.dumpState();
	return stateJson;
}
