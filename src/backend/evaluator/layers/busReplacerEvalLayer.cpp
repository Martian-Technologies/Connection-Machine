#include "busReplacerEvalLayer.h"
#include "backend/circuit/circuitManager.h"
#include "evalLayerState.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

void BusReplacerEvalLayer::run() {
	#ifdef TRACY_PROFILER
	ZoneScopedN("BusReplacer Run");
	#endif
	for (auto iter : currentState.getRemovedConnections()) {
		auto bussesIterA = busses.find(iter.first.connectionPointA.gateId);
		auto bussesIterB = busses.find(iter.first.connectionPointB.gateId);
		if (bussesIterA != busses.end() && bussesIterB != busses.end()) {
			auto busConnectionEndIdIterA = bussesIterA->second.connectionEnds.find(iter.first.connectionPointA.connectionEndId);
			assert(busConnectionEndIdIterA != bussesIterA->second.connectionEnds.end());
			auto busConnectionEndIdIterB = bussesIterB->second.connectionEnds.find(iter.first.connectionPointB.connectionEndId);
			assert(busConnectionEndIdIterB != bussesIterB->second.connectionEnds.end());
			assert(busConnectionEndIdIterA->second.size() == busConnectionEndIdIterB->second.size());
			for (unsigned int i = 0; i < busConnectionEndIdIterA->second.size(); i++) {
				auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second[i]);
				assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
				auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second[i]);
				assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
				nextState.removeConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterA->second, 0),
					EvalConnectionPoint(busLaneJunctionIterB->second, 0)
				), iter.second);
			}
		} else if (bussesIterA != busses.end()) {
			auto busConnectionEndIdIterA = bussesIterA->second.connectionEnds.find(iter.first.connectionPointA.connectionEndId);
			assert(busConnectionEndIdIterA != bussesIterA->second.connectionEnds.end());
			if (busConnectionEndIdIterA->second.size() == 1) { // bitwidth of 1
				auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second.front());
				assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
				assert(!busJunctions.contains(iter.first.connectionPointB.gateId)); // should not be a junction because there are not busJunctions of bitwidth 1
				nextState.removeConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterA->second, 0),
					iter.first.connectionPointB
				), iter.second);
			} else {
				auto busJunctionsIter = busJunctions.find(iter.first.connectionPointB.gateId);
				assert(busJunctionsIter != busJunctions.end());
				for (unsigned int i = 0; i < busConnectionEndIdIterA->second.size(); i++) {
					auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second[i]);
					assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
					nextState.removeConnection(EvalConnection(
						EvalConnectionPoint(busLaneJunctionIterA->second, 0),
						EvalConnectionPoint(busJunctionsIter->second[i], 0)
					), iter.second);
				}
				const EvalGate* nextGate = nextState.getGate(busJunctionsIter->second[0]);
				assert(nextGate);
				if (nextGate->connections.empty()) {
					// const EvalGate* gateB = currentState.getGate(iter.first.connectionPointB.gateId);
					// if (!gateB || gateB->connections.empty()) {
						for (unsigned int i = 1; i < busJunctionsIter->second.size(); i++) {
							nextState.removeGate(busJunctionsIter->second[i]);
							nextState.getGateIdReverseRemapping().erase(busJunctionsIter->second[i]);
						}
						if (currentState.getGate(busJunctionsIter->second[0])) nextState.addGateIdRemappingsUpdated(busJunctionsIter->second[0]);
						busJunctions.erase(busJunctionsIter);
					// }
				}
			}
		} else if (bussesIterB != busses.end()) {
			auto busConnectionEndIdIterB = bussesIterB->second.connectionEnds.find(iter.first.connectionPointB.connectionEndId);
			assert(busConnectionEndIdIterB != bussesIterB->second.connectionEnds.end());
			if (busConnectionEndIdIterB->second.size() == 1) { // bitwidth of 1
				auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second.front());
				assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
				assert(!busJunctions.contains(iter.first.connectionPointA.gateId)); // should not be a junction because there are not busJunctions of bitwidth 1
				nextState.removeConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterB->second, 0),
					iter.first.connectionPointA
				), iter.second);
			} else {
				auto busJunctionsIter = busJunctions.find(iter.first.connectionPointA.gateId);
				assert(busJunctionsIter != busJunctions.end());
				for (unsigned int i = 0; i < busConnectionEndIdIterB->second.size(); i++) {
					auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second[i]);
					assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
					nextState.removeConnection(EvalConnection(
						EvalConnectionPoint(busLaneJunctionIterB->second, 0),
						EvalConnectionPoint(busJunctionsIter->second[i], 0)
					), iter.second);
				}
				const EvalGate* nextGate = nextState.getGate(busJunctionsIter->second[0]);
				assert(nextGate);
				if (nextGate->connections.empty()) {
					// const EvalGate* gateA = currentState.getGate(iter.first.connectionPointA.gateId);
					// if (!gateA || gateA->connections.empty()) {
						for (unsigned int i = 1; i < busJunctionsIter->second.size(); i++) {
							nextState.removeGate(busJunctionsIter->second[i]);
							nextState.getGateIdReverseRemapping().erase(busJunctionsIter->second[i]);
						}
						if (currentState.getGate(busJunctionsIter->second[0])) nextState.addGateIdRemappingsUpdated(busJunctionsIter->second[0]);
						busJunctions.erase(busJunctionsIter);
					// }
				}
			}
		} else {
			nextState.removeConnection(iter.first, iter.second);
		}
	}
	for (auto iter : currentState.getRemovedGates()) {
		auto bussesIter = busses.find(iter.first);
		if (bussesIter != busses.end()) {
			for (std::pair<unsigned int, eval_gate_id> junctionId : bussesIter->second.junctions) {
				nextState.removeGate(junctionId.second);
				auto iterPair = nextState.getConnectionPointReverseRemapping().equal_range(EvalConnectionPoint(junctionId.second, 0));
				for (auto iter = iterPair.first; iter != iterPair.second; iter++) nextState.getConnectionPointRemapping().erase(iter->second);
				nextState.getConnectionPointReverseRemapping().erase(iterPair.first, iterPair.second);
			}
			busses.erase(bussesIter);
		} else {
			nextState.getGateIdRemapping().erase(iter.first);
			nextState.getGateIdReverseRemapping().erase(iter.first);
			nextState.removeGate(iter.first);
		}
	}
	for (auto iter : currentState.getAddedGates()) {
		const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(iter.second));
		assert(blockData);
		if (blockData->isBus()) {
			auto iterBool = busses.try_emplace(iter.first);
			assert(iterBool.second);
			for (std::pair<connection_end_id_t, BlockData::ConnectionData> connectionData : blockData->getConnectionsSafe()) {
				auto iterBool2 = iterBool.first->second.connectionEnds.try_emplace(connectionData.first);
				const std::variant<unsigned int, std::vector<unsigned int>>& bitConfiguration = connectionData.second.bitConfiguration;
				if (std::holds_alternative<unsigned int>(bitConfiguration)) {
					if (std::get<unsigned int>(bitConfiguration) == 1) {
						iterBool2.first->second.push_back(0);
						auto pair = iterBool.first->second.junctions.emplace(0, 0);
						if (pair.second) {
							eval_gate_id junctionId = nextState.getUnusedEvalGateId();
							pair.first->second = junctionId;
							nextState.addGate(junctionId, getEvalGateType(BlockType::JUNCTION));
						}
						nextState.getConnectionPointReverseRemapping().emplace(
							EvalConnectionPoint(pair.first->second, 0),
							EvalConnectionPoint(iter.first, connectionData.first)
						);
						nextState.getConnectionPointRemapping().emplace(
							EvalConnectionPoint(iter.first, connectionData.first),
							EvalConnectionPoint(pair.first->second, 0)
						);
						nextState.addGateIdRemappingsUpdated(pair.first->second);
					} else {
						for (unsigned int lane = 0; lane < std::get<unsigned int>(bitConfiguration); lane++) {
							iterBool2.first->second.push_back(lane);
							auto pair = iterBool.first->second.junctions.emplace(lane, 0);
							if (pair.second) {
								eval_gate_id junctionId = nextState.getUnusedEvalGateId();
								pair.first->second = junctionId;
								nextState.addGate(junctionId, getEvalGateType(BlockType::JUNCTION));
							}
							nextState.getConnectionPointReverseRemapping().emplace(
								EvalConnectionPoint(pair.first->second, 0),
								EvalConnectionPoint(iter.first, connectionData.first)
							);
							nextState.addGateIdRemappingsUpdated(pair.first->second);
						}
					}
				} else {
					const std::vector<unsigned int>& bitConfigurationVec = std::get<std::vector<unsigned int>>(bitConfiguration);
					if (bitConfigurationVec.size() == 1) {
						iterBool2.first->second.push_back(bitConfigurationVec.front());
						auto pair = iterBool.first->second.junctions.emplace(bitConfigurationVec.front(), 0);
						if (pair.second) {
							eval_gate_id junctionId = nextState.getUnusedEvalGateId();
							pair.first->second = junctionId;
							nextState.addGate(junctionId, getEvalGateType(BlockType::JUNCTION));
						}
						nextState.getConnectionPointReverseRemapping().emplace(
							EvalConnectionPoint(pair.first->second, 0),
							EvalConnectionPoint(iter.first, connectionData.first)
						);
						nextState.getConnectionPointRemapping().emplace(
							EvalConnectionPoint(iter.first, connectionData.first),
							EvalConnectionPoint(pair.first->second, 0)
						);
						nextState.addGateIdRemappingsUpdated(pair.first->second);
					} else {
						for (unsigned int lane : bitConfigurationVec) {
							iterBool2.first->second.push_back(lane);
							auto pair = iterBool.first->second.junctions.emplace(lane, 0);
							if (pair.second) {
								eval_gate_id junctionId = nextState.getUnusedEvalGateId();
								pair.first->second = junctionId;
								nextState.addGate(junctionId, getEvalGateType(BlockType::JUNCTION));
							}
							nextState.getConnectionPointReverseRemapping().emplace(
								EvalConnectionPoint(pair.first->second, 0),
								EvalConnectionPoint(iter.first, connectionData.first)
							);
							nextState.addGateIdRemappingsUpdated(pair.first->second);
						}
					}
				}
			}
		} else {
			nextState.getGateIdRemapping().try_emplace(iter.first, iter.first);
			nextState.getGateIdReverseRemapping().emplace(iter.first, iter.first);
			nextState.addGate(iter.first, iter.second);
		}
	}
	for (auto iter : currentState.getAddedConnections()) {
		auto bussesIterA = busses.find(iter.first.connectionPointA.gateId);
		auto bussesIterB = busses.find(iter.first.connectionPointB.gateId);
		if (bussesIterA != busses.end() && bussesIterB != busses.end()) {
			auto busConnectionEndIdIterA = bussesIterA->second.connectionEnds.find(iter.first.connectionPointA.connectionEndId);
			assert(busConnectionEndIdIterA != bussesIterA->second.connectionEnds.end());
			auto busConnectionEndIdIterB = bussesIterB->second.connectionEnds.find(iter.first.connectionPointB.connectionEndId);
			assert(busConnectionEndIdIterB != bussesIterB->second.connectionEnds.end());
			assert(busConnectionEndIdIterA->second.size() == busConnectionEndIdIterB->second.size());
			for (unsigned int i = 0; i < busConnectionEndIdIterA->second.size(); i++) {
				auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second[i]);
				assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
				auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second[i]);
				assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
				nextState.addConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterA->second, 0),
					EvalConnectionPoint(busLaneJunctionIterB->second, 0)
				), iter.second);
			}
		} else if (bussesIterA != busses.end()) {
			auto busConnectionEndIdIterA = bussesIterA->second.connectionEnds.find(iter.first.connectionPointA.connectionEndId);
			assert(busConnectionEndIdIterA != bussesIterA->second.connectionEnds.end());
			if (busConnectionEndIdIterA->second.size() == 1) { // bitwidth of 1
				auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second.front());
				assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
				assert(!busJunctions.contains(iter.first.connectionPointB.gateId)); // should not be a junction because there are not busJunctions of bitwidth 1
				nextState.addConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterA->second, 0),
					iter.first.connectionPointB
				), iter.second);
			} else {
				auto busJunctionsIterPair = busJunctions.try_emplace(iter.first.connectionPointB.gateId);
				if (busJunctionsIterPair.second) {
					const EvalGate* gateB = currentState.getGate(iter.first.connectionPointB.gateId);
					assert(isJunctionType(gateB->type)); // nothing else thats not a bus of junction takes and non 1 bitwidth
					assert(nextState.getGate(iter.first.connectionPointB.gateId)->connections.empty()); // junctions should be merged, it has no connections
					busJunctionsIterPair.first->second.push_back(iter.first.connectionPointB.gateId);
					for (unsigned int i = 1; i < busConnectionEndIdIterA->second.size(); i++) {
						eval_gate_id junctionId = nextState.getUnusedEvalGateId();
						busJunctionsIterPair.first->second.push_back(junctionId);
						nextState.addGate(junctionId, gateB->type);
						assert(currentState.getGate(iter.first.connectionPointA.gateId));
						nextState.getGateIdReverseRemapping().emplace(junctionId, iter.first.connectionPointB.gateId);
						nextState.addGateIdRemappingsUpdated(junctionId);
					}
				}
				for (unsigned int i = 0; i < busConnectionEndIdIterA->second.size(); i++) {
					auto busLaneJunctionIterA = bussesIterA->second.junctions.find(busConnectionEndIdIterA->second[i]);
					assert(busLaneJunctionIterA != bussesIterA->second.junctions.end());
					nextState.addConnection(EvalConnection(
						EvalConnectionPoint(busLaneJunctionIterA->second, 0),
						EvalConnectionPoint(busJunctionsIterPair.first->second[i], 0)
					), iter.second);
				}
			}
		} else if (bussesIterB != busses.end()) {
			auto busConnectionEndIdIterB = bussesIterB->second.connectionEnds.find(iter.first.connectionPointB.connectionEndId);
			assert(busConnectionEndIdIterB != bussesIterB->second.connectionEnds.end());
			if (busConnectionEndIdIterB->second.size() == 1) { // bitwidth of 1
				auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second.front());
				assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
				assert(!busJunctions.contains(iter.first.connectionPointA.gateId)); // should not be a junction because there are not busJunctions of bitwidth 1
				nextState.addConnection(EvalConnection(
					EvalConnectionPoint(busLaneJunctionIterB->second, 0),
					iter.first.connectionPointA
				), iter.second);
			} else {
				auto busJunctionsIterPair = busJunctions.try_emplace(iter.first.connectionPointA.gateId);
				if (busJunctionsIterPair.second) {
					const EvalGate* gateA = currentState.getGate(iter.first.connectionPointA.gateId);
					assert(isJunctionType(gateA->type)); // nothing else thats not a bus of junction takes and non 1 bitwidth
					assert(nextState.getGate(iter.first.connectionPointA.gateId)->connections.empty()); // junctions should be merged, it has no connections
					busJunctionsIterPair.first->second.push_back(iter.first.connectionPointA.gateId);
					for (unsigned int i = 1; i < busConnectionEndIdIterB->second.size(); i++) {
						eval_gate_id junctionId = nextState.getUnusedEvalGateId();
						busJunctionsIterPair.first->second.push_back(junctionId);
						nextState.addGate(junctionId, gateA->type);
						assert(currentState.getGate(iter.first.connectionPointB.gateId));
						nextState.getGateIdReverseRemapping().emplace(junctionId, iter.first.connectionPointA.gateId);
						nextState.addGateIdRemappingsUpdated(junctionId);
					}
				}
				for (unsigned int i = 0; i < busConnectionEndIdIterB->second.size(); i++) {
					auto busLaneJunctionIterB = bussesIterB->second.junctions.find(busConnectionEndIdIterB->second[i]);
					assert(busLaneJunctionIterB != bussesIterB->second.junctions.end());
					nextState.addConnection(EvalConnection(
						EvalConnectionPoint(busLaneJunctionIterB->second, 0),
						EvalConnectionPoint(busJunctionsIterPair.first->second[i], 0)
					), iter.second);
				}
			}
		} else {
			nextState.addConnection(iter.first, iter.second);
		}
	}
	for (eval_gate_id gateId : currentState.getGateIdRemappingsUpdateds()) {
		auto bussesIter = busses.find(gateId);
		if (bussesIter != busses.end()) {
			for (std::pair<unsigned int, eval_gate_id> junctionId : bussesIter->second.junctions) {
				nextState.addGateIdRemappingsUpdated(junctionId.second);
			}
			continue;
		}
		auto busJunctionsIter = busJunctions.find(gateId);
		if (busJunctionsIter != busJunctions.end()) {
			for (auto otherGateId : busJunctionsIter->second) {
				nextState.addGateIdRemappingsUpdated(otherGateId);
			}
			continue;
		}
		assert(nextState.getGate(gateId));
		nextState.addGateIdRemappingsUpdated(gateId);
	}
	for (EvalConnectionPoint connectionPoint : currentState.getConnectionPointRemappingsUpdated()) {
		auto bussesIter = busses.find(connectionPoint.gateId);
		if (bussesIter != busses.end()) {
			for (std::pair<unsigned int, eval_gate_id> junctionId : bussesIter->second.junctions) {
				nextState.addGateIdRemappingsUpdated(junctionId.second);
			}
			continue;
		}
		if (connectionPoint.connectionEndId == 0) {
			auto busJunctionsIter = busJunctions.find(connectionPoint.gateId);
			if (busJunctionsIter != busJunctions.end()) {
				for (auto otherGateId : busJunctionsIter->second) {
					nextState.addGateIdRemappingsUpdated(otherGateId);
				}
				continue;
			}
		}
		assert(nextState.getGate(connectionPoint.gateId));
		nextState.addConnectionPointRemappingsUpdated(connectionPoint);
	}
}

