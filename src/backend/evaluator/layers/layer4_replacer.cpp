#include "layer4_replacer.h"

#include "layer4_replacement.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

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
	std::unordered_set<replacement_id_t>& replacementsSet = node.mapped();
	std::unordered_set<replacement_id_t> remaining;
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
		std::pair<const std::vector<EvalConnection>&, std::vector<EvalConnection>> inputs = busInterfacePassthrough.getInputs(current.blockId);
		BlockData* blockData = blockDataManager.getBlockData(blockType);
		for (int i = 0; i < inputs.first.size() + inputs.second.size(); i++) {
			const EvalConnection& input = (i < inputs.first.size()) ? inputs.first.at(i) : inputs.second[i - inputs.first.size()];
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
				if (sourceConnectionData->getBitWidth() <= connectionLaneIndex) {
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
		std::pair<const std::vector<EvalConnection>&, std::vector<EvalConnection>> outputs = busInterfacePassthrough.getOutputs(current.blockId);
		for (int i = 0; i < outputs.first.size() + outputs.second.size(); i++) {
			const EvalConnection& output = (i < outputs.first.size()) ? outputs.first.at(i) : outputs.second[i - outputs.first.size()];
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
				if (destConnectionData->getBitWidth() <= connectionLaneIndex) {
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
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::vector<middle_id_t> junctionIdsToErase;
	for (const middle_id_t id : existingJunctionIds) {
#ifdef TRACY_PROFILER
		ZoneScopedN("mergeJunctions: per junction");
#endif
		if (replacementIdLayers.contains(id)) {
			if (replacementIdLayers.at(id) >= layer) {
				continue;
			}
		}
		if (!isJunctionType(busInterfacePassthrough.getBlockType(id))) {
			continue;
		}
		JunctionFloodFillResult floodFillResult = junctionFloodFill(id);

		Replacement& replacement = makeReplacement(layer);
		if (floodFillResult.outputsGoingIntoJunctions.size() == 0 && floodFillResult.defaultState == logic_state_t::FLOATING) {
#ifdef TRACY_PROFILER
			ZoneScopedN("mergeJunctions: track gate only");
#endif
			for (const middle_id_t junctionId : floodFillResult.junctionIds) {
				replacement.trackGate(junctionId);
			}
		} else if (floodFillResult.outputsGoingIntoJunctions.size() == 1 && floodFillResult.defaultState == logic_state_t::FLOATING) {
#ifdef TRACY_PROFILER
			ZoneScopedN("mergeJunctions: complete removal (single input)");
#endif
			EvalConnectionPoint output = floodFillResult.outputsGoingIntoJunctions.at(0);
			for (const middle_id_t junctionId : floodFillResult.junctionIds) {
				replacement.removeGate(pauseGuard, junctionId, { {connection_end_id_t(0), output }, {connection_end_id_t(1), output } });
			}
			for (const EvalConnection& input : floodFillResult.inputsPullingFromJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(output, input.destination));
			}
			replacement.trackId(output.gateId);
		} else {
#ifdef TRACY_PROFILER
			ZoneScopedN("mergeJunctions: standard merge");
#endif
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
		for (const middle_id_t junctionId : floodFillResult.junctionIds) {
			junctionIdsToErase.push_back(junctionId);
		}
		replacement.addRevertAction([this, junctionIds = std::move(floodFillResult.junctionIds)]() {
			for (const middle_id_t junctionId : junctionIds) {
				existingJunctionIds.insert(junctionId);
			}
		});
	}
	for (const middle_id_t junctionId : junctionIdsToErase) {
		existingJunctionIds.erase(junctionId);
	}
}

Replacer::JunctionFloodFillResult Replacer::junctionFloodFill(middle_id_t junctionId) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	JunctionFloodFillResult result;
	std::unordered_set<middle_id_t> visited;
	std::unordered_set<EvalConnectionPoint> visitedOutputs;
	std::queue<middle_id_t> queue;
	queue.push(junctionId);
	visited.insert(junctionId);
	logic_state_t defaultState = logic_state_t::FLOATING;
	while (!queue.empty()) {
#ifdef TRACY_PROFILER
		ZoneScopedN("junctionFloodFill: per junction");
#endif
		middle_id_t currentId = queue.front();
		queue.pop();
		result.junctionIds.push_back(currentId);
		std::pair<const std::vector<EvalConnection>&, std::vector<EvalConnection>> outputs = busInterfacePassthrough.getOutputs(currentId);
		std::pair<const std::vector<EvalConnection>&, std::vector<EvalConnection>> inputs = busInterfacePassthrough.getInputs(currentId);
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
		for (int i = 0; i < outputs.first.size() + outputs.second.size(); i++) {
#ifdef TRACY_PROFILER
			ZoneScopedN("junctionFloodFill: per junction output");
#endif
			const EvalConnection& output = (i < outputs.first.size()) ? outputs.first.at(i) : outputs.second[i - outputs.first.size()];
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
		for (int i = 0; i < inputs.first.size() + inputs.second.size(); i++) {
#ifdef TRACY_PROFILER
			ZoneScopedN("junctionFloodFill: per junction input");
#endif
			const EvalConnection& input = (i < inputs.first.size()) ? inputs.first.at(i) : inputs.second[i - inputs.first.size()];
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
			std::pair<const std::vector<EvalConnection>&, std::vector<EvalConnection>> nodeOutputs = busInterfacePassthrough.getOutputs(input.source.gateId);
			for (int j = 0; j < nodeOutputs.first.size() + nodeOutputs.second.size(); j++) {
#ifdef TRACY_PROFILER
				ZoneScopedN("junctionFloodFill: per junction input per output");
#endif
				const EvalConnection& nodeOutput = (j < nodeOutputs.first.size()) ? nodeOutputs.first.at(j) : nodeOutputs.second[j - nodeOutputs.first.size()];
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

std::variant<simulator_id_t, std::vector<simulator_id_t>> Replacer::getPinSimulatorId(EvalConnectionPoint point) const {
	EvalConnectionPoint replacedPoint = getReplacementConnectionPoint(point);
	BlockType blockType = busInterfacePassthrough.getBlockType(replacedPoint.gateId);
	if (blockType == BlockType::NONE) {
		return simulator_id_t(3);
	}
	if (blockType == BlockType::JUNCTION) {
		if (busInternalJunctions.contains(replacedPoint.gateId)) {
			const BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(replacedPoint.gateId);
			std::vector<simulator_id_t> simIds;
			simIds.reserve(busInternalJunctionArray.junctionIds.size());
			for (const middle_id_t junctionId : busInternalJunctionArray.junctionIds) {
				simIds.push_back(busInterfacePassthrough.getPinSimulatorId(getReplacementConnectionPoint({junctionId, connection_end_id_t(0)})));
			}
			if (simIds.size() == 1) {
				return simIds.at(0);
			} else {
				return std::move(simIds);
			}
		} else {
			return busInterfacePassthrough.getPinSimulatorId(replacedPoint);
		}
	} else {
		const BlockData* blockData = blockDataManager.getBlockData(blockType);
		if (blockData && blockData->isBus()) {
			const BlockData::ConnectionData* connectionData = blockData->getConnectionData(replacedPoint.portId);
			std::vector<simulator_id_t> simIds;
			simIds.reserve(connectionData->getBitWidth());
			if (busInternalJunctions.contains(replacedPoint.gateId)) {
				const BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(replacedPoint.gateId);
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
				return simIds.at(0);
			} else {
				return std::move(simIds);
			}
		} else {
			return busInterfacePassthrough.getPinSimulatorId(replacedPoint);
		}
	}
}

nlohmann::json Replacer::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
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

nlohmann::json Replacer::dumpConnectionPointMap(const std::unordered_map<connection_end_id_t, EvalConnectionPoint>& pointMap) const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json pointMapJson;
	for (const auto& [portId, connPoint] : pointMap) {
		pointMapJson[std::to_string(portId.get())] = connPoint.dumpState();
	}
	return pointMapJson;
}

nlohmann::json Replacer::dumpReplacementIdSet(const std::unordered_set<replacement_id_t>& idSet) const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json idSetJson = nlohmann::json::array();
	for (const replacement_id_t id : idSet) {
		idSetJson.push_back(id.get());
	}
	return idSetJson;
}

nlohmann::json Replacer::BusInternalJunctionArray::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	stateJson["junctionIds"] = nlohmann::json::array();
	for (const middle_id_t junctionId : junctionIds) {
		stateJson["junctionIds"].push_back(junctionId.get());
	}
	stateJson["numDefined"] = numDefined;
	return stateJson;
}
