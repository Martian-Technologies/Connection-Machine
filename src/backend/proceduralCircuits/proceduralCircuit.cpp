#include "proceduralCircuit.h"

#include "../circuit/circuitManager.h"
#include "generatedCircuitValidator.h"

ProceduralCircuitParameters::ProceduralCircuitParameters(std::istream& ss) {
	char cToken;
	ss >> cToken;
	if (cToken != '(') {
		ss.unget(); // Makes the stream be unchanged from when it was passed in
		return;
	}
	while (true) {
		std::string str;
		ss >> cToken;
		if (cToken != ',') ss.unget();
		ss >> cToken;
		ss.unget();
		if (cToken != '"') {
			break;
		}
		ss >> std::quoted(str);
		ss >> cToken;
		int value = 0;
		ss >> value;
		parameters[str] = value;
	}
	ss.ignore(std::numeric_limits<std::streamsize>::max(), ')');
}

ProceduralCircuitParameters& ProceduralCircuitParameters::operator=(const ProceduralCircuitParameters& other) {
	if (this != &other) parameters = other.parameters;
	return *this;
}

ProceduralCircuitParameters& ProceduralCircuitParameters::operator=(ProceduralCircuitParameters&& other) {
	if (this != &other) parameters = std::move(other.parameters);
	return *this;
}

std::string ProceduralCircuitParameters::toString() const {
	std::string str = "(";
	for (const auto& iter : parameters) {
		if (str.size() != 1) str += ", ";
		str += '"' + iter.first + "\": " + std::to_string(iter.second);
	}
	return str + ")";
}

ProceduralCircuit::ProceduralCircuit(CircuitManager* circuitManager, DataUpdateEventManager* dataUpdateEventManager, const std::string& name, const std::string& uuid) :
	circuitManager(circuitManager), dataUpdateEventManager(dataUpdateEventManager), dataUpdateEventReceiver(dataUpdateEventManager), proceduralCircuitName(name),
	proceduralCircuitUUID(uuid) { }

ProceduralCircuit::ProceduralCircuit(ProceduralCircuit&& other) :
	proceduralCircuitName(std::move(other.proceduralCircuitName)), proceduralCircuitUUID(std::move(other.proceduralCircuitUUID)), circuitManager(other.circuitManager),
	generatedCircuits(std::move(other.generatedCircuits)), circuitIdToProceduralCircuitParameters(std::move(other.circuitIdToProceduralCircuitParameters)),
	dataUpdateEventManager(other.dataUpdateEventManager), dataUpdateEventReceiver(std::move(other.dataUpdateEventReceiver)) { }

ProceduralCircuit::~ProceduralCircuit() {
	// Destroy unused generated circuits. If some are still used ask the user what to do.
}

void ProceduralCircuit::setProceduralCircuitName(const std::string& name) {
	if (this->proceduralCircuitName == name) return;
	this->proceduralCircuitName = name;
	for (const auto& iter : generatedCircuits) {
		SharedCircuit circuit = circuitManager->getCircuit(iter.second);
		if (circuit) circuit->setCircuitName(name + iter.first.toString());
	}
	dataUpdateEventManager->sendEvent<std::string>("proceduralCircuitPathUpdate", getUUID());
}

const ProceduralCircuitParameters* ProceduralCircuit::getProceduralCircuitParameters(circuit_id_t circuitId) const {
	auto iter = circuitIdToProceduralCircuitParameters.find(circuitId);
	if (iter == circuitIdToProceduralCircuitParameters.end()) return nullptr;
	return &(iter->second);
}

circuit_id_t ProceduralCircuit::getCircuitId(const ProceduralCircuitParameters& parameters) {
	// Make sure to only use parameters that are reconized (anything in parameterDefaults)
	ProceduralCircuitParameters realParameters = parameterDefaults;
	for (auto& iter : realParameters.parameters) {
		auto iter2 = parameters.parameters.find(iter.first);
		if (iter2 != parameters.parameters.end()) {
			iter.second = iter2->second;
		}
	}

	// Check if its already been generated
	auto iter = generatedCircuits.find(realParameters);
	if (iter != generatedCircuits.end()) {
		return iter->second;
	}

	logInfo("Creating circuit with parameters: {}", "ProceduralCircuit", realParameters.toString());

	// Make the circuit
	GeneratedCircuit generatedCircuit;
	this->makeCircuit(realParameters, generatedCircuit);
	generatedCircuit.markAsCustom();
	GeneratedCircuitValidator validator(generatedCircuit, circuitManager->getBlockDataManager());

	if (!(generatedCircuit.isValid())) return 0;

	// Create the circuit if it has not been generated
	circuit_id_t id = circuitManager->createNewCircuit(generatedCircuit, false);

	// Add the circuit id to the generated circuits
	generatedCircuits[realParameters] = id;
	circuitIdToProceduralCircuitParameters[id] = realParameters;

	// Setup the block to be a IC
	BlockType type = circuitManager->setupBlockData(id, proceduralCircuitUUID);
	if (type == BlockType::NONE) return 0;

	// Get useful objects
	SharedCircuit circuit = circuitManager->getCircuit(id);
	BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(type);

	// Settings
	blockData->setIsPlaceable(false);
	blockData->setPath(getPath() + " instances"); // for testing
	circuit->setCircuitName(getProceduralCircuitName() + " (" + realParameters.toString() + ")");
	circuit->setEditable(false);

	return id;
}

BlockType ProceduralCircuit::getBlockType(const ProceduralCircuitParameters& parameters) {
	circuit_id_t circuitId = getCircuitId(parameters);
	SharedCircuit circuit = circuitManager->getCircuit(circuitId);
	return circuit ? circuit->getBlockType() : BlockType::NONE;
}

void ProceduralCircuit::regenerateAll() {
	for (auto pair : generatedCircuits) {
		ProceduralCircuitParameters realParameters = parameterDefaults;
		for (auto& iter : realParameters.parameters) {
			auto iter2 = pair.first.parameters.find(iter.first);
			if (iter2 != pair.first.parameters.end()) {
				iter.second = iter2->second;
			}
		}
		GeneratedCircuit generatedCircuit;
		this->makeCircuit(realParameters, generatedCircuit);
		generatedCircuit.markAsCustom();
		GeneratedCircuitValidator validator(generatedCircuit, circuitManager->getBlockDataManager());
		circuitManager->updateExistingCircuit(pair.second, &generatedCircuit);
		circuitIdToProceduralCircuitParameters[pair.second] = realParameters;
		SharedCircuit circuit = circuitManager->getCircuit(pair.second);
		circuit->setCircuitName(getProceduralCircuitName() + " (" + realParameters.toString() + ")");
	}
}
