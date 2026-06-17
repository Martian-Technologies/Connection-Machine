#include "evaluatorInternal.h"

#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/layers/evalLayerState.h"
#include "backend/evaluator/layers/subcircuitEvalLayer.h"
#include "backend/evaluator/layers/junctionMergeEvalLayer.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

extern std::thread::id mainThreadId;

EvaluatorInternal::EvaluatorInternal(const Circuit& circuit, Evaluator& evaluator, const CircuitManager& circuitManager, DataUpdateEventManager::DataUpdateEventReceiver& receiver) :
	evalGateIdProvider(1), circuitManager(circuitManager), circuit(circuit), layerRunner(evalGateIdProvider, evaluator, circuitManager) {
	receiver.linkFunction("circuitBlockDataConnectionPositionRemove", [&](const DataUpdateEventManager::EventData* event) {
		const auto* data = event->cast<std::tuple<BlockType, connection_end_id_t, Position>>();
		if (!data) return;
		BlockType blockType = std::get<0>(data->get());
		if (blockType != circuit.getBlockType()) return;
		connection_end_id_t connectionEndId = std::get<1>(data->get());
		auto portToInternalPointMappingIter = portToInternalPointMapping.find(connectionEndId);
		if (portToInternalPointMappingIter == portToInternalPointMapping.end()) return;
		if (!portToInternalPointMappingIter->second.connectionPoint.has_value()) return;
		EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
		portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
		sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
	});
	receiver.linkFunction("circuitBlockDataConnectionPositionSet", [&](const DataUpdateEventManager::EventData* event) {
		const auto* data = event->cast<std::tuple<BlockType, connection_end_id_t, Position>>();
		if (!data) return;
		BlockType blockType = std::get<0>(data->get());
		if (blockType != circuit.getBlockType()) return;
		connection_end_id_t connectionEndId = std::get<1>(data->get());
		const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(blockType);
		BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(connectionEndId);
		if (portType == BlockData::ConnectionData::PortType::NONE) return;
		assert(portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL); // can not happen and if it does some things are undefined
		unsigned int bitWidth = blockData->getConnectionBitWidth(connectionEndId);
		assert(bitWidth != 0);
		Position internalPortPosition = std::get<2>(data->get());
		const Block* block = circuit.getBlockContainer().getBlock(internalPortPosition);
		auto portToInternalPointMappingIter = portToInternalPointMapping.find(connectionEndId);
		if (!block) {
			if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
				portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(portType, bitWidth));
			} else if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
				EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
				portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
				sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
			}
			return;
		}
		connection_end_id_t internalConnectionEndId;
		switch (block->type()) {
		case BlockType::JUNCTION: {
			unsigned int junctionBitwidth = circuit.getBlockContainer().getBitwidthOfJunction(internalPortPosition);
			if (junctionBitwidth == 0 || junctionBitwidth == bitWidth) {
				internalConnectionEndId = 0;
			} else {
				if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
					portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(portType, bitWidth));
				} else if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
					portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
					sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
				}
				return;
			}
		} break;
		case BlockType::SWITCH:
		case BlockType::BUTTON:
		case BlockType::TICK_BUTTON: {
			if (bitWidth != 1) {
				if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
					portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(portType, bitWidth));
				} else if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
					portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
					sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
				}
				return;
			}
			internalConnectionEndId = (portType == BlockData::ConnectionData::PortType::INPUT);
		} break;
		case BlockType::LIGHT: {
			if (bitWidth != 1) {
				if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
					portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(portType, bitWidth));
				} else if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
					portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
					sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
				}
				return;
			}
			internalConnectionEndId = 0;
		} break;
		default: {
			const BlockData* internalBlockData = circuitManager.getBlockDataManager().getBlockData(block->type());
			assert(internalBlockData);
			std::optional<connection_end_id_t> optInternalConnectionEndId;
			if (portType == BlockData::ConnectionData::PortType::INPUT) {
				optInternalConnectionEndId = internalBlockData->getInputOrBidirectionalConnectionId(internalPortPosition - block->getPosition(), block->getOrientation());
			} else {
				optInternalConnectionEndId = internalBlockData->getOutputOrBidirectionalConnectionId(internalPortPosition - block->getPosition(), block->getOrientation());
			}
			if (!optInternalConnectionEndId.has_value() || bitWidth != internalBlockData->getConnectionBitWidth(optInternalConnectionEndId.value())) {
				if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
					portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(portType, bitWidth));
				} else if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
					portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
					sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
				}
				return;
			}
			internalConnectionEndId = optInternalConnectionEndId.value();
		}
		}
		auto positionRemappingIter = positionRemapping.find(block->getPosition());
		if (positionRemappingIter == positionRemapping.end()) {
			logError("Could not find positionRemapping while doing \"circuitBlockDataConnectionPositionSet\", not sure why this would happen.", "EvaluatorInternal");
			return;
		}
		if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
			portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId), portType, bitWidth));
		} else if (portToInternalPointMappingIter->second.connectionPoint != EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId)) {
			EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value_or(EvalConnectionPoint::null());
			portToInternalPointMappingIter->second.connectionPoint = EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId);
			sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId));
		}
	});
	receiver.linkFunction("blockDataSetConnection", [&](const DataUpdateEventManager::EventData* event) {
		const auto* data = event->cast<std::pair<BlockType, connection_end_id_t>>();
		if (!data) return;
		BlockType blockType = data->get().first;
		if (blockType != circuit.getBlockType()) return;
		connection_end_id_t connectionEndId = data->get().second;
		auto portToInternalPointMappingIter = portToInternalPointMapping.find(connectionEndId);
		if (portToInternalPointMappingIter == portToInternalPointMapping.end()) {
			const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(blockType);
			const BlockData::ConnectionData* connectionData = blockData->getConnectionData(connectionEndId);
			if (connectionData == nullptr) return;
			assert(connectionData->portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL); // can not happen and if it does some things are undefined
			portToInternalPointMapping.try_emplace(connectionEndId, InternalPointData(connectionData->portType, connectionData->getBitWidth()));
			const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit.getCircuitId());
			const Position* portPosition = circuitBlockData->getConnectionIdToPosition(connectionEndId);
			if (portPosition != nullptr) { // I think this is correct
				logError("Port position on block was set which is unexpected.", "EvaluatorInternal");
				return;
			}
			sendPortUpdate(connectionEndId, EvalConnectionPoint::null(), EvalConnectionPoint::null());
		}
	});
	receiver.linkFunction("blockDataPortBitConfigurationSet", [&](const DataUpdateEventManager::EventData* event) {
		const auto* data = event->cast<std::tuple<BlockType, connection_end_id_t, unsigned int>>();
		if (!data) return;
		BlockType blockType = std::get<0>(data->get());
		if (blockType != circuit.getBlockType()) return;
		connection_end_id_t connectionEndId = std::get<1>(data->get());
		unsigned int bitWidth = std::get<2>(data->get());
		auto portToInternalPointMappingIter = portToInternalPointMapping.find(connectionEndId);
		if (portToInternalPointMappingIter != portToInternalPointMapping.end()) {
			if (portToInternalPointMappingIter->second.bitWith != bitWidth) {
				portToInternalPointMappingIter->second.bitWith = bitWidth;
				const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit.getCircuitId());
				assert(circuitBlockData);
				const Position* internalPortPosition = circuitBlockData->getConnectionIdToPosition(connectionEndId);
				if (!internalPortPosition) {
					logError("No internalPortPosition for connectionEndId {}", "EvaluatorInternal::blockDataPortBitConfigurationSet", connectionEndId);
					if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
						EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
						portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
						sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
					}
					return;
				}
				const Block* block = circuit.getBlockContainer().getBlock(*internalPortPosition);
				if (!block) {
					if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
						EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
						portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
						sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
					}
					return;
				}
				auto positionRemappingIter = positionRemapping.find(block->getPosition());
				if (positionRemappingIter == positionRemapping.end()) {
					logError("Could not find positionRemapping while doing \"blockDataPortBitConfigurationSet\", not sure why this would happen.", "EvaluatorInternal::blockDataPortBitConfigurationSet");
					if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
						EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
						portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
						sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
					}
					return;
				}
				const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(blockType);
				BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(connectionEndId);
				assert(portType != BlockData::ConnectionData::PortType::NONE);
				assert(portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL); // can not happen and if it does some things are undefined
				connection_end_id_t internalConnectionEndId;
				switch (block->type()) {
				case BlockType::JUNCTION: {
					unsigned int junctionBitwidth = circuit.getBlockContainer().getBitwidthOfJunction(*internalPortPosition);
					if (junctionBitwidth == 0 || junctionBitwidth == bitWidth) internalConnectionEndId = 0;
					else {
						if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
							EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
							portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
							sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
						}
						return;
					}
				} break;
				case BlockType::SWITCH:
				case BlockType::BUTTON:
				case BlockType::TICK_BUTTON: {
					if (bitWidth != 1) {
						if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
							EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
							portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
							sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
						}
						return;
					}
					internalConnectionEndId = (portType == BlockData::ConnectionData::PortType::INPUT);
				} break;
				case BlockType::LIGHT: {
					if (bitWidth != 1) {
						if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
							EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
							portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
							sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
						}
						return;
					}
					internalConnectionEndId = 0;
				} break;
				default: {
					const BlockData* internalBlockData = circuitManager.getBlockDataManager().getBlockData(block->type());
					assert(internalBlockData);
					std::optional<connection_end_id_t> optInternalConnectionEndId;
					if (portType == BlockData::ConnectionData::PortType::INPUT) {
						optInternalConnectionEndId = internalBlockData->getInputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
					} else {
						optInternalConnectionEndId = internalBlockData->getOutputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
					}
					if (!optInternalConnectionEndId.has_value() || bitWidth != internalBlockData->getConnectionBitWidth(optInternalConnectionEndId.value())) {
						if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
							EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
							portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
							sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint::null());
						}
						return;
					}
					internalConnectionEndId = optInternalConnectionEndId.value();
				}
				}
				if (portToInternalPointMappingIter->second.connectionPoint != EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId)) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value_or(EvalConnectionPoint::null());
					portToInternalPointMappingIter->second.connectionPoint = EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId);
					sendPortUpdate(connectionEndId, connectionPoint, EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId));
				}
			}
		}

		const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(blockType);
		BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(connectionEndId);
		if (portType == BlockData::ConnectionData::PortType::NONE) return;
		assert(portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL); // can not happen and if it does some things are undefined
		portToInternalPointMapping.try_emplace(connectionEndId, portType, bitWidth);
	});
	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit.getCircuitId());
	if (circuitBlockData == nullptr) return;
	const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
	std::vector<std::tuple<connection_end_id_t, EvalConnectionPoint, EvalConnectionPoint>> connectionEndIdsToUpdate;
	for (const std::pair<connection_end_id_t, BlockData::ConnectionData> pair : blockData->getConnectionsSafe()) {
		connection_end_id_t connectionEndId = pair.first;
		unsigned int bitWidth = pair.second.getBitWidth();
		BlockData::ConnectionData::PortType portType = pair.second.portType;
		if (portType == BlockData::ConnectionData::PortType::NONE) return;
		assert(portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL); // can not happen and if it does some things are undefined
		const Position* internalPortPosition = circuitBlockData->getConnectionIdToPosition(connectionEndId);
		if (internalPortPosition == nullptr) continue;
		const Block* block = circuit.getBlockContainer().getBlock(*internalPortPosition);
		if (!block) {
			portToInternalPointMapping.try_emplace(connectionEndId, portType, bitWidth);
			continue;
		}
		auto positionRemappingIter = positionRemapping.find(block->getPosition());
		if (positionRemappingIter == positionRemapping.end()) {
			logError("Could not find positionRemapping while doing \"circuitBlockDataConnectionPositionSet\", not sure why this would happen.", "EvaluatorInternal");
			continue;
		}
		connection_end_id_t internalConnectionEndId;
		switch (block->type()) {
		case BlockType::JUNCTION: {
			unsigned int junctionBitwidth = circuit.getBlockContainer().getBitwidthOfJunction(*internalPortPosition);
			if (junctionBitwidth == 0 || junctionBitwidth == bitWidth) internalConnectionEndId = 0;
			else continue;
		} break;
		case BlockType::SWITCH:
		case BlockType::BUTTON:
		case BlockType::TICK_BUTTON: {
			if (bitWidth != 1) continue;
			internalConnectionEndId = (portType == BlockData::ConnectionData::PortType::INPUT);
		} break;
		case BlockType::LIGHT: {
			if (bitWidth != 1) continue;
			internalConnectionEndId = 0;
		} break;
		default: {
			const BlockData* internalBlockData = circuitManager.getBlockDataManager().getBlockData(block->type());
			assert(internalBlockData);
			std::optional<connection_end_id_t> optInternalConnectionEndId;
			if (portType == BlockData::ConnectionData::PortType::INPUT) {
				optInternalConnectionEndId =
					internalBlockData->getInputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
			} else {
				optInternalConnectionEndId =
					internalBlockData->getOutputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
			}
			if (!optInternalConnectionEndId.has_value() || bitWidth != internalBlockData->getConnectionBitWidth(optInternalConnectionEndId.value())) {
				portToInternalPointMapping.try_emplace(connectionEndId, portType, bitWidth);
				continue;
			}
			internalConnectionEndId = optInternalConnectionEndId.value();
		}
		}
		portToInternalPointMapping.try_emplace(connectionEndId, EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId), portType, bitWidth);
		connectionEndIdsToUpdate.emplace_back(connectionEndId, EvalConnectionPoint::null(), EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId));
	}
}

