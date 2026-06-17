#include "evalLayerState.h"

void EvalLayerState::addGate(eval_gate_id gateId, EvalGateType type) { // TODO: add transparent junction merging
	bool suc = gates.try_emplace(gateId, type, gateId).second;
	assert(suc);
	auto removedGatesIter = removedGates.find(gateId);
	if (removedGatesIter == removedGates.end() || removedGatesIter->second != type) {
		addedGates.try_emplace(gateId, type);
	} else {
		removedGates.erase(removedGatesIter);
	}
}

void EvalLayerState::removeGate(eval_gate_id gateId) { // TODO: add transparent junction splitting
	auto iter = gates.find(gateId);
	assert(iter != gates.end());
	for (auto connectionsIter : iter->second.connections) {
		for (EvalConnectionPoint connectionPoint : connectionsIter.second) {
			EvalConnection connection = EvalConnection(EvalConnectionPoint(gateId, connectionsIter.first), connectionPoint);
			auto gateBIterBoolPair = gates.find(connectionPoint.gateId);
			assert(gateBIterBoolPair != gates.end());
			auto gateBConnectionIter = gateBIterBoolPair->second.connections.find(connectionPoint.connectionEndId);
			assert(gateBConnectionIter->second.contains(EvalConnectionPoint(gateId, connectionsIter.first)));
			if (gateBConnectionIter->second.size() == 1) gateBIterBoolPair->second.connections.erase(gateBConnectionIter);
			else gateBConnectionIter->second.erase(EvalConnectionPoint(gateId, connectionsIter.first));
			auto connectionWeightIter = connectionWeights.find(connection);
			unsigned int weight = 1;
			if (connectionWeightIter != connectionWeights.end()) {
				weight = connectionWeightIter->second;
				connectionWeights.erase(connectionWeightIter);
			}
			auto addedConnectionIter = addedConnections.find(connection);
			if (addedConnectionIter == addedConnections.end()) {
				removedConnections[connection] += weight;
			} else if (addedConnectionIter->second == weight) {
				addedConnections.erase(addedConnectionIter);
			} else {
				assert(addedConnectionIter->second < weight);
				removedConnections.try_emplace(connection, weight - addedConnectionIter->second);
				addedConnections.erase(addedConnectionIter);
			}
		}
	}
	auto addedGatessIter = addedGates.find(gateId);
	if (addedGatessIter == addedGates.end()) {
		removedGates.try_emplace(gateId, iter->second.type);
	} else {
		assert(addedGatessIter->second == iter->second.type);
		addedGates.erase(addedGatessIter);
	}
	gates.erase(iter);
}

void EvalLayerState::addConnection(const EvalConnection& evalConnection, unsigned int weight) { // TODO: add transparent junction merging
	assert(weight != 0);
	auto gateAIterBoolPair = gates.find(evalConnection.connectionPointA.gateId);
	assert(gateAIterBoolPair != gates.end());
	auto connectionsIter = gateAIterBoolPair->second.connections.find(evalConnection.connectionPointA.connectionEndId);
	if (connectionsIter == gateAIterBoolPair->second.connections.end()) { // connection id does not have connections
		gateAIterBoolPair->second.connections.try_emplace(
			evalConnection.connectionPointA.connectionEndId,
			std::unordered_set<EvalConnectionPoint>({evalConnection.connectionPointB})
		);
		if (weight != 1) connectionWeights.try_emplace(evalConnection, weight);
	} else { // connection id has connections
		auto connectionIter = connectionsIter->second.find(evalConnection.connectionPointB);
		if (connectionIter == connectionsIter->second.end()) {
			connectionsIter->second.insert(evalConnection.connectionPointB);
			if (weight != 1) connectionWeights.try_emplace(evalConnection, weight);
		} else {
			auto connectionWeightIter = connectionWeights.find(evalConnection);
			if (connectionWeightIter == connectionWeights.end()) { connectionWeights.try_emplace(evalConnection, weight + 1); }
			else connectionWeightIter->second += weight;
		}
	}
	auto gateBIterBoolPair = gates.find(evalConnection.connectionPointB.gateId);
	assert(gateBIterBoolPair != gates.end());
	gateBIterBoolPair->second.connections[evalConnection.connectionPointB.connectionEndId].insert(evalConnection.connectionPointA);

	if (addedGates.contains(evalConnection.connectionPointA.gateId) || addedGates.contains(evalConnection.connectionPointB.gateId)) {
		addedConnections[evalConnection] += weight;
	} else {
		auto removedConnectionIter = removedConnections.find(evalConnection);
		if (removedConnectionIter == removedConnections.end()) {
			addedConnections[evalConnection] += weight;
		} else if (removedConnectionIter->second < weight) {
			addedConnections.try_emplace(evalConnection, weight - removedConnectionIter->second);
			removedConnections.erase(removedConnectionIter);
		} else if (removedConnectionIter->second == weight) {
			removedConnections.erase(removedConnectionIter);
		} else {
			removedConnectionIter->second -= weight;
		}
	}
}