std::variant<EvalConnectionPoint, VecEvalConnectionPoint> BusReplacerEvalLayer::getEvalConnectionPointsForConnectionPoint(EvalConnectionPoint connectionPoint) const {
	if (connectionPoint.isNull()) return EvalConnectionPoint::null();
	auto bussesIter = busses.find(connectionPoint.gateId);
	if (bussesIter != busses.end()) {
		auto connectionEndsIter = bussesIter->second.connectionEnds.find(connectionPoint.connectionEndId);
		if (connectionEndsIter == bussesIter->second.connectionEnds.end()) {
			return EvalConnectionPoint::null();
		}
		if (connectionEndsIter->second.size() == 1) {
			return EvalConnectionPoint(bussesIter->second.junctions.at(connectionEndsIter->second.front()), 0);
		}
		VecEvalConnectionPoint connectionPoints;
		for (unsigned int lane : connectionEndsIter->second) {
			connectionPoints.push_back(EvalConnectionPoint(bussesIter->second.junctions.at(lane), 0));
		}
		return connectionPoints;
	}
	if (connectionPoint.connectionEndId == 0) {
		auto busJunctionIter = busJunctions.find(connectionPoint.gateId);
		if (busJunctionIter != busJunctions.end()) {
			VecEvalConnectionPoint connectionPoints;
			for (eval_gate_id junctionId : busJunctionIter->second) {
				connectionPoints.push_back(EvalConnectionPoint(junctionId, 0));
			}
			return connectionPoints;
		}
	}
	return connectionPoint;
}
