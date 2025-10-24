#include "replacement.h"
#include "logicSimulator.h"

#include "replacer.h"

void Replacement::removeGate(
	SimPauseGuard& pauseGuard,
	middle_id_t gateId,
	std::unordered_map<connection_port_id_t, EvalConnectionPoint> replacementConnectionPoints) {
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
	return newId;
}

void Replacement::trackReplacementLayer(middle_id_t id, int layer) {
	auto insertResult = replacer->replacementIdLayers.insert({ id, layer });
	if (insertResult.second) {
		replacementLayerEntries.push_back({ id });
	}
}

void Replacement::revert(SimPauseGuard& pauseGuard) {
	if (isReverting) {
		return;
	}
	// logInfo("Reverting replacement at layer {} at depth {}", "Replacement::revert", layer, depth);
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
	for (const auto& id : reservedIds) {
		replacer->middleIdProvider.releaseId(id);
		replacer->replacementIdLayers.erase(id);
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
