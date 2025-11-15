#include "layer4_replacer.h"

#include "layer4_replacement.h"

Replacement& Replacer::makeReplacement(int layer) {
	// replacements.emplace_back(
	// 	this,
	// 	layer
	// );
	// return replacements.back();
	replacement_id_t newId = replacementIdProvider.getNewId();
	if (replacements.size() <= newId) {
		replacements.resizeWithOffset(newId, 1, std::nullopt);
	}
	replacements[newId] = Replacement(
		this,
		layer,
		newId
	);
	return *replacements[newId];
}

void Replacer::cleanReplacements() {
	// for (auto it = replacements.begin(); it != replacements.end();) {
	// 	if (it->getIsEmpty()) {
	// 		it = replacements.erase(it);
	// 	} else {
	// 		++it;
	// 	}
	// }
	for (replacement_id_t id : replacementIdProvider.getUsedIds()) {
		if (replacements[id] && replacements[id]->getIsEmpty()) {
			replacements[id] = std::nullopt;
			replacementIdProvider.releaseId(id);
		}
	}
}

void Replacer::pingId(SimPauseGuard& pauseGuard, middle_id_t id, int minLayer) {
	// for (Replacement& replacement : replacements) {
	// 	if (replacement.getLayer() < minLayer) {
	// 		continue;
	// 	}
	// 	replacement.pingId(pauseGuard, id);
	// }
	auto node = dependentReplacements.extract(id);
	if (node.empty()) {
		return;
	}
	std::set<replacement_id_t>& replacementsSet = node.mapped();
	std::set<replacement_id_t> remaining;
	std::vector<replacement_id_t> toRevert;
	for (replacement_id_t replacementId : replacementsSet) {
		if (replacements[replacementId]->getLayer() < minLayer) {
			remaining.insert(replacementId);
		} else {
			toRevert.push_back(replacementId);
		}
	}
	if (!remaining.empty()) {
		replacementsSet = std::move(remaining);
		dependentReplacements.insert(std::move(node));
	}
	for (replacement_id_t replacementId : toRevert) {
		replacements[replacementId]->revert(pauseGuard);
	}
}

EvalConnectionPoint Replacer::getReplacementConnectionPoint(EvalConnectionPoint point) const {
	if (replacedIds.contains(point.gateId)) {
		EvalConnectionPoint newPoint = EvalConnectionPoint(replacedIds.at(point.gateId), point.portId);
		if (newPoint != point) {
			return getReplacementConnectionPoint(newPoint);
		}
		return newPoint;
	}
	if (replacedConnectionPoints.contains(point.gateId) && replacedConnectionPoints.at(point.gateId).contains(point.portId)) {
		EvalConnectionPoint newPoint = replacedConnectionPoints.at(point.gateId).at(point.portId);
		if (newPoint != point) {
			return getReplacementConnectionPoint(newPoint);
		}
		return newPoint;
	}
	return point;
}

std::vector<EvalConnectionPoint> Replacer::getReplacementConnectionPoints(const std::vector<EvalConnectionPoint>& points) const {
	std::vector<EvalConnectionPoint> result;
	result.reserve(points.size());
	for (const EvalConnectionPoint& point : points) {
		result.push_back(getReplacementConnectionPoint(point));
	}
	return result;
}

std::vector<std::optional<EvalConnectionPoint>> Replacer::getReplacementConnectionPoints(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
	std::vector<std::optional<EvalConnectionPoint>> result;
	result.reserve(points.size());
	for (const std::optional<EvalConnectionPoint>& point : points) {
		if (!point.has_value()) {
			result.push_back(std::nullopt);
			continue;
		}
		result.push_back(getReplacementConnectionPoint(point.value()));
	}
	return result;
}

void Replacer::mergeBuses(SimPauseGuard& pauseGuard, int layer, int junctionOverpowerLayer) {
	std::vector<middle_id_t> allMiddleIds = middleIdProvider.getUsedIds();
	for (const middle_id_t id : allMiddleIds) {
		if (replacementIdLayers.contains(id)) {
			if (replacementIdLayers.at(id) >= layer) {
				continue;
			}
		}
		BlockType blockType = busInterfacePassthrough.getBlockType(id);
		if (blockType == BlockType::NONE) {
			continue;
		}
		const BlockData* blockData = blockDataManager.getBlockData(blockType);
		if (!blockData->isBus()) {
			continue;
		}
		unsigned int laneCount = blockData->getLaneCount();
		for (unsigned int lane = 0; lane < laneCount; lane++) {
			std::optional<middle_id_t> junctionId = getJunctionInsideBus(id, lane);
			if (junctionId) {
				continue;
			}
			mergeBusLane(pauseGuard, layer, junctionOverpowerLayer, id, lane);
		}
	}
}

