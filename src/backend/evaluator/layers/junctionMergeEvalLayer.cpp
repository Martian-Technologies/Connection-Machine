#include "junctionMergeEvalLayer.h"
#include "evalLayerState.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

bool EvalConnectionEndInfo::isConnectionEndIdSinglePin(EvalGateType gateType, connection_end_id_t connectionEndId) {
	// ignore lights, junctions, busses, custom blocks
	switch (getBlockType(gateType)) {
	case BlockType::SWITCH:
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
	case BlockType::BUFFER:
	case BlockType::NOT:
	case BlockType::CONSTANT_OFF:
	case BlockType::CONSTANT_ON:
	case BlockType::CONSTANT_X:
	case BlockType::CONSTANT_Z:
	case BlockType::TRISTATE_BUFFER:
		return true;
	case BlockType::AND:
	case BlockType::OR:
	case BlockType::XOR:
	case BlockType::NAND:
	case BlockType::NOR:
	case BlockType::XNOR:
		return connectionEndId == 1;
	default:
		return false;
	}
}

bool EvalConnectionEndInfo::isOutputConnectionPort(EvalGateType gateType, connection_end_id_t connectionEndId) {
	// ignore lights, junctions, busses, custom blocks
	switch (getBlockType(gateType)) {
	case BlockType::SWITCH:
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
	case BlockType::CONSTANT_OFF:
	case BlockType::CONSTANT_ON:
	case BlockType::CONSTANT_X:
	case BlockType::CONSTANT_Z:
		return true;
	case BlockType::TRISTATE_BUFFER:
		return connectionEndId == 2;
	case BlockType::BUFFER:
	case BlockType::NOT:
	case BlockType::AND:
	case BlockType::OR:
	case BlockType::XOR:
	case BlockType::NAND:
	case BlockType::NOR:
	case BlockType::XNOR:
		return connectionEndId == 1;
	default:
		return false;
	}
}

