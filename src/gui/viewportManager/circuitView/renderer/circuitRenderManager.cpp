#include "circuitRenderManager.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/circuit/circuit.h"
#include "gpu/mainRenderer.h"
#include "backend/backend.h"

CircuitRenderManager::CircuitRenderManager(Backend& backend, circuit_id_t circuitId, ViewportId viewportId) : backend(backend), circuitId(circuitId), viewportId(viewportId) {
	SharedCircuit circuit = backend.getCircuit(circuitId);
	if (!circuit) {
		logError("Failed to find circuit with Id {}", "CircuitRenderManager", circuitId);
		return;
	}
	circuit->connectListener(this, [this](DifferenceSharedPtr diff, circuit_id_t circuitId) {if (circuitId == this->circuitId) addDifference(diff); });
	addDifference(circuit->getBlockContainer().getCreationDifferenceShared());
}

CircuitRenderManager::~CircuitRenderManager() {
	SharedCircuit circuit = backend.getCircuit(circuitId);
	if (!circuit) return; // not an error because the circuit may have already been destroyed
	MainRenderer::get().resetCircuit(viewportId);
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

	SharedCircuit circuit = backend.getCircuit(circuitId);
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
			MainRenderer::get().addBlock(viewportId, blockType, position, orientation);
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
			if (iter->second.connectionsToOtherBlock.size() != 0) {
				logError("Trying to remove block at {} that still has {} connections.", "CircuitRenderManager", position, iter->second.connectionsToOtherBlock.size());
				continue;
			}
			renderedBlocks.erase(iter);
			break;
		}
		case Difference::ModificationType::CREATED_CONNECTION:
		{
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);

			// uses position of output and input CELLS fed into offset function
			std::pair<Position, Position> newConnection = { outputPosition, inputPosition };

			auto outputIter = renderedBlocks.find(outputBlockPosition);
			if (outputIter == renderedBlocks.end()) {
				logError("Could not find block at {} to add output connection to.", "CircuitRenderManager", outputBlockPosition);
			}
			outputIter->second.connectionsToOtherBlock.emplace(newConnection, inputBlockPosition);

			std::optional<ConnectionEnd> outputConnectionEnd = circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(outputPosition);
			if (!outputConnectionEnd) continue;
			std::optional<ConnectionEnd> inputConnectionEnd = circuit->getBlockContainer().getInputOrBidirectionalConnectionEnd(inputPosition);
			if (!inputConnectionEnd) continue;

			// only need both if it is a different block
			if (outputBlockPosition != inputBlockPosition) {
				auto inputIter = renderedBlocks.find(inputBlockPosition);
				if (inputIter == renderedBlocks.end()) {
					logError("Could not find block at {} to add input connection to.", "CircuitRenderManager", inputBlockPosition);
					continue;
				}
				inputIter->second.connectionsToOtherBlock.emplace(newConnection, outputBlockPosition);
				MainRenderer::get().addWire(viewportId, newConnection, {
					blockDataManager.getBlockData(outputIter->second.type)->getConnectionPortOffset(outputConnectionEnd.value().getConnectionId(), outputIter->second.orientation).value_or(FVector(0.5)),
					blockDataManager.getBlockData(inputIter->second.type)->getConnectionPortOffset(inputConnectionEnd.value().getConnectionId(), inputIter->second.orientation).value_or(FVector(0.5))
					// getOutputOffset(outputIter->second.type, outputConnectionEnd.value().getConnectionId(), outputIter->second.orientation),
					// getInputOffset(inputIter->second.type, inputConnectionEnd.value().getConnectionId(), inputIter->second.orientation)
				});
			} else {
				MainRenderer::get().addWire(viewportId, newConnection, {
					blockDataManager.getBlockData(outputIter->second.type)->getConnectionPortOffset(outputConnectionEnd.value().getConnectionId(), outputIter->second.orientation).value_or(FVector(0.5)),
					blockDataManager.getBlockData(outputIter->second.type)->getConnectionPortOffset(inputConnectionEnd.value().getConnectionId(), outputIter->second.orientation).value_or(FVector(0.5))
					// getOutputOffset(outputIter->second.type, outputConnectionEnd.value().getConnectionId(), outputIter->second.orientation),
					// getInputOffset(outputIter->second.type, inputConnectionEnd.value().getConnectionId(), outputIter->second.orientation)
				});
			}
			break;
		}
		case Difference::ModificationType::REMOVED_CONNECTION:
		{
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);

			MainRenderer::get().removeWire(viewportId, { outputPosition, inputPosition });

			auto outputIter = renderedBlocks.find(outputBlockPosition);
			if (outputIter == renderedBlocks.end()) {
				logError("Could not find block at {} to remove output connection from.", "CircuitRenderManager", outputBlockPosition);
			}
			auto returnVal = outputIter->second.connectionsToOtherBlock.erase({ outputPosition, inputPosition });
			if (!returnVal) {
				if (!outputIter->second.connectionsToOtherBlock.erase({ inputPosition, outputPosition })) {
					logError("Cound not remove connection {}, {} from output block", "", outputPosition, inputPosition);
				}
			}

			// only need both if it is a different block
			if (outputBlockPosition != inputBlockPosition) {
				auto inputIter = renderedBlocks.find(inputBlockPosition);
				if (inputIter == renderedBlocks.end()) {
					logError("Could not find block at {} to remove input connection from.", "CircuitRenderManager", inputBlockPosition);
				}
				auto returnVal2 = inputIter->second.connectionsToOtherBlock.erase({ outputPosition, inputPosition });
				if (!returnVal2) {
					if (!inputIter->second.connectionsToOtherBlock.erase({ inputPosition, outputPosition })) {
						logError("Cound not remove connection {}, {} from inpuit block", "", outputPosition, inputPosition);
					}
				}
			}
			break;
		}
		case Difference::ModificationType::MOVE_BLOCK:
		{
			const auto& [curPosition, curOrientation, newPosition, newOrientation, moveType] = std::get<Difference::move_modification_t>(modificationData);
			if (curPosition == newPosition && curOrientation == newOrientation) continue;

			auto iter = renderedBlocks.find(curPosition);
			if (iter == renderedBlocks.end()) {
				logError("Could not find block at {} to move.", "CircuitRenderManager", curPosition);
				continue;
			}

			if (curPosition != newPosition) {
				auto newIter = renderedBlocks.emplace(newPosition, std::move(iter->second)).first;
				renderedBlocks.erase(iter);
				iter = newIter;
			}

			iter->second.orientation = newOrientation;

			// MOVE BLOCK
			MainRenderer::get().moveBlock(viewportId, curPosition, newPosition, newOrientation);

			Size blockSize = blockDataManager.getBlockSize(iter->second.type, curOrientation);
			Orientation transformAmount = newOrientation.relativeTo(curOrientation);

			// MOVE CONNECTIONS
			std::unordered_map<std::pair<Position, Position>, Position> oldConnectionsToOtherBlock = std::move(iter->second.connectionsToOtherBlock);
			iter->second.connectionsToOtherBlock.clear();

			std::vector<std::pair<std::pair<Position, Position>, std::pair<Position, Position>>> wiresToAdd;
			std::vector<std::pair<std::pair<Position, Position>, std::pair<FVector, FVector>>> wiresToRender;

			for (auto& [posPair, otherBlockPos] : oldConnectionsToOtherBlock) {
				if ((moveType == MoveType::SINGLE || moveType == MoveType::MULTI_BEGIN) && otherBlockPos.x != 10000000) {
					MainRenderer::get().removeWire(viewportId, posPair);
				}
				if (otherBlockPos == curPosition) {
					Vector outputVec = transformAmount.transformVectorWithArea(posPair.first - curPosition, blockSize);
					Position outputPos = newPosition + outputVec;
					Vector inputVec = transformAmount.transformVectorWithArea(posPair.second - curPosition, blockSize);
					Position inputPos = newPosition + inputVec;
					if (moveType == MoveType::SINGLE || moveType == MoveType::MULTI_FINAL) {
						const BlockData* blockData = circuit->getBlockContainer().getBlockDataManager().getBlockData(iter->second.type);
						std::optional<connection_end_id_t> outputEndId = blockData->getOutputOrBidirectionalConnectionId(outputVec, newOrientation);
						if (!outputEndId) continue;
						std::optional<connection_end_id_t> inputEndId = blockData->getInputOrBidirectionalConnectionId(inputVec, newOrientation);
						if (!inputEndId) continue;
						wiresToRender.emplace_back(std::make_pair(outputPos, inputPos), std::make_pair(
							blockDataManager.getBlockData(iter->second.type)->getConnectionPortOffset(outputEndId.value(), newOrientation).value_or(FVector(0.5)),
							blockDataManager.getBlockData(iter->second.type)->getConnectionPortOffset(inputEndId.value(), newOrientation).value_or(FVector(0.5))
							// getOutputOffset(iter->second.type, outputEndId.value(), newOrientation),
							// getInputOffset(iter->second.type, inputEndId.value(), newOrientation)
						));
					}
					wiresToAdd.emplace_back(std::make_pair(outputPos, inputPos), std::make_pair(newPosition, newPosition));
				} else {
					auto otherIter = renderedBlocks.find(otherBlockPos);
					if (otherIter == renderedBlocks.end()) {
						logError("Could not find block at {} to move connection.", "CircuitRenderManager", otherBlockPos);
						continue;
					}
					otherIter->second.connectionsToOtherBlock.erase(posPair);
					bool isInput = posPair.second.withinArea(curPosition, curPosition + blockSize.getLargestVectorInArea());
					if (isInput) {
						Vector inputVec = transformAmount.transformVectorWithArea(posPair.second - curPosition, blockSize);
						Position inputPos = newPosition + inputVec;
						if ((moveType == MoveType::SINGLE || moveType == MoveType::MULTI_FINAL) && otherBlockPos.x != 10000000) {
							std::optional<connection_end_id_t> outputEndId = circuit->getBlockContainer().getBlockDataManager().getOutputOrBidirectionalConnectionId(
								otherIter->second.type, otherIter->second.orientation, posPair.first - otherBlockPos
							);
							if (!outputEndId) continue;
							std::optional<connection_end_id_t> inputEndId = circuit->getBlockContainer().getBlockDataManager().getInputOrBidirectionalConnectionId(
								iter->second.type, newOrientation, inputVec
							);
							if (!inputEndId) continue;
							wiresToRender.emplace_back(std::make_pair(posPair.first, inputPos), std::make_pair(
								blockDataManager.getBlockData(otherIter->second.type)->getConnectionPortOffset(outputEndId.value(), otherIter->second.orientation).value_or(FVector(0.5)),
								blockDataManager.getBlockData(iter->second.type)->getConnectionPortOffset(inputEndId.value(), newOrientation).value_or(FVector(0.5))
								// getOutputOffset(otherIter->second.type, outputEndId.value(), otherIter->second.orientation),
								// getInputOffset(iter->second.type, inputEndId.value(), newOrientation)
							));
						}
						wiresToAdd.emplace_back(std::make_pair(posPair.first, inputPos), std::make_pair(otherBlockPos, newPosition));
					} else {
						Vector outputVec = transformAmount.transformVectorWithArea(posPair.first - curPosition, blockSize);
						Position outputPos = newPosition + outputVec;
						if ((moveType == MoveType::SINGLE || moveType == MoveType::MULTI_FINAL) && otherBlockPos.x != 10000000) {
							std::optional<connection_end_id_t> outputEndId = circuit->getBlockContainer().getBlockDataManager().getOutputOrBidirectionalConnectionId(
								iter->second.type, newOrientation, outputVec
							);
							if (!outputEndId) continue;
							std::optional<connection_end_id_t> inputEndId = circuit->getBlockContainer().getBlockDataManager().getInputOrBidirectionalConnectionId(
								otherIter->second.type, otherIter->second.orientation, posPair.second - otherBlockPos
							);
							if (!inputEndId) continue;
							wiresToRender.emplace_back(std::make_pair(outputPos, posPair.second), std::make_pair(
								blockDataManager.getBlockData(iter->second.type)->getConnectionPortOffset(outputEndId.value(), newOrientation).value_or(FVector(0.5)),
								blockDataManager.getBlockData(otherIter->second.type)->getConnectionPortOffset(inputEndId.value(), otherIter->second.orientation).value_or(FVector(0.5))
								// getOutputOffset(iter->second.type, outputEndId.value(), newOrientation),
								// getInputOffset(otherIter->second.type, inputEndId.value(), otherIter->second.orientation)
							));
						}
						wiresToAdd.emplace_back(std::make_pair(outputPos, posPair.second), std::make_pair(newPosition, otherBlockPos));
					}
				}
			}
			for (const auto& pairs : wiresToAdd) {
				if (pairs.second.first == pairs.second.second) {
					renderedBlocks.find(pairs.second.first)->second.connectionsToOtherBlock.emplace(pairs.first, pairs.second.first);
				} else {
					renderedBlocks.find(pairs.second.first)->second.connectionsToOtherBlock.emplace(pairs.first, pairs.second.second);
					renderedBlocks.find(pairs.second.second)->second.connectionsToOtherBlock.emplace(pairs.first, pairs.second.first);
				}
			}
			for (const auto& pairs : wiresToRender) {
				MainRenderer::get().addWire(viewportId, pairs.first, pairs.second);
			}
		}
		}
	}

	MainRenderer::get().stopMakingEdits(viewportId);
}