void Replacer::mergeBusLane(SimPauseGuard& pauseGuard, int layer, int junctionOverpowerLayer, middle_id_t id, unsigned int laneId) {
	Replacement& replacement = makeReplacement(layer);
	middle_id_t newJunctionId = replacement.getNewId();
	replacement.addGate(pauseGuard, BlockType::JUNCTION, newJunctionId);
	std::queue<BlockLane> mergeQueue;
	std::unordered_set<BlockLane, BlockLane::Hash> visited;
	mergeQueue.push({ id, laneId });
	visited.insert({ id, laneId });
	while (!mergeQueue.empty()) {
		BlockLane current = mergeQueue.front();
		mergeQueue.pop();
		replacement.trackGate(current.blockId);
		defineJunctionInsideBus(current.blockId, current.laneId, newJunctionId, replacement);
		BlockType blockType = busInterfacePassthrough.getBlockType(current.blockId);
		if (blockType == BlockType::JUNCTION) {
			replacement.markIdAsReplaced(current.blockId, junctionOverpowerLayer);
		}
		// get all inputs/outputs, and add them to the queue if the lanes line up
		std::vector<EvalConnection> inputs = busInterfacePassthrough.getInputs(current.blockId);
		BlockData* blockData = blockDataManager.getBlockData(blockType);
		for (const EvalConnection& input : inputs) {
			const BlockData::ConnectionData* connectionData = blockData->getConnectionData(input.destination.portId);
			unsigned int connectionLaneIndex = current.laneId;
			BlockType sourceBlockType = busInterfacePassthrough.getBlockType(input.source.gateId);

			if (blockType != BlockType::JUNCTION) {
				connectionLaneIndex = connectionData->getIndexOfLaneId(current.laneId);
				if (connectionLaneIndex > connectionData->getMaxLaneIndex()) continue;

				BlockData* sourceBlockData = blockDataManager.getBlockData(sourceBlockType);
				if (!sourceBlockData->isBus() && connectionData->getBitWidth() == 1){
					replacement.trackConnection(input);
					replacement.removeConnection(pauseGuard, input);
					replacement.makeConnection(pauseGuard, { input.source, { newJunctionId, connection_end_id_t(0) } });
					replacement.overrideConnectionPoint(input.destination, { newJunctionId, connection_end_id_t(0) });
					continue;
				}
			}
			replacement.trackConnection(input);
			middle_id_t sourceBlockId = input.source.gateId;
			if (sourceBlockType == BlockType::NONE) {
				continue;
			}
			unsigned int sourceLaneId = connectionLaneIndex;
			if (sourceBlockType != BlockType::JUNCTION) {
				const BlockData* sourceBlockData = blockDataManager.getBlockData(sourceBlockType);
				const BlockData::ConnectionData* sourceConnectionData = sourceBlockData->getConnectionData(input.source.portId);
				if (!sourceConnectionData) {
					continue;
				}
				sourceLaneId = sourceConnectionData->getLaneId(connectionLaneIndex);
			}
			BlockLane sourceLane = { sourceBlockId, sourceLaneId };
			if (!visited.contains(sourceLane)) {
				mergeQueue.push(sourceLane);
				visited.insert(sourceLane);
			}
		}
		std::vector<EvalConnection> outputs = busInterfacePassthrough.getOutputs(current.blockId);
		for (const EvalConnection& output : outputs) {
			const BlockData::ConnectionData* connectionData = blockData->getConnectionData(output.source.portId);
			unsigned int connectionLaneIndex = current.laneId;
			BlockType destBlockType = busInterfacePassthrough.getBlockType(output.destination.gateId);
			if (blockType != BlockType::JUNCTION) {
				connectionLaneIndex = connectionData->getIndexOfLaneId(current.laneId);
				if (connectionLaneIndex > connectionData->getMaxLaneIndex()) continue;
				BlockData* destBlockData = blockDataManager.getBlockData(destBlockType);
				if (!destBlockData->isBus() && connectionData->getBitWidth() == 1) {
					replacement.trackConnection(output);
					replacement.removeConnection(pauseGuard, output);
					replacement.makeConnection(pauseGuard, { { newJunctionId, connection_end_id_t(0) } , output.destination });
					replacement.overrideConnectionPoint(output.source, { newJunctionId, connection_end_id_t(0) });
					continue;
				}
			}
			replacement.trackConnection(output);
			middle_id_t destBlockId = output.destination.gateId;
			if (destBlockType == BlockType::NONE) {
				continue;
			}
			unsigned int destLaneId = connectionLaneIndex;
			if (destBlockType != BlockType::JUNCTION) {
				const BlockData* destBlockData = blockDataManager.getBlockData(destBlockType);
				const BlockData::ConnectionData* destConnectionData = destBlockData->getConnectionData(output.destination.portId);
				if (!destConnectionData) {
					continue;
				}
				destLaneId = destConnectionData->getLaneId(connectionLaneIndex);
			}
			BlockLane destLane = { destBlockId, destLaneId };
			if (!visited.contains(destLane)) {
				mergeQueue.push(destLane);
				visited.insert(destLane);
			}
		}
	}
}