void JunctionMergeEvalLayer::run() {
	#ifdef TRACY_PROFILER
	ZoneScopedN("JunctionMerge Run");
	#endif
	// logInfo("Running layer {}", "", (unsigned long long)this);
	// currentState.visualize();
	std::unordered_set<EvalConnectionPoint> connectionPointsToScan;
	std::unordered_set<eval_gate_id> junctionGroupsToKill;
	for (auto iter : currentState.getRemovedConnections()) {
		EvalConnection connection = iter.first;
		auto connectionPointRemappingIterA = connectionPointRemapping.find(connection.connectionPointA);
		if (connectionPointRemappingIterA != connectionPointRemapping.end()) {
			connection.connectionPointA = EvalConnectionPoint(connectionPointRemappingIterA->second, 0);
		} else if (connection.connectionPointA.connectionEndId == 0) {
			auto remappingIterA = nextState.getGateIdRemapping().find(connection.connectionPointA.gateId);
			if (remappingIterA == nextState.getGateIdRemapping().end()) {
				// logError("Failed to find junction gate remapping for gate id {}.", "JunctionMergeEvalLayer::run", connection.connectionPointA.gateId);
				continue;
			}
			connection.connectionPointA.gateId = remappingIterA->second;
		}
		auto connectionPointRemappingIterB = connectionPointRemapping.find(connection.connectionPointB);
		if (connectionPointRemappingIterB != connectionPointRemapping.end()) {
			connection.connectionPointB = EvalConnectionPoint(connectionPointRemappingIterB->second, 0);
		} else if (connection.connectionPointB.connectionEndId == 0) {
			auto remappingIterB = nextState.getGateIdRemapping().find(connection.connectionPointB.gateId);
			if (remappingIterB == nextState.getGateIdRemapping().end()) {
				// logError("Failed to find junction gate remapping for gate id {}.", "JunctionMergeEvalLayer::run", connection.connectionPointB.gateId);
				continue;
			}
			connection.connectionPointB.gateId = remappingIterB->second;
		}
		if (connection.connectionPointB == connection.connectionPointA) {
			const EvalGate* nextLayerJunction = nextState.getGate(connection.connectionPointA.gateId);
			assert(nextLayerJunction);
			assert(isJunctionType(nextLayerJunction->type));

			auto iterPair = connectionPointReverseRemapping.equal_range(connection.connectionPointB.gateId);
			for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
				connectionPointRemapping.erase(reverseRemappingIter->second);
				connectionPointsToScan.insert(reverseRemappingIter->second);
			}
			connectionPointReverseRemapping.erase(iterPair.first, iterPair.second);
			auto iterPair2 = nextState.getGateIdReverseRemapping().equal_range(connection.connectionPointB.gateId);
			for (auto reverseRemappingIter = iterPair2.first; reverseRemappingIter != iterPair2.second; reverseRemappingIter++) {
				if (nextState.getGate(reverseRemappingIter->second)) nextState.removeGate(reverseRemappingIter->second);
				nextState.getGateIdRemapping().erase(reverseRemappingIter->second);
				connectionPointsToScan.emplace(reverseRemappingIter->second, 0);
			}
			nextState.getGateIdReverseRemapping().erase(iterPair2.first, iterPair2.second);
			assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());


			// junctionGroupsToKill.insert(connection.connectionPointA.gateId);
			// if (!currentState.getRemovedGates().contains(iter.first.connectionPointA.gateId)) connectionPointsToScan.insert(iter.first.connectionPointA);
			// if (!currentState.getRemovedGates().contains(iter.first.connectionPointB.gateId)) connectionPointsToScan.insert(iter.first.connectionPointB);
		} else {
			if (nextState.getConnectionWeight(connection)) nextState.removeConnection(connection, iter.second);
		}
	}
	for (auto iter : currentState.getRemovedGates()) {
		const EvalGate* gate = nextState.getGate(iter.first);
		if (gate == nullptr) { // if the gate was merged already
			assert(isJunctionType(iter.second));
			auto remappingIter = nextState.getGateIdRemapping().find(iter.first);
			if (remappingIter == nextState.getGateIdRemapping().end()) continue;
			// if (currentState.getAddedGates().contains(iter.first)) nextState.getGateIdRemapping().erase(remappingIter);

			auto iterPair = connectionPointReverseRemapping.equal_range(remappingIter->second);
			for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
				connectionPointRemapping.erase(reverseRemappingIter->second);
				connectionPointsToScan.insert(reverseRemappingIter->second);
			}
			connectionPointReverseRemapping.erase(iterPair.first, iterPair.second);
			auto iterPair2 = nextState.getGateIdReverseRemapping().equal_range(remappingIter->second);
			for (auto reverseRemappingIter = iterPair2.first; reverseRemappingIter != iterPair2.second; reverseRemappingIter++) {
				if (nextState.getGate(reverseRemappingIter->second)) nextState.removeGate(reverseRemappingIter->second);
				nextState.getGateIdRemapping().erase(reverseRemappingIter->second);
				connectionPointsToScan.emplace(reverseRemappingIter->second, 0);
			}
			nextState.getGateIdReverseRemapping().erase(iterPair2.first, iterPair2.second);
			assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());

			continue;
		}
		if (isJunctionType(gate->type)) { // if the gate had other gates merged into it
			auto iterPair = connectionPointReverseRemapping.equal_range(iter.first);
			for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
				connectionPointRemapping.erase(reverseRemappingIter->second);
				connectionPointsToScan.insert(reverseRemappingIter->second);
			}
			connectionPointReverseRemapping.erase(iterPair.first, iterPair.second);
			auto iterPair2 = nextState.getGateIdReverseRemapping().equal_range(iter.first);
			for (auto reverseRemappingIter = iterPair2.first; reverseRemappingIter != iterPair2.second; reverseRemappingIter++) {
				if (nextState.getGate(reverseRemappingIter->second)) nextState.removeGate(reverseRemappingIter->second);
				nextState.getGateIdRemapping().erase(reverseRemappingIter->second);
				connectionPointsToScan.emplace(reverseRemappingIter->second, 0);
			}
			nextState.getGateIdReverseRemapping().erase(iterPair2.first, iterPair2.second);
			assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());
			continue;
		}
		for (auto remappingIter = connectionPointRemapping.begin(); remappingIter != connectionPointRemapping.end(); ) {
			if (remappingIter->first.gateId == iter.first) {
				auto iterPair = connectionPointReverseRemapping.equal_range(remappingIter->second);
				for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
					if (reverseRemappingIter->second.gateId == iter.first) {
						connectionPointReverseRemapping.erase(reverseRemappingIter);
						break;
					}
				}
				remappingIter = connectionPointRemapping.erase(remappingIter);
			} else ++remappingIter;
		}
		assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());
		nextState.getGateIdRemapping().erase(iter.first);
		nextState.getGateIdReverseRemapping().erase(iter.first);
		nextState.removeGate(iter.first);
	}
	// logInfo("----------------------------------------------");
	// currentState.visualize();
	// nextState.visualize();
	for (auto iter : currentState.getAddedGates()) {
		if (isJunctionType(iter.second)) {
			connectionPointsToScan.emplace(iter.first, 0);
		} else {
			auto suc = nextState.getGateIdRemapping().try_emplace(iter.first, iter.first);
			assert(suc.second);
			nextState.getGateIdReverseRemapping().emplace(iter.first, iter.first);
			assert(nextState.getGateIdReverseRemapping().size() == nextState.getGateIdRemapping().size());
			nextState.addGate(iter.first, iter.second);
		}
	}
	// nextState.visualize();
	for (auto iter : currentState.getAddedConnections()) {
		EvalConnection connection = iter.first;
		unsigned int remappedTypeA = 0; // 0: None, 1: connection point, 2: junction id remapped, 3: already at merged junction
		auto connectionPointRemappingIterA = connectionPointRemapping.find(connection.connectionPointA);
		if (connectionPointRemappingIterA != connectionPointRemapping.end()) {
			const EvalGate* gate = nextState.getGate(connection.connectionPointA.gateId);
			assert(gate);
			assert(EvalConnectionEndInfo::isConnectionEndIdSinglePin(gate->type, connection.connectionPointA.connectionEndId));
			connection.connectionPointA = EvalConnectionPoint(connectionPointRemappingIterA->second, 0);
			remappedTypeA = 1;
		} else if (connection.connectionPointA.connectionEndId == 0) {
			const EvalGate* gate = nextState.getGate(connection.connectionPointA.gateId);
			if (gate) {
				if (isJunctionType(gate->type)) {
					remappedTypeA = 3;
				}
			} else {
				auto remappingAIter = nextState.getGateIdRemapping().find(connection.connectionPointA.gateId);
				if (remappingAIter == nextState.getGateIdRemapping().end()) {
					if (connectionPointsToScan.contains(connection.connectionPointA)) continue;
					// logError("Failed to find junction gate remapping for gate id {}.", "JunctionMergeEvalLayer::run", connection.connectionPointA.gateId);
					continue;
				}
				connection.connectionPointA.gateId = remappingAIter->second;
				remappedTypeA = 2;
			}
		}
		unsigned int remappedTypeB = 0; // 0: None, 1: connection point, 2: junction id remapped, 3: already at merged junction
		auto connectionPointRemappingIterB = connectionPointRemapping.find(connection.connectionPointB);
		if (connectionPointRemappingIterB != connectionPointRemapping.end()) {
			const EvalGate* gate = nextState.getGate(connection.connectionPointB.gateId);
			assert(gate);
			assert(EvalConnectionEndInfo::isConnectionEndIdSinglePin(gate->type, connection.connectionPointB.connectionEndId));
			connection.connectionPointB = EvalConnectionPoint(connectionPointRemappingIterB->second, 0);
			remappedTypeB = 1;
		} else if (connection.connectionPointB.connectionEndId == 0) {
			const EvalGate* gate = nextState.getGate(connection.connectionPointB.gateId);
			if (gate) {
				if (isJunctionType(gate->type)) {
					remappedTypeB = 3;
				}
			} else {
				auto remappingBIter = nextState.getGateIdRemapping().find(connection.connectionPointB.gateId);
				if (remappingBIter == nextState.getGateIdRemapping().end()) {
					if (connectionPointsToScan.contains(connection.connectionPointB)) continue;
					// logError("Failed to find junction gate remapping for gate id {}.", "JunctionMergeEvalLayer::run", connection.connectionPointB.gateId);
					continue;
				}
				connection.connectionPointB.gateId = remappingBIter->second;
				remappedTypeB = 2;
			}
		}
		if (connection.connectionPointB == connection.connectionPointA) continue;

		const EvalGate* gateA = nextState.getGate(connection.connectionPointA.gateId);
		assert(gateA);
		const EvalGate* gateB = nextState.getGate(connection.connectionPointB.gateId);
		assert(gateB);

		if (remappedTypeA != 0) assert(isJunctionType(gateA->type));
		if (remappedTypeB != 0) assert(isJunctionType(gateB->type));

		if (isJunctionType(gateA->type) && isJunctionType(gateB->type)) {
			connectionPointsToScan.insert(iter.first.connectionPointA);
			if (remappedTypeA != 0) junctionGroupsToKill.insert(connection.connectionPointA.gateId);
			if (remappedTypeB != 0) junctionGroupsToKill.insert(connection.connectionPointB.gateId);
		} else if (
			EvalConnectionEndInfo::isConnectionEndIdSinglePin(gateA->type, connection.connectionPointA.connectionEndId) &&
			EvalConnectionEndInfo::isConnectionEndIdSinglePin(gateB->type, connection.connectionPointB.connectionEndId)
		) {
			connectionPointsToScan.insert(iter.first.connectionPointA);
			assert(remappedTypeA == 0);
			assert(remappedTypeB == 0);
		} else if (isJunctionType(gateA->type) && EvalConnectionEndInfo::isConnectionEndIdSinglePin(gateB->type, connection.connectionPointB.connectionEndId)) {
			connectionPointsToScan.insert(iter.first.connectionPointA);
			if (remappedTypeA != 0) junctionGroupsToKill.insert(connection.connectionPointA.gateId);
		} else if (isJunctionType(gateB->type) && EvalConnectionEndInfo::isConnectionEndIdSinglePin(gateA->type, connection.connectionPointA.connectionEndId)) {
			connectionPointsToScan.insert(iter.first.connectionPointA);
			if (remappedTypeB != 0) junctionGroupsToKill.insert(connection.connectionPointB.gateId);
		} else {
			nextState.addConnection(connection, iter.second);
		}
	}
	for (eval_gate_id junctionId : junctionGroupsToKill) {
		auto iterPair = connectionPointReverseRemapping.equal_range(junctionId);
		for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
			connectionPointRemapping.erase(reverseRemappingIter->second);
			connectionPointsToScan.insert(reverseRemappingIter->second);
		}
		connectionPointReverseRemapping.erase(iterPair.first, iterPair.second);
		auto iterPair2 = nextState.getGateIdReverseRemapping().equal_range(junctionId);
		for (auto reverseRemappingIter = iterPair2.first; reverseRemappingIter != iterPair2.second; reverseRemappingIter++) {
			if (nextState.getGate(reverseRemappingIter->second)) nextState.removeGate(reverseRemappingIter->second);
			nextState.getGateIdRemapping().erase(reverseRemappingIter->second);
			connectionPointsToScan.emplace(reverseRemappingIter->second, 0);
		}
		nextState.getGateIdReverseRemapping().erase(iterPair2.first, iterPair2.second);
		assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());
	}
	// nextState.visualize();
	while (!connectionPointsToScan.empty()) {
		EvalConnectionPoint connectionPointToScanFrom = *connectionPointsToScan.begin();
		connectionPointsToScan.erase(connectionPointsToScan.begin());
		{
			const EvalGate* curGate = currentState.getGate(connectionPointToScanFrom.gateId);
			if (!curGate) continue;
			if (!(isJunctionType(curGate->type) || EvalConnectionEndInfo::isConnectionEndIdSinglePin(curGate->type, connectionPointToScanFrom.connectionEndId))) {
				// logError("This is a bug and should not happen. Tho it should not break anything I dont want it to happen!"); // for now this will happen as I just throw alot at connectionPointsToScan
				continue;
			}
			if (isJunctionType(curGate->type) && connectionPointToScanFrom.connectionEndId != connection_end_id_t(0)) continue;
		}
		auto [junctions, singlePinConnectionPoints, nonJunctionConnectionPoints, gateType] = gatherJunctionGroup(connectionPointToScanFrom, currentState);
		assert(junctions.size() > 0 || singlePinConnectionPoints.size() > 0);
		std::optional<EvalConnectionPoint> outputConnectionPoint = junctions.empty() ? std::optional<EvalConnectionPoint>(EvalConnectionPoint::null()) : std::nullopt;
		for (EvalConnectionPoint connectionPoint : singlePinConnectionPoints) {
			auto connectionPointRemappingIter = connectionPointRemapping.find(connectionPoint);
			if (connectionPointRemappingIter != connectionPointRemapping.end()) {
				auto iterPair = connectionPointReverseRemapping.equal_range(connectionPointRemappingIter->second);
				for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
					connectionPointRemapping.erase(reverseRemappingIter->second);
				}
				connectionPointReverseRemapping.erase(iterPair.first, iterPair.second);
			}
			const EvalGate* gate = nextState.getGate(connectionPoint.gateId);
			assert(gate);
			if (outputConnectionPoint.has_value()) {
				if (EvalConnectionEndInfo::isOutputConnectionPort(gate->type, connectionPoint.connectionEndId)) {
					if (outputConnectionPoint->isNull()) outputConnectionPoint = connectionPoint;
					else outputConnectionPoint = std::nullopt;
				}
			}
			while (true) {
				auto iter = gate->connections.find(connectionPoint.connectionEndId);
				if (iter == gate->connections.end()) break;
				EvalConnectionPoint otherConnectionPoint = *iter->second.begin();
				unsigned int weight = nextState.getConnectionWeight(EvalConnection(connectionPoint, otherConnectionPoint));
				assert(weight != 0);
				nextState.removeConnection(EvalConnection(connectionPoint, otherConnectionPoint), weight);
			}
		}
		if (outputConnectionPoint.has_value()) { // we can not add any junctions (this also means that one of the points is a output)
			EvalConnectionPoint connectionPoint = outputConnectionPoint.value();
			for (std::pair<EvalConnectionPoint, unsigned int> pair : nonJunctionConnectionPoints) {
				connectionPointsToScan.erase(pair.first);
				// connectionPoint is null when the group has no driving output (e.g. an
				// input sharing a bus port whose only driver was just removed); there is
				// nothing to wire from, so leave the input unconnected.
				if (connectionPoint.isNull() || pair.first == connectionPoint) continue;
				nextState.addConnection(EvalConnection(connectionPoint, pair.first), pair.second);
			}
			continue;
		}

		eval_gate_id mergedGateId = std::numeric_limits<eval_gate_id::rep>::max();
		if (junctions.empty()) {
			// create a junction if needed
			mergedGateId = nextState.getUnusedEvalGateId();
		} else {
			// We do sorting to see which gate would be the best to place down.
			bool typeMatches = false;
			for (eval_gate_id junctionId : junctions) {
				if (junctionId < mergedGateId) {
					if (currentState.getGate(junctionId)->type == gateType) {
						typeMatches = true;
						mergedGateId = junctionId;
					} else if (!typeMatches) {
						mergedGateId = junctionId;
					}
				} else if ((!typeMatches) && currentState.getGate(junctionId)->type == gateType) {
					typeMatches = true;
					mergedGateId = junctionId;
				}
			}
		}
		for (std::pair<EvalConnectionPoint, unsigned int> connectionPoint : nonJunctionConnectionPoints) {
			connectionPointsToScan.erase(connectionPoint.first);
			nextState.addConnectionPointRemappingsUpdated(connectionPoint.first);
			if (singlePinConnectionPoints.contains(connectionPoint.first)) {
				auto suc = connectionPointRemapping.emplace(connectionPoint.first, mergedGateId);
				assert(suc.second);
				connectionPointReverseRemapping.emplace(mergedGateId, connectionPoint.first);
			}
		}
		for (eval_gate_id junctionId : junctions) {
			connectionPointsToScan.erase(EvalConnectionPoint(junctionId, 0));
			auto remappingIter = nextState.getGateIdRemapping().find(junctionId);
			if (remappingIter != nextState.getGateIdRemapping().end()) {
				auto iterPair = nextState.getGateIdReverseRemapping().equal_range(remappingIter->second);
				for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
					nextState.getGateIdRemapping().erase(reverseRemappingIter->second);
					if (nextState.getGate(reverseRemappingIter->second)) nextState.removeGate(reverseRemappingIter->second);
				}
				nextState.getGateIdReverseRemapping().erase(iterPair.first, iterPair.second);
			}
		}
		for (eval_gate_id junctionId : junctions) {
			auto suc = nextState.getGateIdRemapping().try_emplace(junctionId, mergedGateId);
			assert(suc.second);
			nextState.getGateIdReverseRemapping().emplace(mergedGateId, junctionId);
			assert(nextState.getGateIdReverseRemapping().size() == nextState.getGateIdRemapping().size());
		}
		nextState.addGateIdRemappingsUpdated(mergedGateId);
		nextState.addGate(mergedGateId, gateType);
		for (std::pair<EvalConnectionPoint, unsigned int> pair : nonJunctionConnectionPoints) {
			nextState.addConnection(EvalConnection(EvalConnectionPoint(mergedGateId, 0), pair.first), pair.second);
		}
	}
	for (eval_gate_id gateId : currentState.getGateIdRemappingsUpdateds()) {
		auto iter = nextState.getGateIdRemapping().find(gateId);
		if (iter == nextState.getGateIdRemapping().end()) {
			logError("could not find eval id while proagating id {} update.", "JunctionMergeEvalLayer::run", gateId);
			continue;
		}
		assert(nextState.getGate(iter->second));
		nextState.addGateIdRemappingsUpdated(iter->second);
	}
	for (EvalConnectionPoint connectionPoint : currentState.getConnectionPointRemappingsUpdated()) {
		if (connectionPoint.connectionEndId == 0) {
			auto iter = nextState.getGateIdRemapping().find(connectionPoint.gateId);
			if (iter == nextState.getGateIdRemapping().end()) {
				logError("Could not find eval id while proagating connectionPoint {} update.", "JunctionMergeEvalLayer::run", connectionPoint);
				continue;
			}
			connectionPoint.gateId = iter->second;
		}
		nextState.addConnectionPointRemappingsUpdated(connectionPoint);
	}
	assert(connectionPointRemapping.size() == connectionPointReverseRemapping.size());
	// nextState.visualize();
}