void EvaluatorInternal::startEdit() {
	layerRunner.resetEdits();
}

void EvaluatorInternal::endEdit() {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	layerRunner.runAll();
	const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuit.getBlockType());
	if (!blockData) return;
	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit.getCircuitId());
	assert(circuitBlockData);
	std::vector<std::tuple<connection_end_id_t, EvalConnectionPoint, EvalConnectionPoint>> connectionEndIdsToUpdate;
	for (const std::pair<connection_end_id_t, BlockData::ConnectionData>& connectionData : blockData->getConnections()) {
		const Position* internalPortPosition = circuitBlockData->getConnectionIdToPosition(connectionData.first);
		if (!internalPortPosition) continue;
		const Block* block = circuit.getBlockContainer().getBlock(*internalPortPosition);
		unsigned int bitWidth = connectionData.second.getBitWidth();
		assert(portToInternalPointMapping.contains(connectionData.first));
		auto portToInternalPointMappingIter = portToInternalPointMapping.find(connectionData.first);
		assert(portToInternalPointMappingIter != portToInternalPointMapping.end());
		if (!block) {
			if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
				EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
				portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
				connectionEndIdsToUpdate.emplace_back(connectionData.first, connectionPoint, EvalConnectionPoint::null());
			}
			continue;
		}
		connection_end_id_t internalConnectionEndId;
		switch (block->type()) {
		case BlockType::SWITCH:
		case BlockType::BUTTON:
		case BlockType::TICK_BUTTON: {
			if (bitWidth != 1) continue;
			internalConnectionEndId = (connectionData.second.portType == BlockData::ConnectionData::PortType::INPUT);
		} break;
		case BlockType::LIGHT: {
			if (bitWidth != 1) continue;
			internalConnectionEndId = 0;
		} break;
		default: {
			const BlockData* internalBlockData = circuitManager.getBlockDataManager().getBlockData(block->type());
			assert(internalBlockData);
			std::optional<connection_end_id_t> optInternalConnectionEndId;
			if (connectionData.second.portType == BlockData::ConnectionData::PortType::INPUT) {
				optInternalConnectionEndId = internalBlockData->getInputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
			} else {
				optInternalConnectionEndId = internalBlockData->getOutputOrBidirectionalConnectionId(*internalPortPosition - block->getPosition(), block->getOrientation());
			}
			if (!optInternalConnectionEndId.has_value() || bitWidth != internalBlockData->getConnectionBitWidth(optInternalConnectionEndId.value())) {
				if (portToInternalPointMappingIter->second.connectionPoint.has_value()) {
					EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value();
					portToInternalPointMappingIter->second.connectionPoint = std::nullopt;
					connectionEndIdsToUpdate.emplace_back(connectionData.first, connectionPoint, EvalConnectionPoint::null());
				}
				continue;
			}
			internalConnectionEndId = optInternalConnectionEndId.value();
		}
		}
		auto positionRemappingIter = positionRemapping.find(block->getPosition());
		if (positionRemappingIter == positionRemapping.end()) {
			logError("Could not find positionRemapping while doing \"circuitBlockDataConnectionPositionSet\", not sure why this would happen.", "EvaluatorInternal");
			continue;
		}
		if (portToInternalPointMappingIter->second.connectionPoint != EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId)) {
			EvalConnectionPoint connectionPoint = portToInternalPointMappingIter->second.connectionPoint.value_or(EvalConnectionPoint::null());
			portToInternalPointMappingIter->second.connectionPoint = EvalConnectionPoint(positionRemappingIter->second.first, internalConnectionEndId);
			connectionEndIdsToUpdate.emplace_back(connectionData.first, connectionPoint, portToInternalPointMappingIter->second.connectionPoint.value());
		}
	}

	for (std::pair<SubcircuitEvalLayer*, unsigned int> evaluator : evaluatorsUsingThisEvaluator) evaluator.first->processICEdits(
		circuit.getCircuitId(), connectionEndIdsToUpdate
	);
}

