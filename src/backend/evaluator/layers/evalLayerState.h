#ifndef evalLayerState_h
#define evalLayerState_h

#include "../evalDefs.h"

struct EvalGate {
	EvalGate(EvalGateType type, eval_gate_id gateId) : type(type), gateId(gateId) { }

	EvalGateType type;
	eval_gate_id gateId;

	std::map<connection_end_id_t, std::unordered_set<EvalConnectionPoint>> connections;
};

class EvalLayerState {
public:
	EvalLayerState(IdProvider<eval_gate_id>& evalGateIdProvider) : evalGateIdProvider(evalGateIdProvider) {}

	void addGate(eval_gate_id gateId, EvalGateType type);
	void removeGate(eval_gate_id gateId);
	void addConnection(const EvalConnection& evalConnection, unsigned int weight);
	void removeConnection(const EvalConnection& evalConnection, unsigned int weight);
	void changeGateType(eval_gate_id gateId, EvalGateType newType);

 	const EvalGate* getGate(eval_gate_id gateId) const {
		auto iter = gates.find(gateId);
		if (iter == gates.end()) return nullptr;
		return &iter->second;
	}

	const IdMap<eval_gate_id, EvalGate>& getGates() const { return gates; }

	const IdMap<eval_gate_id, EvalGateType>& getAddedGates() const { return addedGates; }

	const IdMap<eval_gate_id, EvalGateType>& getRemovedGates() const { return removedGates; }

	const std::unordered_map<EvalConnection, unsigned int>& getAddedConnections() const { return addedConnections; }

	const std::unordered_map<EvalConnection, unsigned int>& getRemovedConnections() const { return removedConnections; }

	unsigned int getConnectionWeight(EvalConnection connection) const {
		auto iter = connectionWeights.find(connection);
		if (iter == connectionWeights.end()) {
			const EvalGate* gate = getGate(connection.connectionPointA.gateId);
			if (!gate) return 0;
			auto iter = gate->connections.find(connection.connectionPointA.connectionEndId);
			if (iter == gate->connections.end()) return 0;
			return iter->second.contains(connection.connectionPointB);
		}
		return iter->second;
	}
	void setConnectionWeight(EvalConnection connection, unsigned int weight) {
		assert(weight > 1);
		connectionWeights.insert_or_assign(connection, weight);
	}
	void addConnectionWeight(EvalConnection connection, int weightChange) {
		assert(weightChange != 0); // just dont
		auto iter = connectionWeights.find(connection);
		if (iter == connectionWeights.end()) {
			if (weightChange == 1) return;
			connectionWeights.emplace(connection, weightChange);
		}
		iter->second += weightChange;
		if (iter->second == 1) {
			connectionWeights.erase(iter);
		}
		assert(iter->second != 0);
	}
	void removeConnectionWeight(EvalConnection connection) {
		unsigned int deletedCount = connectionWeights.erase(connection);
		assert(deletedCount == 1);
	}

	void resetEdits() {
		// lastUnusedEvalGateId = std::numeric_limits<eval_gate_id::rep>::max() - 1; // -1 in case of math errors with this high number
		addedGates.clear();
		removedGates.clear();
		addedConnections.clear();
		removedConnections.clear();
		gateIdRemappingsUpdated.clear();
		connectionPointRemappingsUpdated.clear();
	}

	EvalLayerState& getOrMakeNextLayerState() {
		if (!nextLayerState) {
			nextLayerState = std::make_unique<EvalLayerState>(evalGateIdProvider);
			nextLayerState->setLastLayer(this);
		}
		return *nextLayerState;
	}
	EvalLayerState* getNextLayerState() {
		return nextLayerState.get();
	}
	const EvalLayerState* getNextLayerState() const {
		return nextLayerState.get();
	}
	const EvalLayerState* getLastLayerState() const {
		return lastLayerState;
	}
	IdMap<eval_gate_id, eval_gate_id>& getGateIdRemapping() { return gateIdRemapping; }
	const IdMap<eval_gate_id, eval_gate_id>& getGateIdRemapping() const { return gateIdRemapping; }
	IdMultiMap<eval_gate_id, eval_gate_id>& getGateIdReverseRemapping() { return gateIdReverseRemapping; }
	const IdMultiMap<eval_gate_id, eval_gate_id>& getGateIdReverseRemapping() const { return gateIdReverseRemapping; }
	std::unordered_map<EvalConnectionPoint, EvalConnectionPoint>& getConnectionPointRemapping() { return connectionPointRemapping; }
	const std::unordered_map<EvalConnectionPoint, EvalConnectionPoint>& getConnectionPointRemapping() const { return connectionPointRemapping; }
	std::unordered_multimap<EvalConnectionPoint, EvalConnectionPoint>& getConnectionPointReverseRemapping() { return connectionPointReverseRemapping; }
	const std::unordered_multimap<EvalConnectionPoint, EvalConnectionPoint>& getConnectionPointReverseRemapping() const { return connectionPointReverseRemapping; }
	std::unordered_set<EvalConnectionPoint>& getConnectionPointRemappingToNothing() { return connectionPointRemappingToNothing; }
	const std::unordered_set<EvalConnectionPoint>& getConnectionPointRemappingToNothing() const { return connectionPointRemappingToNothing; }

	void addGateIdRemappingsUpdated(eval_gate_id gateId) { gateIdRemappingsUpdated.insert(gateId); }
	void addConnectionPointRemappingsUpdated(EvalConnectionPoint evalConnectionPoint) { connectionPointRemappingsUpdated.insert(evalConnectionPoint); }
	const IdSet<eval_gate_id>& getGateIdRemappingsUpdateds() const { return gateIdRemappingsUpdated; }
	const std::unordered_set<EvalConnectionPoint>& getConnectionPointRemappingsUpdated() const { return connectionPointRemappingsUpdated; }

	eval_gate_id getUnusedEvalGateId() { return evalGateIdProvider.getNewId(); }
	void releaseUnusedEvalGateId(eval_gate_id evalGateId) { return evalGateIdProvider.releaseId(evalGateId); }

	void visualize() const;

private:
	void setLastLayer(const EvalLayerState* lastLayerState) { this->lastLayerState = lastLayerState; }

	IdProvider<eval_gate_id>& evalGateIdProvider;

	std::unique_ptr<EvalLayerState> nextLayerState;
	const EvalLayerState* lastLayerState;

	IdMap<eval_gate_id, EvalGate> gates;
	std::unordered_map<EvalConnection, unsigned int> connectionWeights;

	// all remappings will be eval_gate_id or EvalConnectionPoint
	IdMap<eval_gate_id, eval_gate_id> gateIdRemapping;
	IdMultiMap<eval_gate_id, eval_gate_id> gateIdReverseRemapping;
	std::unordered_map<EvalConnectionPoint, EvalConnectionPoint> connectionPointRemapping;
	std::unordered_multimap<EvalConnectionPoint, EvalConnectionPoint> connectionPointReverseRemapping;
	std::unordered_set<EvalConnectionPoint> connectionPointRemappingToNothing;

	IdMap<eval_gate_id, EvalGateType> addedGates;
	IdMap<eval_gate_id, EvalGateType> removedGates;
	std::unordered_map<EvalConnection, unsigned int> addedConnections;
	std::unordered_map<EvalConnection, unsigned int> removedConnections;

	IdSet<eval_gate_id> gateIdRemappingsUpdated;
	std::unordered_set<EvalConnectionPoint> connectionPointRemappingsUpdated;
};

#endif /* evalLayerState_h */