void Replacer::mergeJunctions(SimPauseGuard& pauseGuard, int layer) {
	std::vector<middle_id_t> allMiddleIds = middleIdProvider.getUsedIds();
	for (const middle_id_t id : allMiddleIds) {
		if (replacementIdLayers.contains(id)) {
			if (replacementIdLayers.at(id) >= layer) {
				continue;
			}
		}
		// check if we're a junction
		BlockType blockType = busInterfacePassthrough.getBlockType(id);
		if (!isJunctionType(blockType)) {
			continue;
		}
		JunctionFloodFillResult floodFillResult = junctionFloodFill(id);

		if (floodFillResult.outputsGoingIntoJunctions.size() == 0 && floodFillResult.defaultState == logic_state_t::FLOATING) {
			continue;
		}

		Replacement& replacement = makeReplacement(layer);
		if (floodFillResult.outputsGoingIntoJunctions.size() == 1 && floodFillResult.defaultState == logic_state_t::FLOATING) {
			EvalConnectionPoint output = floodFillResult.outputsGoingIntoJunctions.at(0);
			for (const middle_id_t junctionId : floodFillResult.junctionIds) {
				replacement.removeGate(pauseGuard, junctionId, { {connection_end_id_t(0), output }, {connection_end_id_t(1), output } });
			}
			for (const EvalConnection& input : floodFillResult.inputsPullingFromJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(output, input.destination));
			}
			replacement.trackId(output.gateId);
		} else {
			middle_id_t newJunctionId = replacement.getNewId();
			BlockType typeOfJunction;
			if (floodFillResult.defaultState == logic_state_t::LOW) {
				typeOfJunction = BlockType::JUNCTION_L;
			} else if (floodFillResult.defaultState == logic_state_t::HIGH) {
				typeOfJunction = BlockType::JUNCTION_H;
			} else if (floodFillResult.defaultState == logic_state_t::UNDEFINED) {
				typeOfJunction = BlockType::JUNCTION_X;
			} else {
				typeOfJunction = BlockType::JUNCTION;
			}
			replacement.addGate(pauseGuard, typeOfJunction, newJunctionId);
			for (const middle_id_t junctionId : floodFillResult.junctionIds) {
				replacement.removeGate(pauseGuard, junctionId, newJunctionId);
			}
			for (const EvalConnection& input : floodFillResult.inputsPullingFromJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(EvalConnectionPoint(newJunctionId, connection_end_id_t(0)), input.destination));
			}
			for (const EvalConnectionPoint& output : floodFillResult.outputsGoingIntoJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(output, EvalConnectionPoint(newJunctionId, connection_end_id_t(0))));
			}
			for (const EvalConnection& conn : floodFillResult.connectionsToReroute) {
				EvalConnection newConnection = EvalConnection(EvalConnectionPoint(newJunctionId, connection_end_id_t(0)), conn.destination);
				replacement.removeConnection(pauseGuard, conn);
				replacement.makeConnection(pauseGuard, newConnection);
			}
		}
	}
}