void EvaluatorInternal::addBlock(Position position, Orientation orientation, BlockType blockType) {
	eval_gate_id simulatorId = evalGateIdProvider.getNewId();
	positionRemapping.try_emplace(position, simulatorId, orientation);
	positionReverseRemapping.try_emplace(simulatorId, position, orientation);
	if (blockType == BlockType::LIGHT) {
		blockType = BlockType::JUNCTION;
	}
	layerRunner.getInputLayer().addGate(simulatorId, getEvalGateType(blockType));
}

void EvaluatorInternal::removeBlock(Position position, Orientation orientation, BlockType blockType) {
	auto remappingIter = positionRemapping.find(position);
	if (remappingIter == positionRemapping.end()) {
		logError("Could not find placed gate at {} when removing gate.", "evaluatorInternal", position);
		return;
	}
	layerRunner.getInputLayer().removeGate(remappingIter->second.first);
	evalGateIdProvider.releaseId(remappingIter->second.first);
	positionReverseRemapping.erase(remappingIter->second.first);
	positionRemapping.erase(remappingIter);
}

void EvaluatorInternal::moveBlock(Position curPosition, Orientation curOrientation, Position newPosition, Orientation newOrientation) {
	auto remappingIter = positionRemapping.find(curPosition);
	if (remappingIter == positionRemapping.end()) {
		logError("Could not find placed gate at {} when moving gate.", "evaluatorInternal", curPosition);
		return;
	}
	eval_gate_id simulatorId = remappingIter->second.first;
	positionRemapping.erase(remappingIter);
	positionRemapping.try_emplace(newPosition, simulatorId, newOrientation);
	positionReverseRemapping.at(simulatorId) = { newPosition, newOrientation };
}

