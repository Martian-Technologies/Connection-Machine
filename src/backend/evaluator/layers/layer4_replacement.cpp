#include "layer4_replacement.h"

#include "layer4_replacer.h"

void Replacement::removeGate(
	SimPauseGuard& pauseGuard,
	middle_id_t gateId,
	std::unordered_map<connection_end_id_t, EvalConnectionPoint> replacementConnectionPoints) {
	isEmpty = false;
	// track connection removals
	std::vector<EvalConnection> outputs = replacer->busInterfacePassthrough.getOutputs(gateId);
	std::vector<EvalConnection> inputs = replacer->busInterfacePassthrough.getInputs(gateId);
	for (const auto& conn : outputs) {
		if (conn.destination.gateId != conn.source.gateId) {
			deletedConnections.push_back(conn);
		}
	}
	for (const auto& conn : inputs) {
		deletedConnections.push_back(conn);
	}
	deletedGates.push_back({ gateId, replacer->busInterfacePassthrough.getBlockType(gateId) });
	trackId(gateId);
	replacer->replacedConnectionPoints.insert({ gateId, replacementConnectionPoints });
	replacer->busInterfacePassthrough.removeGate(pauseGuard, gateId);
	trackReplacementLayer(gateId, layer);
}

void Replacement::removeGate(
	SimPauseGuard& pauseGuard,
	middle_id_t gateId,
	middle_id_t replacementId) {
	isEmpty = false;
	// track connection removals
	std::vector<EvalConnection> outputs = replacer->busInterfacePassthrough.getOutputs(gateId);
	std::vector<EvalConnection> inputs = replacer->busInterfacePassthrough.getInputs(gateId);
	for (const auto& conn : outputs) {
		if (conn.destination.gateId != conn.source.gateId) {
			deletedConnections.push_back(conn);
		}
	}
	for (const auto& conn : inputs) {
		deletedConnections.push_back(conn);
	}
	deletedGates.push_back({ gateId, replacer->busInterfacePassthrough.getBlockType(gateId) });
	trackId(gateId);
	replacer->replacedIds.insert({ gateId, replacementId });
	replacer->busInterfacePassthrough.removeGate(pauseGuard, gateId);
	trackReplacementLayer(replacementId, layer);
}

void Replacement::addGate(
	SimPauseGuard& pauseGuard,
	BlockType blockType,
	middle_id_t gateId) {
	isEmpty = false;
	replacer->busInterfacePassthrough.addGate(pauseGuard, blockType, gateId);
	// we don't need to track, because nothing can happen to this gate at a lower level
	addedGates.push_back({ gateId, blockType });
}

void Replacement::removeConnection(
	SimPauseGuard& pauseGuard,
	EvalConnection connection) {
	isEmpty = false;
	replacer->busInterfacePassthrough.removeConnection(pauseGuard, connection);
	trackId(connection.destination.gateId);
	trackId(connection.source.gateId);
	deletedConnections.push_back(connection);
}

void Replacement::makeConnection(
	SimPauseGuard& pauseGuard,
	EvalConnection connection) {
	isEmpty = false;
	replacer->busInterfacePassthrough.makeConnection(pauseGuard, connection);
	trackId(connection.destination.gateId);
	trackId(connection.source.gateId);
	addedConnections.push_back(connection);
}

void Replacement::overrideConnectionPoint(
	EvalConnectionPoint originalPoint,
	EvalConnectionPoint replacementPoint) {
	isEmpty = false;
	auto& gateOverrides = replacer->replacedConnectionPoints[originalPoint.gateId];
	std::optional<EvalConnectionPoint> previousPoint;
	auto it = gateOverrides.find(originalPoint.portId);
	if (it != gateOverrides.end()) {
		previousPoint = it->second;
	}
	overriddenConnectionPoints.push_back({ originalPoint.gateId, originalPoint.portId, previousPoint });
	gateOverrides[originalPoint.portId] = replacementPoint;
}

void Replacement::markIdAsReplaced(
	middle_id_t originalId,
	int overpowerLayer) {
	isEmpty = false;
	trackReplacementLayer(originalId, overpowerLayer);
}

middle_id_t Replacement::getNewId() {
	middle_id_t newId = replacer->middleIdProvider.getNewId();
	reservedIds.push_back(newId);
	trackReplacementLayer(newId, layer);
	return newId;
}

void Replacement::trackReplacementLayer(middle_id_t middleId, int layer) {
	auto insertResult = replacer->replacementIdLayers.insert({ middleId, layer });
	if (insertResult.second) {
		replacementLayerEntries.push_back({ middleId });
	}
}

