#include "circuitRenderManager.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/circuit/circuit.h"
#include "backend/evaluator/layers/junctionMergeEvalLayer.h"
#include "backend/evaluator/evaluator.h"
#include "backend/evaluator/evaluatorInternal.h"
#include "gpu/mainRenderer.h"
#include "environment/environment.h"

CircuitRenderManager::CircuitRenderManager(Environment& environment, circuit_id_t circuitId, ViewportId viewportId) :
	environment(environment), backend(environment.getBackend()), circuitId(circuitId), viewportId(viewportId) {
	Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId);
	if (!circuit) {
		logError("Failed to find circuit with Id {}", "CircuitRenderManager", circuitId);
		return;
	}
	circuit->connectListener(this, [this](DifferenceSharedPtr diff, circuit_id_t circuitId) {if (circuitId == this->circuitId) addDifference(diff); });
	addDifference(circuit->getBlockContainer().getCreationDifferenceShared());
}

CircuitRenderManager::~CircuitRenderManager() {
	Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId);
	if (!circuit) return; // not an error because the circuit may have already been destroyed
	MainRenderer::get().resetCircuit(viewportId);
	MainRenderer::get().setViewportSimulator(viewportId, nullptr, Address());
	circuit->disconnectListener(this);
}

void CircuitRenderManager::addDifference(DifferenceSharedPtr diff) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (diff->clearsAll()) {
		renderedBlocks.clear();
		MainRenderer::get().resetCircuit(viewportId);
		return;
	}

	MainRenderer::get().startMakingEdits(viewportId);

	Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId);
	if (!circuit) { // not an error because the circuit may have already been destroyed
		MainRenderer::get().resetViewport(viewportId);
		circuitId = 0; // dont let this attach to something else
		return;
	}
	const BlockDataManager& blockDataManager = circuit->getBlockContainer().getBlockDataManager();
	for (const auto& modification : diff->getModifications()) {
		const auto& [modificationType, modificationData] = modification;
		switch (modificationType) {
		case Difference::ModificationType::PLACE_BLOCK:
		{
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);
			MainRenderer::get().addBlock(viewportId, environment.getBlockRenderDataFeeder().getBlockRenderDataId(blockType), position, orientation);
			renderedBlocks.emplace(position, RenderedBlock(blockType, orientation));
			break;
		}
		case Difference::ModificationType::REMOVED_BLOCK:
		{
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);

			MainRenderer::get().removeBlock(viewportId, position);
			auto iter = renderedBlocks.find(position);
			if (iter == renderedBlocks.end()) {
				logError("Could not find block at {} to remove.", "CircuitRenderManager", position);
				continue;
			}
			if (iter->second.inputConnections.size() != 0 || iter->second.outputConnections.size() != 0) {
				logError("Trying to remove block at {} that still has {} connections.", "CircuitRenderManager", position,
					iter->second.inputConnections.size() + iter->second.outputConnections.size());
				continue;
			}
			renderedBlocks.erase(iter);
			break;
		}
		case Difference::ModificationType::CREATED_CONNECTION:
		{
			auto [outputBlockPosition, outputPortPosition, inputBlockPosition, inputPortPosition] = std::get<Difference::connection_modification_t>(modificationData);
			orderConnection(outputBlockPosition, outputPortPosition, inputBlockPosition, inputPortPosition);

			auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
			if (outputBlockIter == renderedBlocks.end()) {
				logError("Could not find block at {} to add output connection to.", "CircuitRenderManager", outputBlockPosition);
				continue;
			}
			auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
			if (inputBlockIter == renderedBlocks.end()) {
				logError("Could not find block at {} to add input connection to.", "CircuitRenderManager", inputBlockPosition);
				continue;
			}
			outputBlockIter->second.outputConnections[outputPortPosition].connections.emplace(inputPortPosition, inputBlockPosition);
			const BlockData* outputBlockData = blockDataManager.getBlockData(outputBlockIter->second.type);
			assert(outputBlockData);
			std::optional<connection_end_id_t> outputConnectionEndId = outputBlockData->getOutputOrBidirectionalConnectionId(
				outputPortPosition - outputBlockPosition, outputBlockIter->second.orientation
			);
			if (!outputConnectionEndId) continue;
			const BlockData* inputBlockData = blockDataManager.getBlockData(inputBlockIter->second.type);
			assert(inputBlockData);
			std::optional<connection_end_id_t> inputConnectionEndId = inputBlockData->getInputOrBidirectionalConnectionId(
				inputPortPosition - inputBlockPosition, inputBlockIter->second.orientation
			);
			if (!inputConnectionEndId) continue;
			if (outputBlockPosition == inputBlockPosition) {
				InputRenderedBlockPort& inputRenderedBlockPort = outputBlockIter->second.inputConnections[inputPortPosition];
				inputRenderedBlockPort.connections.emplace(outputPortPosition, outputBlockPosition);
				if ((!inputRenderedBlockPort.ordering.has_value()) &&
					inputBlockIter->second.type != BlockType::JUNCTION && inputBlockIter->second.type != BlockType::JUNCTION_L &&
					inputBlockIter->second.type != BlockType::JUNCTION_H && inputBlockIter->second.type != BlockType::JUNCTION_X &&
					inputBlockIter->second.type != BlockType::LIGHT && !inputBlockData->isBus()
				) {
					circuit_id_t inputBlockCircuitId = environment.getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitId(inputBlockIter->second.type);
					if (inputBlockCircuitId == 0) {
						if (!EvalConnectionEndInfo::isConnectionEndIdSinglePin(getEvalGateType(inputBlockIter->second.type), inputConnectionEndId.value())) {
							inputRenderedBlockPort.ordering.emplace();
						}
					} else {
						const Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(inputBlockCircuitId);
						assert(circuit);
						if (!circuit->getEvaluator().getEvaluatorInternal().isConnectionIdSinglePin(inputConnectionEndId.value())) {
							inputRenderedBlockPort.ordering.emplace();
						}
					}
				}
				if (inputRenderedBlockPort.ordering.has_value()) {
					for (Position otherOutputPortPosition : inputRenderedBlockPort.ordering.value()) {
						MainRenderer::get().removeWire(viewportId, std::make_pair(otherOutputPortPosition, inputPortPosition));
					}
					inputRenderedBlockPort.ordering->push_back(outputPortPosition);
					createConnectionsForInputPort(inputBlockPosition, inputPortPosition);
				} else {
					MainRenderer::get().addWire(viewportId, { outputPortPosition, inputPortPosition }, {
						outputBlockData->getConnectionPortOffset(
							outputConnectionEndId.value(), outputBlockIter->second.orientation
						).value_or(FVector(0.5)),
						outputBlockData->getConnectionPortOffset(
							inputConnectionEndId.value(), outputBlockIter->second.orientation
						).value_or(FVector(0.5)),
					});
				}
			} else {
				InputRenderedBlockPort& inputRenderedBlockPort = inputBlockIter->second.inputConnections[inputPortPosition];
				inputRenderedBlockPort.connections.emplace(outputPortPosition, outputBlockPosition);
				if (!inputRenderedBlockPort.ordering.has_value() &&
					inputBlockIter->second.type != BlockType::JUNCTION && inputBlockIter->second.type != BlockType::JUNCTION_L &&
					inputBlockIter->second.type != BlockType::JUNCTION_H && inputBlockIter->second.type != BlockType::JUNCTION_X &&
					inputBlockIter->second.type != BlockType::LIGHT && !inputBlockData->isBus()
				) {
					circuit_id_t inputBlockCircuitId = environment.getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitId(inputBlockIter->second.type);
					if (inputBlockCircuitId == 0) {
						if (!EvalConnectionEndInfo::isConnectionEndIdSinglePin(getEvalGateType(inputBlockIter->second.type), inputConnectionEndId.value())) {
							inputRenderedBlockPort.ordering.emplace();
						}
					} else {
						const Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(inputBlockCircuitId);
						assert(circuit);
						if (!circuit->getEvaluator().getEvaluatorInternal().isConnectionIdSinglePin(inputConnectionEndId.value())) {
							inputRenderedBlockPort.ordering.emplace();
						}
					}
				}
				if (inputRenderedBlockPort.ordering.has_value()) {
					for (Position otherOutputPortPosition : inputRenderedBlockPort.ordering.value()) {
						MainRenderer::get().removeWire(viewportId, std::make_pair(otherOutputPortPosition, inputPortPosition));
					}
					inputRenderedBlockPort.ordering->push_back(outputPortPosition);
					createConnectionsForInputPort(inputBlockPosition, inputPortPosition);
				} else {
					MainRenderer::get().addWire(viewportId, { outputPortPosition, inputPortPosition }, {
						outputBlockData->getConnectionPortOffset(
							outputConnectionEndId.value(), outputBlockIter->second.orientation
						).value_or(FVector(0.5)),
						inputBlockData->getConnectionPortOffset(
							inputConnectionEndId.value(), inputBlockIter->second.orientation
						).value_or(FVector(0.5)),
					});
				}
			}
			break;
		}
		case Difference::ModificationType::REMOVED_CONNECTION:
		{
			auto [outputBlockPosition, outputPortPosition, inputBlockPosition, inputPortPosition] = std::get<Difference::connection_modification_t>(modificationData);
			orderConnection(outputBlockPosition, outputPortPosition, inputBlockPosition, inputPortPosition);

			MainRenderer::get().removeWire(viewportId, { outputPortPosition, inputPortPosition });

			auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
			if (outputBlockIter == renderedBlocks.end()) {
				logError("Could not find block at {} to remove output connection from.", "CircuitRenderManager", outputBlockPosition);
				continue;;
			}
			auto outputRenderedBlockPortIter = outputBlockIter->second.outputConnections.find(outputPortPosition);
			if (outputRenderedBlockPortIter == outputBlockIter->second.outputConnections.end()) {
				logError("Could not find port at {} to remove output connection from.", "CircuitRenderManager", outputPortPosition);
				continue;;
			}
			if (outputRenderedBlockPortIter->second.connections.size() == 1) {
				if (outputRenderedBlockPortIter->second.connections.begin()->first != inputPortPosition) {
					logError("Failed to erase output connection {} from port {}.", "CircuitRenderManager", inputPortPosition, outputPortPosition);
					continue;
				}
				outputBlockIter->second.outputConnections.erase(outputRenderedBlockPortIter);
			} else {
				auto returnVal = outputRenderedBlockPortIter->second.connections.erase(inputPortPosition);
				if (!returnVal) {
					logError("Failed to erase output connection {} from port {}.", "CircuitRenderManager", inputPortPosition, outputPortPosition);
					continue;
				}
			}

			if (outputBlockPosition == inputBlockPosition) {
				auto inputRenderedBlockPortIter = outputBlockIter->second.inputConnections.find(inputPortPosition);
				if (inputRenderedBlockPortIter == outputBlockIter->second.inputConnections.end()) {
					logError("Could not find port at {} to remove input connection from.", "CircuitRenderManager", inputPortPosition);
					continue;;
				}
				if (inputRenderedBlockPortIter->second.connections.size() == 1) {
					if (inputRenderedBlockPortIter->second.connections.begin()->second != outputPortPosition) {
						logError("Failed to erase input connection {} from port {}.", "CircuitRenderManager", outputPortPosition, inputPortPosition);
						continue;;
					}
					outputBlockIter->second.inputConnections.erase(inputRenderedBlockPortIter);
				} else {
					auto returnVal = inputRenderedBlockPortIter->second.connections.erase(outputPortPosition);
					if (!returnVal) {
						logError("Failed to erase input connection {} from port {}.", "CircuitRenderManager", outputPortPosition, inputPortPosition);
						continue;;
					}
					if (inputRenderedBlockPortIter->second.ordering.has_value()) {
						auto iter = std::find(inputRenderedBlockPortIter->second.ordering->begin(), inputRenderedBlockPortIter->second.ordering->end(), outputPortPosition);
						inputRenderedBlockPortIter->second.ordering->erase(iter);
						for (Position otherOutputPortPosition : inputRenderedBlockPortIter->second.ordering.value()) {
							MainRenderer::get().removeWire(viewportId, std::make_pair(otherOutputPortPosition, inputPortPosition));
							createConnectionsForInputPort(inputBlockPosition, inputPortPosition);
						}
					}
				}
			} else {
				auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
				if (inputBlockIter == renderedBlocks.end()) {
					logError("Could not find block at {} to remove input connection from.", "CircuitRenderManager", inputBlockPosition);
					continue;
				}
				auto inputRenderedBlockPortIter = inputBlockIter->second.inputConnections.find(inputPortPosition);
				if (inputRenderedBlockPortIter == inputBlockIter->second.inputConnections.end()) {
					logError("Could not find port at {} to remove input connection from.", "CircuitRenderManager", inputPortPosition);
					continue;;
				}
				if (inputRenderedBlockPortIter->second.connections.size() == 1) {
					if (inputRenderedBlockPortIter->second.connections.begin()->second != outputPortPosition) {
						logError("Failed to erase input connection {} from port {}.", "CircuitRenderManager", outputPortPosition, inputPortPosition);
						continue;;
					}
					inputBlockIter->second.inputConnections.erase(inputRenderedBlockPortIter);
				} else {
					auto returnVal = inputRenderedBlockPortIter->second.connections.erase(outputPortPosition);
					if (!returnVal) {
						logError("Failed to erase input connection {} from port {}.", "CircuitRenderManager", outputPortPosition, inputPortPosition);
						continue;;
					}
					if (inputRenderedBlockPortIter->second.ordering.has_value()) {
						auto iter = std::find(inputRenderedBlockPortIter->second.ordering->begin(), inputRenderedBlockPortIter->second.ordering->end(), outputPortPosition);
						inputRenderedBlockPortIter->second.ordering->erase(iter);
						for (Position otherOutputPortPosition : inputRenderedBlockPortIter->second.ordering.value()) {
							MainRenderer::get().removeWire(viewportId, std::make_pair(otherOutputPortPosition, inputPortPosition));
							createConnectionsForInputPort(inputBlockPosition, inputPortPosition);
						}
					}
				}
			}
			break;
		}
		case Difference::ModificationType::MOVE_BLOCK:
		{
			const auto& [curBlockPosition, curOrientation, newBlockPosition, newOrientation, moveType] = std::get<Difference::move_modification_t>(modificationData);
			if (curBlockPosition == newBlockPosition && curOrientation == newOrientation) continue;

			auto blockIter = renderedBlocks.find(curBlockPosition);
			if (blockIter == renderedBlocks.end()) {
				logError("Could not find block at {} to move.", "CircuitRenderManager", curBlockPosition);
				continue;
			}

			if (curBlockPosition != newBlockPosition) {
				auto newIter = renderedBlocks.emplace(newBlockPosition, std::move(blockIter->second)).first;
				renderedBlocks.erase(blockIter);
				blockIter = newIter;
			}

			blockIter->second.orientation = newOrientation;

			// MOVE BLOCK
			MainRenderer::get().moveBlock(viewportId, curBlockPosition, newBlockPosition, newOrientation);

			Size blockSize = blockDataManager.getBlockSize(blockIter->second.type, curOrientation);
			Orientation transformAmount = newOrientation.relativeTo(curOrientation);

			// MOVE CONNECTIONS
			std::unordered_map<Position, OutputRenderedBlockPort> oldOutputConnectionsCopy = std::move(blockIter->second.outputConnections);
			std::unordered_map<Position, InputRenderedBlockPort> oldInputConnectionsCopy = std::move(blockIter->second.inputConnections);
			blockIter->second.outputConnections.clear();
			blockIter->second.inputConnections.clear();

			std::vector<std::pair<Position, Position>> inputPorts;

			for (const auto& [outputPortPosition, outputRenderedBlockPort] : oldOutputConnectionsCopy) {
				Position newOutputPortPosition = newBlockPosition + transformAmount.transformVectorWithArea(outputPortPosition - curBlockPosition, blockSize);
				for (const auto& [inputPortPosition, inputBlockPosition] : outputRenderedBlockPort.connections) {
					if (inputBlockPosition == curBlockPosition) continue;
					if ((moveType == MoveType::SINGLE || moveType == MoveType::MULTI_BEGIN) && inputBlockPosition.x != 10000000) {
						MainRenderer::get().removeWire(viewportId, {outputPortPosition, inputPortPosition});
					}
					auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
					if (inputBlockIter == renderedBlocks.end()) {
						logError("Could not find block at {} to move connection.", "CircuitRenderManager", inputBlockPosition);
						continue;
					}
					auto inputRenderedBlockPortIter = inputBlockIter->second.inputConnections.find(inputPortPosition);
					if (inputRenderedBlockPortIter == inputBlockIter->second.inputConnections.end()) {
						logError("Could not find port at {} to move connection.", "CircuitRenderManager", inputPortPosition);
						continue;
					}
					inputPorts.emplace_back(inputBlockPosition, inputPortPosition);
					blockIter->second.outputConnections[newOutputPortPosition].connections.emplace(inputPortPosition, inputBlockPosition);
					inputRenderedBlockPortIter->second.connections.erase(outputPortPosition);
					inputRenderedBlockPortIter->second.connections.emplace(newOutputPortPosition, newBlockPosition);
					if (inputRenderedBlockPortIter->second.ordering.has_value()) {
						auto iter = std::find(inputRenderedBlockPortIter->second.ordering->begin(), inputRenderedBlockPortIter->second.ordering->end(), outputPortPosition);
						if (iter == inputRenderedBlockPortIter->second.ordering->end()) {
							logError("Could not find port {} in ordering.", "CircuitRenderManager", outputPortPosition);
							continue;
						}
						inputRenderedBlockPortIter->second.ordering->erase(iter);
						inputRenderedBlockPortIter->second.ordering->push_back(newOutputPortPosition);
					}
				}
			}
			for (const auto& [inputPortPosition, inputRenderedBlockPort] : oldInputConnectionsCopy) {
				Position newInputPortPosition = newBlockPosition + transformAmount.transformVectorWithArea(inputPortPosition - curBlockPosition, blockSize);
				if (inputRenderedBlockPort.ordering.has_value()) {
					blockIter->second.inputConnections[newInputPortPosition].ordering.emplace();
				}
				inputPorts.emplace_back(newBlockPosition, newInputPortPosition);
				for (const auto& [outputPortPosition, outputBlockPosition] : inputRenderedBlockPort.connections) {
					if ((moveType == MoveType::SINGLE || moveType == MoveType::MULTI_BEGIN) && outputBlockPosition.x != 10000000) {
						MainRenderer::get().removeWire(viewportId, {outputPortPosition, inputPortPosition});
					}
					if (outputBlockPosition == curBlockPosition) {
						Position newOutputPortPosition = newBlockPosition + transformAmount.transformVectorWithArea(outputPortPosition - curBlockPosition, blockSize);
						blockIter->second.outputConnections[newOutputPortPosition].connections.emplace(newInputPortPosition, newBlockPosition);
						if (inputRenderedBlockPort.ordering.has_value()) {
							blockIter->second.inputConnections[newInputPortPosition].ordering->emplace_back(newOutputPortPosition);
						}
						blockIter->second.inputConnections[newInputPortPosition].connections.emplace(newOutputPortPosition, newBlockPosition);
					} else {
						auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
						if (outputBlockIter == renderedBlocks.end()) {
							logError("Could not find block at {} to move connection.", "CircuitRenderManager", outputBlockPosition);
							continue;
						}
						auto outputRenderedBlockPortIter = outputBlockIter->second.outputConnections.find(outputPortPosition);
						if (outputRenderedBlockPortIter == outputBlockIter->second.outputConnections.end()) {
							logError("Could not find port at {} to move connection.", "CircuitRenderManager", outputPortPosition);
							continue;
						}
						outputRenderedBlockPortIter->second.connections.erase(inputPortPosition);
						outputRenderedBlockPortIter->second.connections.emplace(newInputPortPosition, newBlockPosition);
						blockIter->second.inputConnections[newInputPortPosition].connections.emplace(outputPortPosition, outputBlockPosition);
						if (inputRenderedBlockPort.ordering.has_value()) {
							blockIter->second.inputConnections[newInputPortPosition].ordering->emplace_back(outputPortPosition);
						}
					}
				}
			}
			if (moveType == MoveType::SINGLE || moveType == MoveType::MULTI_FINAL) {
				assert(newBlockPosition.x != 10000000);
				for (const auto& [inputBlockPosition, inputPortPosition] : inputPorts) {
					createConnectionsForInputPort(inputBlockPosition, inputPortPosition);
					// auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
					// assert(inputBlockIter != renderedBlocks.end());
					// InputRenderedBlockPort& inputRenderedBlockPort = inputBlockIter->second.inputConnections.at(inputBlockPosition);
				}
			} else {
				assert(newBlockPosition.x == 10000000);
			}
		}
	}
	}
	MainRenderer::get().stopMakingEdits(viewportId);
}