void EvaluatorInternal::createConnection(Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition) {
	auto outputRemappingIter = positionRemapping.find(outputBlockPosition);
	if (outputRemappingIter == positionRemapping.end()) return;
	const EvalGate* outputGate = layerRunner.getInputLayer().getGate(outputRemappingIter->second.first);
	assert(outputGate);
	const BlockData* outputBlockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(outputGate->type));
	std::optional<connection_end_id_t> outputConnectionEndId =
		outputBlockData->getOutputOrBidirectionalConnectionId(outputPosition - outputBlockPosition, outputRemappingIter->second.second);
	assert(outputConnectionEndId);

	auto inputRemappingIter = positionRemapping.find(inputBlockPosition);
	if (inputRemappingIter == positionRemapping.end()) return;
	const EvalGate* inputGate = layerRunner.getInputLayer().getGate(inputRemappingIter->second.first);
	assert(inputGate);
	const BlockData* inputBlockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(inputGate->type));
	std::optional<connection_end_id_t> inputConnectionEndId =
		inputBlockData->getInputOrBidirectionalConnectionId(inputPosition - inputBlockPosition, inputRemappingIter->second.second);
	assert(inputConnectionEndId);

	layerRunner.getInputLayer().addConnection(EvalConnection(outputGate->gateId, *outputConnectionEndId, inputGate->gateId, *inputConnectionEndId), 1);
}

