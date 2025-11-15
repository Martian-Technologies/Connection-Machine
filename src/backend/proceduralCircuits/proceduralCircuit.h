#ifndef proceduralCircuit_h
#define proceduralCircuit_h

#include "backend/dataUpdateEventManager.h"
#include "backend/circuit/circuit.h"

class CircuitManager;
class CircuitBlockData;
class GeneratedCircuit;

struct ProceduralCircuitParameters {
	ProceduralCircuitParameters() { }
	ProceduralCircuitParameters(const ProceduralCircuitParameters& other) : parameters(other.parameters) { }
	ProceduralCircuitParameters(ProceduralCircuitParameters&& other) : parameters(std::move(other.parameters)) { }

	ProceduralCircuitParameters(std::istream& ss);

	ProceduralCircuitParameters& operator=(const ProceduralCircuitParameters& other);
	ProceduralCircuitParameters& operator=(ProceduralCircuitParameters&& other);

	std::string toString() const;

	bool operator==(const ProceduralCircuitParameters& other) const {
		return parameters == other.parameters;
	}

	std::map<std::string, int> parameters;

	nlohmann::json dumpState() const;
};

template<>
struct std::hash<ProceduralCircuitParameters> {
	inline std::size_t operator()(const ProceduralCircuitParameters& parameters) const noexcept {
		std::size_t seed = 0;
		for (const auto& iter : parameters.parameters) {
			std::size_t a = std::hash<std::string> {}(iter.first);
			std::size_t b = std::hash<int> {}(iter.second);
			seed = a + 0x9e3779b9 + (seed << 6) + (seed >> 2); // this is what boost::hash_combine does
			seed = b + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

class ProceduralCircuit {
public:
	ProceduralCircuit(
		CircuitManager& circuitManager,
		DataUpdateEventManager& dataUpdateEventManager,
		const std::string& name,
		const std::string& uuid
	);
	ProceduralCircuit(const ProceduralCircuit&) = delete;
    ProceduralCircuit& operator=(const ProceduralCircuit&) = delete;
	ProceduralCircuit(ProceduralCircuit&& other);
	virtual ~ProceduralCircuit();

	inline std::string getPath() const { return "Procedural Circuits/" + getProceduralCircuitName(); }

	inline const std::string& getUUID() const { return proceduralCircuitUUID; }
	inline std::string getProceduralCircuitNameUUID() const { return proceduralCircuitName + " : " + proceduralCircuitUUID; }
	inline const std::string& getProceduralCircuitName() const { return proceduralCircuitName; }
	void setProceduralCircuitName(const std::string& name);

	inline void setParameterDefaults(const ProceduralCircuitParameters& parameterDefaults) { this->parameterDefaults = parameterDefaults; regenerateAll(); }
	const ProceduralCircuitParameters& getParameterDefaults() const { return this->parameterDefaults; }
	const ProceduralCircuitParameters* getProceduralCircuitParameters(circuit_id_t circuitId) const;
	circuit_id_t getCircuitId(const ProceduralCircuitParameters& parameters);
	BlockType getBlockType(const ProceduralCircuitParameters& parameters);
	nlohmann::json dumpState() const;

protected:
	virtual void makeCircuit(const ProceduralCircuitParameters& parameters, GeneratedCircuit& generatedCircuit) = 0;
	void regenerateAll();
	virtual nlohmann::json dumpStateInherited() const = 0;

private:
	std::string proceduralCircuitName;
	std::string proceduralCircuitUUID;

	ProceduralCircuitParameters	parameterDefaults;

	CircuitManager& circuitManager;
	std::unordered_map<ProceduralCircuitParameters, circuit_id_t> generatedCircuits;
	std::unordered_map<circuit_id_t, ProceduralCircuitParameters> circuitIdToProceduralCircuitParameters;

	DataUpdateEventManager& dataUpdateEventManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
};

typedef std::shared_ptr<ProceduralCircuit> SharedProceduralCircuit;

#endif /* proceduralCircuit_h */