Replacer::JunctionFloodFillResult Replacer::junctionFloodFill(middle_id_t junctionId) {
	JunctionFloodFillResult result;
	std::set<middle_id_t> visited;
	std::set<EvalConnectionPoint> visitedOutputs;
	std::queue<middle_id_t> queue;
	queue.push(junctionId);
	visited.insert(junctionId);
	logic_state_t defaultState = logic_state_t::FLOATING;
	while (!queue.empty()) {
		middle_id_t currentId = queue.front();
		queue.pop();
		result.junctionIds.push_back(currentId);
		std::vector<EvalConnection> outputs = busInterfacePassthrough.getOutputs(currentId);
		std::vector<EvalConnection> inputs = busInterfacePassthrough.getInputs(currentId);
		BlockType currentBlockType = busInterfacePassthrough.getBlockType(currentId);
		if (currentBlockType == BlockType::JUNCTION_L) {
			if (defaultState == logic_state_t::HIGH) {
				defaultState = logic_state_t::UNDEFINED;
			} else if (defaultState == logic_state_t::FLOATING) {
				defaultState = logic_state_t::LOW;
			}
		} else if (currentBlockType == BlockType::JUNCTION_H) {
			if (defaultState == logic_state_t::LOW) {
				defaultState = logic_state_t::UNDEFINED;
			} else if (defaultState == logic_state_t::FLOATING) {
				defaultState = logic_state_t::HIGH;
			}
		} else if (currentBlockType == BlockType::JUNCTION_X) {
			defaultState = logic_state_t::UNDEFINED;
		}
		for (const EvalConnection& output : outputs) {
			if (visited.contains(output.destination.gateId)) {
				continue;
			}
			BlockType outputBlockType = busInterfacePassthrough.getBlockType(output.destination.gateId);
			if (isJunctionType(outputBlockType)) {
				queue.push(output.destination.gateId);
				visited.insert(output.destination.gateId);
				continue;
			}
			result.inputsPullingFromJunctions.push_back(output);
		}
		for (const EvalConnection& input : inputs) {
			if (visited.contains(input.source.gateId)) {
				continue;
			}
			BlockType inputBlockType = busInterfacePassthrough.getBlockType(input.source.gateId);
			if (isJunctionType(inputBlockType)) {
				queue.push(input.source.gateId);
				visited.insert(input.source.gateId);
				continue;
			}
			// not a junction going into a junction
			if (visitedOutputs.contains(input.source)) {
				continue;
			}
			visitedOutputs.insert(input.source);
			result.outputsGoingIntoJunctions.push_back(input.source);
			std::vector<EvalConnection> nodeOutputs = busInterfacePassthrough.getOutputs(input.source.gateId);
			for (const EvalConnection& nodeOutput : nodeOutputs) {
				if (nodeOutput.source.portId != input.source.portId) {
					continue; // only consider outputs from the same port
				}
				BlockType nodeOutputBlockType = busInterfacePassthrough.getBlockType(nodeOutput.destination.gateId);
				if (isJunctionType(nodeOutputBlockType)) {
					if (visited.contains(nodeOutput.destination.gateId)) {
						continue;
					}
					queue.push(nodeOutput.destination.gateId);
					visited.insert(nodeOutput.destination.gateId);
				} else {
					result.connectionsToReroute.push_back(nodeOutput);
				}
			}
		}
	}
	result.defaultState = defaultState;
	return result;
}