void EvaluatorInternal::removeConnection(Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition) {
	auto outputRemappingIter = positionRemapping.find(outputBlockPosition);
	if (outputRemappingIter == positionRemapping.end()) return;
	const EvalGate* outputGate = layerRunner.getInputLayer().getGate(outputRemappingIter->second.first);
	assert(outputGate);
	const BlockData* outputBlockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(outputGate->type));
	std::optional<connection_end_id_t> outputConnectionEndId =
		outputBlockData->getOutputOrBidirectionalConnectionId(outputPosition - outputBlockPosition, outputRemappingIter->second.second);
	assert(outputConnectionEndId);

	auto inputRemappingIter = positionRemapping.find(inputBlockPosition);
	if (inputRemappingIter == positionRemapping.end()) return;
	const EvalGate* inputGate = layerRunner.getInputLayer().getGate(inputRemappingIter->second.first);
	assert(inputGate);
	const BlockData* inputBlockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(inputGate->type));
	std::optional<connection_end_id_t> inputConnectionEndId =
		inputBlockData->getInputOrBidirectionalConnectionId(inputPosition - inputBlockPosition, inputRemappingIter->second.second);
	assert(inputConnectionEndId);

	layerRunner.getInputLayer().removeConnection(EvalConnection(outputGate->gateId, *outputConnectionEndId, inputGate->gateId, *inputConnectionEndId), 1);
}

