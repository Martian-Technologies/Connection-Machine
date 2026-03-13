#ifndef simulatorDefs_h
#define simulatorDefs_h

#include "util/id.h"
#include "../evalDefs.h"
#include "simBlockData.h"

DECLARE_ID_TYPE(simulator_id_t, unsigned int);
DECLARE_ID_TYPE(gate_group_id_t, unsigned int);
DECLARE_ID_TYPE(simulator_state_reference, unsigned int);

typedef std::variant<simulator_state_reference, std::vector<simulator_state_reference>> SimulatorStateIndexVecVariant;

struct SimulatorMappingUpdate {
	SimulatorMappingUpdate(Position position, const SimulatorStateIndexVecVariant& simulatorIds) : position(position), simulatorIds(simulatorIds) {}
	SimulatorMappingUpdate(Position position, std::optional<virtual_connection_id_t> virtualConnectionId, const SimulatorStateIndexVecVariant& simulatorIds) :
		position(position), virtualConnectionId(virtualConnectionId), simulatorIds(simulatorIds) {}
	Position position;
	std::optional<virtual_connection_id_t> virtualConnectionId = std::nullopt;
	SimulatorStateIndexVecVariant simulatorIds;
};

typedef std::function<void(const std::vector<SimulatorMappingUpdate>&)> SimulatorMappingUpdateListenerFunction;

struct SimulatorMappingUpdateListener {
	Address address;
	std::function<void(const std::vector<SimulatorMappingUpdate>&)> callback;
};

struct SimulatorGate {
	eval_gate_id id;
	BlockType type;
    std::unordered_map<
        connection_end_id_t,
        std::unordered_map<EvalConnectionPoint, unsigned int> // weight, INPUT/OUTPUT only
	> connections;
	std::unordered_map<connection_end_id_t, std::unordered_map<EvalConnectionPoint, SimBlockData::InputOutput>> directionsOfBidirectionalPorts; // only used for bidirectional ports, otherwise direction is determined by block type

    std::unordered_map<EvalConnectionPoint, unsigned int>& getConnectionsFromPort(connection_end_id_t connectionEndId) {
        return connections[connectionEndId.get()];
	}
	bool hasConnectionsFromPort(connection_end_id_t connectionEndId) const {
		return connections.contains(connectionEndId.get());
	}
	const std::unordered_map<EvalConnectionPoint, unsigned int>& getConnectionsFromPort(connection_end_id_t connectionEndId) const {
		static const std::unordered_map<EvalConnectionPoint, unsigned int> emptyMap;
		auto it = connections.find(connectionEndId.get());
		if (it != connections.end()) {
			return it->second;
		}
		return emptyMap;
	}

	bool operator==(const SimulatorGate& other) const {
		return id == other.id && type == other.type && connections == other.connections;
	}

	SimBlockData::InputOutput getDirection(connection_end_id_t selfPort, EvalConnectionPoint otherPoint) const {
		SimBlockData::PortDirection selfPortDirection = SimBlockData::getPortDirection(type, selfPort);
		if (selfPortDirection == SimBlockData::PortDirection::INPUT) return SimBlockData::InputOutput::INPUT;
		if (selfPortDirection == SimBlockData::PortDirection::OUTPUT) return SimBlockData::InputOutput::OUTPUT;
		return directionsOfBidirectionalPorts.at(selfPort.get()).at(otherPoint);
	}
};

#endif /* simulatorDefs_h */