void CircuitRenderManager::createConnectionsForInputPort(Position inputBlockPosition, Position inputPortPosition) {
	if (inputBlockPosition.x == 10000000) return;
	auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
	if (inputBlockIter == renderedBlocks.end()) {
		logError("Could not find block position {}.", "CircuitRenderManager::createConnectionsForInputPort", inputBlockPosition);
		return;
	}
	auto inputPortIter = inputBlockIter->second.inputConnections.find(inputPortPosition);
	if (inputPortIter == inputBlockIter->second.inputConnections.end()) return;
	const BlockDataManager& blockDataManager = environment.getBackend().getCircuitManager().getBlockDataManager();
	const BlockData* inputBlockData = blockDataManager.getBlockData(inputBlockIter->second.type);
	assert(inputBlockData);
	std::optional<connection_end_id_t> inputEndId = inputBlockData->getInputOrBidirectionalConnectionId(
		inputPortPosition - inputBlockPosition, inputBlockIter->second.orientation);
	if (!inputEndId) return;
	FVector inputOffset = inputBlockData->getConnectionPortOffset(inputEndId.value(), inputBlockIter->second.orientation).value_or(FVector(0.5));
	if (inputPortIter->second.ordering.has_value()) {
		float spacing = 0.4 / (float)inputPortIter->second.ordering->size();
		float start = spacing / 2.0 - 0.4 / 2.0f;
		bool orderOnX = abs(inputOffset.dx - 0.5) < abs(inputOffset.dy - 0.5);
		sortPortVector(inputPortPosition, orderOnX, inputPortIter->second.ordering.value());
		for (unsigned int i = 0; i < inputPortIter->second.ordering.value().size(); i++) {
			Position outputPortPosition = inputPortIter->second.ordering.value()[i];
			Position outputBlockPosition = inputPortIter->second.connections.at(outputPortPosition);
			if (outputBlockPosition.x == 10000000) continue;
			if (inputBlockPosition == outputBlockPosition) {
				std::optional<connection_end_id_t> outputEndId = inputBlockData->getOutputOrBidirectionalConnectionId(
					outputPortPosition - inputBlockPosition, inputBlockIter->second.orientation);
				if (!outputEndId) continue;
				MainRenderer::get().addWire(viewportId, std::make_pair(outputPortPosition, inputPortPosition), std::make_pair(
					inputBlockData->getConnectionPortOffset(outputEndId.value(), inputBlockIter->second.orientation).value_or(FVector(0.5)),
					inputOffset + (orderOnX ? FVector(i * spacing + start, 0) : FVector(0, i * spacing + start))
				));
			} else {
				auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
				if (outputBlockIter == renderedBlocks.end()) {
					logError("Could not find block at {} to connection with.", "CircuitRenderManager", outputBlockPosition);
					continue;
				}
				const BlockData* outputBlockData = blockDataManager.getBlockData(outputBlockIter->second.type);
				assert(outputBlockData);
				std::optional<connection_end_id_t> outputEndId = outputBlockData->getOutputOrBidirectionalConnectionId(
					outputPortPosition - outputBlockPosition, outputBlockIter->second.orientation);
				if (!outputEndId) continue;
				std::optional<connection_end_id_t> inputEndId = inputBlockData->getInputOrBidirectionalConnectionId(
					inputPortPosition - inputBlockPosition, inputBlockIter->second.orientation);
				if (!inputEndId) continue;
				MainRenderer::get().addWire(viewportId, std::make_pair(outputPortPosition, inputPortPosition), std::make_pair(
					outputBlockData->getConnectionPortOffset(outputEndId.value(), outputBlockIter->second.orientation).value_or(FVector(0.5)),
					inputOffset + (orderOnX ? FVector(i * spacing + start, 0) : FVector(0, i * spacing + start))
				));
			}
		}
	} else {
		for (const auto& [outputPortPosition, outputBlockPosition] : inputPortIter->second.connections) {
			if (outputPortPosition == inputPortPosition) {
				std::optional<connection_end_id_t> outputEndId = inputBlockData->getOutputOrBidirectionalConnectionId(
					outputPortPosition - inputBlockPosition, inputBlockIter->second.orientation);
				if (!outputEndId) continue;
				std::optional<connection_end_id_t> inputEndId = inputBlockData->getInputOrBidirectionalConnectionId(
					inputPortPosition - inputBlockPosition, inputBlockIter->second.orientation);
				if (!inputEndId) continue;
				MainRenderer::get().addWire(viewportId, std::make_pair(outputPortPosition, inputPortPosition), std::make_pair(
					inputBlockData->getConnectionPortOffset(outputEndId.value(), inputBlockIter->second.orientation).value_or(FVector(0.5)),
					inputBlockData->getConnectionPortOffset(inputEndId.value(), inputBlockIter->second.orientation).value_or(FVector(0.5))
				));
			} else {
				auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
				if (outputBlockIter == renderedBlocks.end()) {
					logError("Could not find block at {} to connection with.", "CircuitRenderManager", outputBlockPosition);
					continue;
				}
				const BlockData* outputBlockData = blockDataManager.getBlockData(outputBlockIter->second.type);
				assert(outputBlockData);
				std::optional<connection_end_id_t> outputEndId = outputBlockData->getOutputOrBidirectionalConnectionId(
					outputPortPosition - outputBlockPosition, outputBlockIter->second.orientation);
				if (!outputEndId) continue;
				MainRenderer::get().addWire(viewportId, std::make_pair(outputPortPosition, inputPortPosition), std::make_pair(
					outputBlockData->getConnectionPortOffset(outputEndId.value(), outputBlockIter->second.orientation).value_or(FVector(0.5)),
					inputOffset
				));
			}
		}
	}
}