std::optional<std::pair<Position, Position>> EvaluatorInternal::mapFromTopConnectionPointToPointAndBlockPosition(EvalConnectionPoint topConnectionPoint) const {
	if (topConnectionPoint.isNull()) return std::nullopt;
	auto iter = positionReverseRemapping.find(topConnectionPoint.gateId);
	if (iter == positionReverseRemapping.end()) return std::nullopt;
	BlockType blockType = getBlockType(layerRunner.getInputLayer().getGate(topConnectionPoint.gateId)->type);
	if (blockType == BlockType::SWITCH || blockType == BlockType::BUTTON || blockType == BlockType::TICK_BUTTON) return std::make_pair(iter->second.first, iter->second.first);
	return std::make_pair(iter->second.first + circuitManager.getBlockDataManager().getConnectionVector(
		blockType,
		iter->second.second,
		topConnectionPoint.connectionEndId
	).value(), iter->second.first);
}
EvalConnectionPoint EvaluatorInternal::mapFromPositionToTopConnectionPoint(Position position) const {
	assert(mainThreadId == std::this_thread::get_id());
	const Block* block = circuit.getBlockContainer().getBlock(position);
	if (block == nullptr) return EvalConnectionPoint::null();
	auto iter = positionRemapping.find(block->getPosition());
	if (iter == positionRemapping.end()) return EvalConnectionPoint::null();
	// hardcode some of the v port connections for primitive blocks
	if (block->type() == BlockType::LIGHT || block->type() == BlockType::JUNCTION_H || block->type() == BlockType::JUNCTION_L || block->type() == BlockType::JUNCTION_X) {
		return EvalConnectionPoint(iter->second.first, 0);
	}
	if (block->type() == BlockType::TRISTATE_BUFFER) return EvalConnectionPoint(iter->second.first, 2);
	// other blocks are 1x1 and have an output so you just need to get what connection end id is the output
	std::optional<connection_end_id_t> connectionEndId = block->getOutputOrBidirectionalConnectionId(position);
	if (!connectionEndId) return EvalConnectionPoint::null();
	return EvalConnectionPoint(iter->second.first, connectionEndId.value());
}
std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> EvaluatorInternal::mapFromAddressToBottomConnectionPoints(const Address& address) const {
	if (address.size() == 1) {
		EvalConnectionPoint topConnectionPoint = mapFromPositionToTopConnectionPoint(address.getPosition(0));
		if (topConnectionPoint.isNull()) return EvalConnectionPoint::null();
		return layerRunner.getMappedEvalConnectionPoint(topConnectionPoint);
	}
	if (address.size() == 0) return EvalConnectionPoint::null();
	auto iter = positionRemapping.find(address.getPosition(0));
	if (iter == positionRemapping.end()) return EvalConnectionPoint::null();
	return layerRunner.getMappedAddress(iter->second.first, address.popTopPosition());
}
EvalConnectionPoint EvaluatorInternal::mapFromAddressToBottomConnectionPointForOtherEvals(const Address& address) const {
	if (address.size() == 1) {
		EvalConnectionPoint topConnectionPoint = mapFromPositionToTopConnectionPoint(address.getPosition(0));
		if (topConnectionPoint.isNull()) return EvalConnectionPoint::null();
		return layerRunner.getMappedEvalConnectionPointForOtherEvals(topConnectionPoint);
	}
	if (address.size() == 0) return EvalConnectionPoint::null();
	auto iter = positionRemapping.find(address.getPosition(0));
	if (iter == positionRemapping.end()) return EvalConnectionPoint::null();
	return layerRunner.getMappedAddressForOtherEvals(iter->second.first, address.popTopPosition());
}
EvalConnectionPoint EvaluatorInternal::mapFromTopConnectionPointToBottomConnectionPointForOtherEvals(EvalConnectionPoint topConnectionPoint) const {
	return layerRunner.getMappedEvalConnectionPointForOtherEvals(topConnectionPoint);
}
VecVecEvalConnectionPoint EvaluatorInternal::mapFromBottomConnectionPointsToTopConnectionPointsForOtherEvals(VecEvalConnectionPoint bottomConnectionPoints, Address address) const {
	if (address.size() == 0) {
		VecVecEvalConnectionPoint outputConnectionPoints;
		for (EvalConnectionPoint connectionPoint : bottomConnectionPoints) {
			outputConnectionPoints.push_back({ });
			layerRunner.getReversedMappedEvalConnectionPointForOtherEvals(connectionPoint, outputConnectionPoints.back());
		}
		return outputConnectionPoints;
	}
	auto iter = positionRemapping.find(address.getPosition(0));
	if (iter == positionRemapping.end()) return { };
	return layerRunner.getReversedMappedConnectionPointsWithAddressForOtherEvals(bottomConnectionPoints, iter->second.first, address.popTopPosition());
}
VecVecEvalConnectionPoint EvaluatorInternal::mapFromBottomConnectionPointGroupsToTopConnectionPointsForOtherEvals(VecVecEvalConnectionPoint bottomConnectionPoints, Address address) const {
	if (address.size() == 0) {
		VecVecEvalConnectionPoint outputConnectionPoints;
		for (VecEvalConnectionPoint connectionPoints : bottomConnectionPoints) {
			outputConnectionPoints.push_back({ });
			for (EvalConnectionPoint connectionPoint : connectionPoints) {
				layerRunner.getReversedMappedEvalConnectionPointForOtherEvals(connectionPoint, outputConnectionPoints.back());
			}
		}
		return outputConnectionPoints;
	}
	auto iter = positionRemapping.find(address.getPosition(0));
	if (iter == positionRemapping.end()) return { };
	return layerRunner.getReversedMappedConnectionPointGroupsWithAddressForOtherEvals(bottomConnectionPoints, iter->second.first, address.popTopPosition());
}