void EvalLayerState::removeConnection(const EvalConnection& connection, unsigned int weight) { // TODO: add transparent junction splitting
	assert(weight != 0);
	bool removeConnection = true;
	unsigned int currentWeight = 1;
	auto connectionWeightIter = connectionWeights.find(connection);
	if (connectionWeightIter == connectionWeights.end()) {
		assert(weight == 1);
	} else if (connectionWeightIter->second == weight) {
		currentWeight = weight;
		connectionWeights.erase(connectionWeightIter);
	} else {
		currentWeight = connectionWeightIter->second;
		assert(connectionWeightIter->second > weight);
		connectionWeightIter->second -= weight;
		if (connectionWeightIter->second == 1) connectionWeights.erase(connectionWeightIter);
		removeConnection = false;
	}

	if (removeConnection) {
		auto gateAIterBoolPair = gates.find(connection.connectionPointA.gateId);
		assert(gateAIterBoolPair != gates.end());
		auto gateAConnectionIter = gateAIterBoolPair->second.connections.find(connection.connectionPointA.connectionEndId);
		assert(gateAConnectionIter != gateAIterBoolPair->second.connections.end());
		assert(gateAConnectionIter->second.contains(connection.connectionPointB));
		if (gateAConnectionIter->second.size() == 1) gateAIterBoolPair->second.connections.erase(gateAConnectionIter);
		else gateAConnectionIter->second.erase(connection.connectionPointB);

		auto gateBIterBoolPair = gates.find(connection.connectionPointB.gateId);
		assert(gateBIterBoolPair != gates.end());
		auto gateBConnectionIter = gateBIterBoolPair->second.connections.find(connection.connectionPointB.connectionEndId);
		assert(gateBConnectionIter != gateBIterBoolPair->second.connections.end());
		assert(gateBConnectionIter->second.contains(connection.connectionPointA));
		if (gateBConnectionIter->second.size() == 1) gateBIterBoolPair->second.connections.erase(gateBConnectionIter);
		else gateBConnectionIter->second.erase(connection.connectionPointA);
	}

	auto addedConnectionIter = addedConnections.find(connection);
	if (addedConnectionIter == addedConnections.end()) {
		removedConnections[connection] += weight;
	} else if (addedConnectionIter->second == weight) {
		addedConnections.erase(addedConnectionIter);
	} else if (addedConnectionIter->second < weight) {
		removedConnections.try_emplace(connection, weight - addedConnectionIter->second);
		addedConnections.erase(addedConnectionIter);
	} else {
		addedConnectionIter->second -= weight;
	}
}

void EvalLayerState::changeGateType(eval_gate_id gateId, EvalGateType newType) {
	auto gatesIter = gates.find(gateId);
	EvalGateType oldType = gatesIter->second.type;
	if (oldType == newType) return;
	gatesIter->second.type = newType;
	auto addedGatesPair = addedGates.insert_or_assign(gateId, newType);
	if (!addedGatesPair.second) return;
	removedGates.try_emplace(gateId, oldType);
	for (const auto& connectionsPair : gatesIter->second.connections) {
		for (const EvalConnectionPoint& otherConnectionPoint : connectionsPair.second) {
			EvalConnection evalConnection(EvalConnectionPoint(gateId, connectionsPair.first), otherConnectionPoint);
			auto weightIter = connectionWeights.find(evalConnection);
			if (weightIter == connectionWeights.end()) {
				auto addedConnectionIter = addedConnections.find(evalConnection);
				if (addedConnectionIter != addedConnections.end()) {
					removedConnections.try_emplace(evalConnection, 1);
					addedConnectionIter->second += 1;
					continue;
				}
				auto removedConnectionIter = removedConnections.find(evalConnection);
				if (removedConnectionIter != removedConnections.end()) {
					removedConnectionIter->second += 1;
					addedConnections.try_emplace(evalConnection, 1);
					continue;
				}
				removedConnections.try_emplace(evalConnection, 1);
				addedConnections.try_emplace(evalConnection, 1);
			} else {
				auto addedConnectionIter = addedConnections.find(evalConnection);
				if (addedConnectionIter != addedConnections.end()) {
					removedConnections.try_emplace(evalConnection, weightIter->second);
					addedConnectionIter->second += weightIter->second;
					continue;
				}
				auto removedConnectionIter = removedConnections.find(evalConnection);
				if (removedConnectionIter != removedConnections.end()) {
					removedConnectionIter->second += weightIter->second;
					addedConnections.try_emplace(evalConnection, weightIter->second);
					continue;
				}
				removedConnections.try_emplace(evalConnection, weightIter->second);
				addedConnections.try_emplace(evalConnection, weightIter->second);
			}
		}
	}
}