void CircuitRenderManager::sortPortVector(Position inputPortPosition, bool orderOnX, std::vector<Position>& outputPortPositions) {
	std::sort(outputPortPositions.begin(), outputPortPositions.end(), [&](Position a, Position b) {
		if (orderOnX) {
			if (a.x == b.x) {
				if (a.x > inputPortPosition.x) return abs(a.y - inputPortPosition.y) > abs(b.y - inputPortPosition.y);
				return abs(a.y - inputPortPosition.y) < abs(b.y - inputPortPosition.y);
			}
			return a.x < b.x;
		}
		if (a.y == b.y) {
			if (a.y > inputPortPosition.y) return abs(a.x - inputPortPosition.x) > abs(b.x - inputPortPosition.x);
			return abs(a.x - inputPortPosition.x) < abs(b.x - inputPortPosition.x);
		}
		return a.y < b.y;
	});
}

void CircuitRenderManager::orderConnection(Position& outputBlockPosition, Position& outputPortPosition, Position& inputBlockPosition, Position& inputPortPosition) {
	auto outputBlockIter = renderedBlocks.find(outputBlockPosition);
	if (outputBlockIter == renderedBlocks.end()) {
		logError("Could not find block at {} in renderedBlocks.", "CircuitRenderManager::orderConnection", outputBlockPosition);
		return;
	}
	const BlockData* outputBlockData = environment.getBackend().getBlockDataManager().getBlockData(outputBlockIter->second.type);
	if (outputBlockData == nullptr) {
		logError("Could not find block type {}.", "CircuitRenderManager::orderConnection", outputBlockIter->second.type);
		return;
	}
	if (!outputBlockData->getBidirectionalConnectionId(outputPortPosition - outputBlockPosition, outputBlockIter->second.orientation).has_value()) return;
	auto inputBlockIter = renderedBlocks.find(inputBlockPosition);
	if (inputBlockIter == renderedBlocks.end()) {
		logError("Could not find block at {} in renderedBlocks.", "CircuitRenderManager::orderConnection", inputBlockPosition);
		return;
	}
	const BlockData* inputBlockData = environment.getBackend().getBlockDataManager().getBlockData(inputBlockIter->second.type);
	if (inputBlockData == nullptr) {
		logError("Could not find block type {}.", "CircuitRenderManager::orderConnection", inputBlockIter->second.type);
		return;
	}
	if (!inputBlockData->getBidirectionalConnectionId(inputPortPosition - inputBlockPosition, inputBlockIter->second.orientation).has_value()) return;
	if (outputPortPosition.x < inputPortPosition.x || (outputPortPosition.x == inputPortPosition.x && outputPortPosition.y < inputPortPosition.y)) return;
	std::swap(outputBlockPosition, inputBlockPosition);
	std::swap(outputPortPosition, inputPortPosition);
}