EvalConnectionPoint EvaluatorInternal::getConnectionPointFromConnectionEndId(connection_end_id_t connectionEndId) const {
	auto iter = portToInternalPointMapping.find(connectionEndId);
	if (iter == portToInternalPointMapping.end()) return EvalConnectionPoint::null();
	return iter->second.connectionPoint.value_or(EvalConnectionPoint::null());
}

bool EvaluatorInternal::isConnectionIdSinglePin(connection_end_id_t connectionEndId) const {
	EvalConnectionPoint topConnectionPoint = getConnectionPointFromConnectionEndId(connectionEndId);
	if (topConnectionPoint.isNull()) return false; // no point means that they are not connection
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPointsVariant = layerRunner.getMappedEvalConnectionPoint(topConnectionPoint);
	if (std::holds_alternative<EvalConnectionPoint>(connectionPointsVariant)) {
		EvalConnectionPoint connectionPoint = std::get<EvalConnectionPoint>(connectionPointsVariant);
		const EvalGate* gate = layerRunner.getOutputLayer().getGate(connectionPoint.gateId);
		if (gate == nullptr) {
			logError("No gate {} found when checking output layer", "EvalConnectionPoint::isConnectionIdSinglePin", connectionPoint.gateId);
			return false;
		}
		return (gate->type != BlockType::JUNCTION && gate->type != BlockType::JUNCTION_L &&
			gate->type != BlockType::JUNCTION_H && gate->type != BlockType::JUNCTION_X &&
			EvalConnectionEndInfo::isConnectionEndIdSinglePin(gate->type, connectionPoint.connectionEndId)
		);
	}
	const std::vector<EvalConnectionPoint>& connectionPoints = std::get<std::vector<EvalConnectionPoint>>(connectionPointsVariant);
	for (EvalConnectionPoint connectionPoint : connectionPoints) {
		const EvalGate* gate = layerRunner.getOutputLayer().getGate(connectionPoint.gateId);
		if (gate == nullptr) {
			logError("No gate {} found when checking output layer", "EvalConnectionPoint::isConnectionIdSinglePin", connectionPoint.gateId);
			continue;
		}
		if (gate->type != BlockType::JUNCTION && gate->type != BlockType::JUNCTION_L &&
			gate->type != BlockType::JUNCTION_H && gate->type != BlockType::JUNCTION_X &&
			EvalConnectionEndInfo::isConnectionEndIdSinglePin(gate->type, connectionPoint.connectionEndId)
		) return true;
	}
	return false;
}

std::vector<std::pair<Position, circuit_id_t>> EvaluatorInternal::getSubcircuits() const {
	std::vector<std::pair<Position, circuit_id_t>> subcircuits;
	for (const std::pair<eval_gate_id, circuit_id_t>& pair : layerRunner.getSubcircuits()) {
		subcircuits.emplace_back(positionReverseRemapping.at(pair.first).first, pair.second);
	}
	return subcircuits;
}

void EvaluatorInternal::sendPortUpdate(connection_end_id_t connectionEndId, EvalConnectionPoint preConnectionPoint, EvalConnectionPoint postConnectionPoint) {
	startEdit();
	for (const std::pair<SubcircuitEvalLayer*, unsigned int>& subcircuitEvalLayer : evaluatorsUsingThisEvaluator) {
		subcircuitEvalLayer.first->processICEdits(circuit.getCircuitId(), {{ connectionEndId, preConnectionPoint, postConnectionPoint }});
	}
}