std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> Replacer::getPinSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
	// return busInterfacePassthrough.getPinSimulatorIds(getReplacementConnectionPoints(points));
	std::vector<std::optional<EvalConnectionPoint>> replacedPoints = getReplacementConnectionPoints(points);
	std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> result;
	result.reserve(replacedPoints.size());
	for (const auto& point : replacedPoints) {
		if (!point.has_value()) {
			result.emplace_back(static_cast<simulator_id_t>(0));
			continue;
		}
		BlockType blockType = busInterfacePassthrough.getBlockType(point->gateId);
		if (blockType == BlockType::NONE) {
			result.emplace_back(static_cast<simulator_id_t>(0));
			continue;
		}
		if (blockType == BlockType::JUNCTION) {
			if (busInternalJunctions.contains(point->gateId)) {
				const BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(point->gateId);
				std::vector<simulator_id_t> simIds;
				simIds.reserve(busInternalJunctionArray.junctionIds.size());
				for (const middle_id_t junctionId : busInternalJunctionArray.junctionIds) {
					simIds.push_back(busInterfacePassthrough.getPinSimulatorId(getReplacementConnectionPoint({junctionId, connection_end_id_t(0)})));
				}
				if (simIds.size() == 1) {
					result.emplace_back(simIds.at(0));
				} else {
					result.emplace_back(std::move(simIds));
				}
			} else {
				simulator_id_t simId = busInterfacePassthrough.getPinSimulatorId(*point);
				result.emplace_back(simId);
			}
		} else {
			const BlockData* blockData = blockDataManager.getBlockData(blockType);
			if (blockData && blockData->isBus()) {
				const BlockData::ConnectionData* connectionData = blockData->getConnectionData(point->portId);
				std::vector<simulator_id_t> simIds;
				simIds.reserve(connectionData->getBitWidth());
				if (busInternalJunctions.contains(point->gateId)) {
					const BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(point->gateId);
					if (std::holds_alternative<unsigned int>(connectionData->bitConfiguration)) {
						for (unsigned int laneId = 0; laneId < std::get<unsigned int>(connectionData->bitConfiguration); laneId++) {
							simIds.push_back(busInterfacePassthrough.getPinSimulatorId(getReplacementConnectionPoint({busInternalJunctionArray.junctionIds[laneId], connection_end_id_t(0)})));
						}
					} else {
						for (unsigned int laneId : std::get<std::vector<unsigned int>>(connectionData->bitConfiguration)) {
							simIds.push_back(busInterfacePassthrough.getPinSimulatorId(getReplacementConnectionPoint({busInternalJunctionArray.junctionIds[laneId], connection_end_id_t(0)})));
						}
					}
				} else {
					simIds.resize(connectionData->getBitWidth(), simulator_id_t(0));
				}
				if (simIds.size() == 1) {
					result.emplace_back(simIds.at(0));
				} else {
					result.emplace_back(std::move(simIds));
				}
			} else {
				simulator_id_t simId = busInterfacePassthrough.getPinSimulatorId(*point);
				result.emplace_back(simId);
			}
		}
	}
	return result;
}

nlohmann::json Replacer::dumpState() const {
	nlohmann::json stateJson;
	stateJson["busInterfacePassthrough"] = busInterfacePassthrough.dumpState();
	stateJson["replacements"] = nlohmann::json::object();
	for (replacement_id_t id : replacementIdProvider.getUsedIds()) {
		stateJson["replacements"][std::to_string(id.get())] = replacements[id]->dumpState();
	}
	stateJson["replacementIdProvider"] = replacementIdProvider.dumpState();
	stateJson["replacedConnectionPoints"] = nlohmann::json::object();
	for (const auto& [originalId, pointMap] : replacedConnectionPoints) {
		stateJson["replacedConnectionPoints"][std::to_string(originalId.get())] = dumpConnectionPointMap(pointMap);
	}
	stateJson["replacedIds"] = nlohmann::json::object();
	for (const auto& [originalId, replacementId] : replacedIds) {
		stateJson["replacedIds"][std::to_string(originalId.get())] = replacementId.get();
	}
	stateJson["replacementIdLayers"] = nlohmann::json::object();
	for (const auto& [id, layer] : replacementIdLayers) {
		stateJson["replacementIdLayers"][std::to_string(id.get())] = layer;
	}
	stateJson["busInternalJunctions"] = nlohmann::json::object();
	for (const auto& [busId, junctionArray] : busInternalJunctions) {
		stateJson["busInternalJunctions"][std::to_string(busId.get())] = junctionArray.dumpState();
	}
	stateJson["dependentReplacements"] = nlohmann::json::object();
	for (const auto& [id, replacementsSet] : dependentReplacements) {
		stateJson["dependentReplacements"][std::to_string(id.get())] = dumpReplacementIdSet(replacementsSet);
	}
	return stateJson;
}

nlohmann::json Replacer::dumpConnectionPointMap(const std::unordered_map<connection_end_id_t, EvalConnectionPoint>& pointMap) const {
	nlohmann::json pointMapJson;
	for (const auto& [portId, connPoint] : pointMap) {
		pointMapJson[std::to_string(portId.get())] = connPoint.dumpState();
	}
	return pointMapJson;
}

nlohmann::json Replacer::dumpReplacementIdSet(const std::set<replacement_id_t>& idSet) const {
	nlohmann::json idSetJson = nlohmann::json::array();
	for (const replacement_id_t id : idSet) {
		idSetJson.push_back(id.get());
	}
	return idSetJson;
}

nlohmann::json Replacer::BusInternalJunctionArray::dumpState() const {
	nlohmann::json stateJson;
	stateJson["junctionIds"] = nlohmann::json::array();
	for (const middle_id_t junctionId : junctionIds) {
		stateJson["junctionIds"].push_back(junctionId.get());
	}
	stateJson["numDefined"] = numDefined;
	return stateJson;
}