void Replacement::revert(SimPauseGuard& pauseGuard) {
	if (isReverting) {
		return;
	}
	isReverting = true;
	isEmpty = true;
	for (const auto& conn : addedConnections) {
		replacer->pingId(pauseGuard, conn.source.gateId, layer+1);
		replacer->pingId(pauseGuard, conn.destination.gateId, layer+1);
	}
	for (const auto& conn : deletedConnections) {
		replacer->pingId(pauseGuard, conn.source.gateId, layer+1);
		replacer->pingId(pauseGuard, conn.destination.gateId, layer+1);
	}
	for (const auto& conn : addedGates) {
		replacer->pingId(pauseGuard, conn.id, layer+1);
		replacer->pingId(pauseGuard, conn.id, layer+1);
	}
	for (const auto& conn : addedConnections) {
		replacer->busInterfacePassthrough.removeConnection(pauseGuard, conn);
	}
	for (const auto& conn : addedGates) {
		replacer->busInterfacePassthrough.removeGate(pauseGuard, conn.id);
	}
	for (const auto& gate : deletedGates) {
		replacer->busInterfacePassthrough.addGate(pauseGuard, gate.type, gate.id);
		replacer->replacedConnectionPoints.erase(gate.id);
		replacer->replacedIds.erase(gate.id);
	}
	for (const auto& conn : deletedConnections) {
		replacer->busInterfacePassthrough.makeConnection(pauseGuard, conn);
	}
	for (auto it = overriddenConnectionPoints.rbegin(); it != overriddenConnectionPoints.rend(); ++it) {
		auto gateMapIt = replacer->replacedConnectionPoints.find(it->gateId);
		if (!it->previousPoint.has_value()) {
			if (gateMapIt != replacer->replacedConnectionPoints.end()) {
				gateMapIt->second.erase(it->portId);
				if (gateMapIt->second.empty()) {
					replacer->replacedConnectionPoints.erase(gateMapIt);
				}
			}
		} else {
			replacer->replacedConnectionPoints[it->gateId][it->portId] = *it->previousPoint;
		}
	}
	for (const auto& entry : replacementLayerEntries) {
		replacer->replacementIdLayers.erase(entry.id);
	}
	for (const auto& reservedId : reservedIds) {
		replacer->middleIdProvider.releaseId(reservedId);
	}
	auto callbacksWithPauseGuard = std::move(revertCallbacksWithPauseGuard);
	for (auto& callback : callbacksWithPauseGuard) {
		if (callback) {
			callback(pauseGuard);
		}
	}
	auto callbacks = std::move(revertCallbacks);
	for (auto& callback : callbacks) {
		if (callback) {
			callback();
		}
	}
	for (const auto idToTrack : idsToTrack) {
		if (replacer->dependentReplacements.contains(idToTrack)) {
			replacer->dependentReplacements.at(idToTrack).erase(id);
		}
	}

	addedConnections.clear();
	addedGates.clear();
	deletedConnections.clear();
	deletedGates.clear();
	reservedIds.clear();
	idsToTrack.clear();
	replacementLayerEntries.clear();
	overriddenConnectionPoints.clear();
	isReverting = false;
}

void Replacement::trackId(middle_id_t middleId) {
	replacer->dependentReplacements[middleId].insert(id);
	idsToTrack.insert(middleId);
}

nlohmann::json Replacement::dumpState() const {
	nlohmann::json stateJson;
	stateJson["id"] = id.get();
	stateJson["layer"] = layer;
	stateJson["isEmpty"] = isEmpty;
	stateJson["isReverting"] = isReverting;
	stateJson["addedGates"] = nlohmann::json::array();
	for (const auto& gate : addedGates) {
		stateJson["addedGates"].push_back(gate.dumpState());
	}
	stateJson["deletedGates"] = nlohmann::json::array();
	for (const auto& gate : deletedGates) {
		stateJson["deletedGates"].push_back(gate.dumpState());
	}
	stateJson["addedConnections"] = nlohmann::json::array();
	for (const auto& conn : addedConnections) {
		stateJson["addedConnections"].push_back(conn.dumpState());
	}
	stateJson["deletedConnections"] = nlohmann::json::array();
	for (const auto& conn : deletedConnections) {
		stateJson["deletedConnections"].push_back(conn.dumpState());
	}
	stateJson["reservedIds"] = nlohmann::json::array();
	for (const auto& id : reservedIds) {
		stateJson["reservedIds"].push_back(id.get());
	}
	stateJson["replacementLayerEntries"] = nlohmann::json::array();
	for (const auto& entry : replacementLayerEntries) {
		stateJson["replacementLayerEntries"].push_back(entry.dumpState());
	}
	stateJson["overriddenConnectionPoints"] = nlohmann::json::array();
	for (const auto& overrideEntry : overriddenConnectionPoints) {
		stateJson["overriddenConnectionPoints"].push_back(overrideEntry.dumpState());
	}
	stateJson["idsToTrack"] = nlohmann::json::array();
	for (const auto& id : idsToTrack) {
		stateJson["idsToTrack"].push_back(id.get());
	}
	return stateJson;
}

nlohmann::json Replacement::ReplacementLayerEntry::dumpState() const {
	return id.get();
}

nlohmann::json Replacement::ReplacementConnectionPointOverride::dumpState() const {
	nlohmann::json stateJson;
	stateJson["gateId"] = gateId.get();
	stateJson["portId"] = portId.get();
	if (previousPoint.has_value()) {
		stateJson["previousPoint"] = previousPoint->dumpState();
	} else {
		stateJson["previousPoint"] = nullptr;
	}
	return stateJson;
}

nlohmann::json ReplacementGate::dumpState() const {
	nlohmann::json stateJson;
	stateJson["id"] = id.get();
	stateJson["type"] = blocktype_to_string(type);
	return stateJson;
}