std::tuple<
	std::vector<eval_gate_id>,
	std::unordered_set<EvalConnectionPoint>,
	std::unordered_map<EvalConnectionPoint, unsigned int>,
	EvalGateType
> JunctionMergeEvalLayer::gatherJunctionGroup(EvalConnectionPoint start, const EvalLayerState& layerState) const {
	const EvalGate* startGate = layerState.getGate(start.gateId);
	assert(startGate);

	std::vector<eval_gate_id> junctions;
	std::unordered_map<EvalConnectionPoint, unsigned int> nonJunctionConnectionPoints;
	std::unordered_set<EvalConnectionPoint> singlePinConnectionPoints;

	bool foundPullDown = false;
	bool foundPullUp = false;

	if (isJunctionType(startGate->type) ) {
		junctions.push_back(start.gateId);
		if (startGate->type == getEvalGateType(BlockType::JUNCTION_L)) foundPullDown = true;
		else if (startGate->type == getEvalGateType(BlockType::JUNCTION_H)) foundPullUp = true;
		else if (startGate->type == getEvalGateType(BlockType::JUNCTION_X)) foundPullUp = foundPullDown = true;
	} else if (EvalConnectionEndInfo::isConnectionEndIdSinglePin(startGate->type, start.connectionEndId)) {
		singlePinConnectionPoints.insert(start);
	} else {
		assert(false);
	}

	std::unordered_set<EvalConnectionPoint> visited;
	std::unordered_set<EvalConnection> visitedEdges;
	std::deque<EvalConnectionPoint> queue;
	queue.push_back(start);
	visited.insert(start);


	while (!queue.empty()) {
		EvalConnectionPoint connectionPoint = queue.front();
		queue.pop_front();

		const EvalGate* gate = layerState.getGate(connectionPoint.gateId);
		assert(gate);
		auto connectionIter = gate->connections.find(connectionPoint.connectionEndId);
		if (connectionIter == gate->connections.end())continue;
		for (const EvalConnectionPoint& otherConnectionPoint : connectionIter->second) {
			const EvalGate* otherGate = layerState.getGate(otherConnectionPoint.gateId);
			assert(otherGate);

			EvalConnection connection(connectionPoint, otherConnectionPoint);
			unsigned int weight = layerState.getConnectionWeight(connection);
			assert(weight != 0);

			if (isJunctionType(otherGate->type)) {
				assert(otherConnectionPoint.connectionEndId == 0);
				if (visited.insert(otherConnectionPoint).second) {
					junctions.push_back(otherConnectionPoint.gateId);
					queue.push_back(otherConnectionPoint);

					if (otherGate->type == getEvalGateType(BlockType::JUNCTION_L)) foundPullDown = true;
					else if (otherGate->type == getEvalGateType(BlockType::JUNCTION_H)) foundPullUp = true;
					else if (otherGate->type == getEvalGateType(BlockType::JUNCTION_X)) foundPullUp = foundPullDown = true;
				}
			} else if (EvalConnectionEndInfo::isConnectionEndIdSinglePin(otherGate->type, otherConnectionPoint.connectionEndId)) {
				if (visited.insert(otherConnectionPoint).second) {
					queue.push_back(otherConnectionPoint);
					singlePinConnectionPoints.insert(otherConnectionPoint);
				}
				if (visitedEdges.insert(connection).second) {
					if (!isJunctionType(gate->type)) nonJunctionConnectionPoints[connectionPoint] += weight;
					nonJunctionConnectionPoints[otherConnectionPoint] += weight;
				}
			} else {
				if (visitedEdges.insert(connection).second) {
					if (!isJunctionType(gate->type)) nonJunctionConnectionPoints[connectionPoint] += weight;
					nonJunctionConnectionPoints[otherConnectionPoint] += weight;
				}
			}
		}
	}

	EvalGateType resultType = getEvalGateType(BlockType::JUNCTION);
	if (foundPullDown && foundPullUp) resultType = getEvalGateType(BlockType::JUNCTION_X);
	else if (foundPullDown) resultType = getEvalGateType(BlockType::JUNCTION_L);
	else if (foundPullUp) resultType = getEvalGateType(BlockType::JUNCTION_H);

	return { junctions, singlePinConnectionPoints, nonJunctionConnectionPoints, resultType };
}