void EvalLayerState::visualize() const {
	logInfo("=== Eval Layer State {} ===", "", (unsigned long long)this);
	logInfo("Last: {}   Next: {}", "", (unsigned long long)lastLayerState, (unsigned long long)nextLayerState.get());
	logInfo("{} Gates", "", gates.size());
	for (auto gatePair : gates) {
		logInfo("Gate Id {}, Type: {}", "", gatePair.second.gateId, gatePair.second.type);
		for (auto connPair : gatePair.second.connections) {
			std::stringstream ss;
			for (EvalConnectionPoint connectionPoint : connPair.second) {
				ss << "(" << connectionPoint.gateId.get() << ", " << connectionPoint.connectionEndId.get() << ") ";
			}
			logInfo("End Id {}: {}", "", connPair.first.get(), ss.str());
		}
	}
	std::string tmpBuf;
	for (auto pair : connectionWeights) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.first) + " " + std::to_string(pair.second) + "]";
	}
	logInfo("connectionWeights: {}", "", tmpBuf);
	logInfo("{} Gate Remapping", "", gateIdRemapping.size());
	for (auto pair : gateIdRemapping) {
		logInfo("{} -> {}", "", pair.first, pair.second);
	}
	logInfo("{} Gate Reverse Remapping", "", gateIdReverseRemapping.size());
	eval_gate_id curKey = 0;
	tmpBuf.clear();
	for (auto pair : gateIdReverseRemapping) {
		if (curKey != pair.first) {
			if (curKey != 0) logInfo("{} -> [{}]", "", curKey, tmpBuf);
			tmpBuf.clear();
			curKey = pair.first;
		}
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += std::to_string(pair.second);
	}
	if (curKey != 0) logInfo("{} -> [{}]", "", curKey, tmpBuf);
	logInfo("{} Port Remapping", "", connectionPointRemapping.size());
	for (auto pair : connectionPointRemapping) {
		logInfo("({}, {}) -> ({}, {})", "", pair.first.gateId, pair.first.connectionEndId, pair.second.gateId, pair.second.connectionEndId);
	}
	logInfo("{} Port Reverse Remapping", "", connectionPointReverseRemapping.size());
	for (auto pair : connectionPointReverseRemapping) {
		logInfo("({}, {}) -> ({}, {})", "", pair.first.gateId, pair.first.connectionEndId, pair.second.gateId, pair.second.connectionEndId);
	}
	tmpBuf.clear();
	for (auto pair : connectionPointRemappingToNothing) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.gateId) + " " + fmt::to_string(pair.connectionEndId) + "]";
	}
	logInfo("{} Remapping to Nothing: {}", "", connectionPointRemappingToNothing.size(), tmpBuf);
	tmpBuf.clear();
	for (auto pair : addedGates) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.first) + " " + fmt::to_string((unsigned int)pair.second) + "]";
	}
	logInfo("{} addedGates: {}", "", addedGates.size(), tmpBuf);
	tmpBuf.clear();
	for (auto pair : removedGates) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.first) + " " + fmt::to_string((unsigned int)pair.second) + "]";
	}
	logInfo("{} removedGates: {}", "", removedGates.size(), tmpBuf);
	tmpBuf.clear();
	for (std::pair<EvalConnection, unsigned int> pair : addedConnections) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.first) + " " + fmt::to_string(pair.second) + "]";
	}
	logInfo("{} addedConnections: {}", "", addedConnections.size(), tmpBuf);
	tmpBuf.clear();
	for (std::pair<EvalConnection, unsigned int> pair : removedConnections) {
		if (tmpBuf.size() != 0) tmpBuf += ", ";
		tmpBuf += "[" + fmt::to_string(pair.first) + " " + fmt::to_string(pair.second) + "]";
	}
	logInfo("{} removedConnections: {}", "", removedConnections.size(), tmpBuf);
}
