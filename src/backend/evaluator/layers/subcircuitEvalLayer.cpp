#include "subcircuitEvalLayer.h"
#include "evalLayerState.h"
#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/evaluator.h"
#include "backend/evaluator/evaluatorInternal.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

void SubcircuitEvalLayer::run() {
	#ifdef TRACY_PROFILER
	ZoneScopedN("Subcircuit Run");
	#endif
	for (auto iter : currentState.getRemovedConnections()) {
		EvalConnection connection = iter.first;
		auto remappingAIter = nextState.getConnectionPointRemapping().find(connection.connectionPointA);
		if (remappingAIter != nextState.getConnectionPointRemapping().end()) {
			connection.connectionPointA = remappingAIter->second;
		} else {
			if (nextState.getConnectionPointRemappingToNothing().contains(connection.connectionPointA)) continue;
		}
		auto remappingBIter = nextState.getConnectionPointRemapping().find(connection.connectionPointB);
		if (remappingBIter != nextState.getConnectionPointRemapping().end()) {
			connection.connectionPointB = remappingBIter->second;
		} else {
			if (nextState.getConnectionPointRemappingToNothing().contains(connection.connectionPointB)) continue;
		}
		nextState.removeConnection(connection, iter.second);
	}
	for (auto iter : currentState.getRemovedGates()) {
		auto subcircuitDataIter = subcircuits.find(iter.first);
		if (subcircuitDataIter == subcircuits.end()) {
			nextState.getGateIdRemapping().erase(iter.first);
			nextState.getGateIdReverseRemapping().erase(iter.first);
			nextState.removeGate(iter.first);
			continue;
		}
		const Circuit* circuit = circuitManager.getCircuit(subcircuitDataIter->second.circuitId);
		assert(circuit);
		circuit->getEvaluator().getEvaluatorInternal().removeEvaluator(*this);
		const EvalLayerState& evalLayerState = subcircuitDataIter->second.outputEvalLayer;
		for (const auto& pair : evalLayerState.getGates()) {
			eval_gate_id thisGateId = subcircuitDataIter->second.otherSimulatorToThisSimulatorIdMapping.at(pair.second.gateId);
			for (auto connectionsPair : pair.second.connections) {
				for (EvalConnectionPoint otherConnectionPoint : connectionsPair.second) {
					// need to add some logic to not double make connections
					if (
						(otherConnectionPoint.gateId > pair.second.gateId) ||
						(otherConnectionPoint.gateId == pair.second.gateId && otherConnectionPoint.connectionEndId.get() > connectionsPair.first.get())
					) {
						eval_gate_id otherGateId = subcircuitDataIter->second.otherSimulatorToThisSimulatorIdMapping.at(otherConnectionPoint.gateId);
						unsigned int weight = nextState.getConnectionWeight(EvalConnection(
							EvalConnectionPoint(thisGateId, connectionsPair.first),
							EvalConnectionPoint(otherGateId, otherConnectionPoint.connectionEndId)
						));
						nextState.removeConnection(EvalConnection(
							EvalConnectionPoint(thisGateId, connectionsPair.first),
							EvalConnectionPoint(otherGateId, otherConnectionPoint.connectionEndId)
						), weight);
					}
				}
			}
		}
		for (auto pair : evalLayerState.getGates()) {
			eval_gate_id thisGateId = subcircuitDataIter->second.otherSimulatorToThisSimulatorIdMapping.at(pair.second.gateId);
			nextState.removeGate(thisGateId);
			nextState.releaseUnusedEvalGateId(thisGateId);
		}
		for (const auto& pair : circuit->getEvaluator().getEvaluatorInternal().getPortToInternalPointMapping()) {
			if (!pair.second.connectionPoint) {
				bool suc = nextState.getConnectionPointRemappingToNothing().erase(EvalConnectionPoint(iter.first, pair.first));
				assert(suc);
				assert(!nextState.getConnectionPointRemapping().contains(EvalConnectionPoint(iter.first, pair.first)));
				continue;
			}
			auto connectionPointRemappingIter = nextState.getConnectionPointRemapping().find(EvalConnectionPoint(iter.first, pair.first));
			if (connectionPointRemappingIter == nextState.getConnectionPointRemapping().end()) continue;
			auto iterPair = nextState.getConnectionPointReverseRemapping().equal_range(connectionPointRemappingIter->second);
			for (auto reverseRemappingIter = iterPair.first; reverseRemappingIter != iterPair.second; reverseRemappingIter++) {
				if (reverseRemappingIter->second == EvalConnectionPoint(iter.first, pair.first)) {
					nextState.getConnectionPointReverseRemapping().erase(reverseRemappingIter);
					break;
				}
			}
			nextState.getConnectionPointRemapping().erase(connectionPointRemappingIter);
		}
		subcircuits.erase(subcircuitDataIter);
	}
	for (auto iter : currentState.getAddedGates()) {
		circuit_id_t circuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(getBlockType(iter.second));
		if (circuitId == 0) {
			nextState.getGateIdRemapping().try_emplace(iter.first, iter.first);
			nextState.getGateIdReverseRemapping().emplace(iter.first, iter.first);
			nextState.addGate(iter.first, iter.second);
			continue;
		}
		const Circuit* circuit = circuitManager.getCircuit(circuitId);
		assert(circuit);
		circuit->getEvaluator().getEvaluatorInternal().addEvaluator(*this);
		const EvaluatorInternal& evaluatorInternal = circuit->getEvaluator().getEvaluatorInternal();
		const EvalLayerState& evalLayerState = evaluatorInternal.getLayerRunner().getOutputLayerForOtherEvals();
		auto subcircuitsPair = subcircuits.try_emplace(iter.first, circuitId, evalLayerState);
		for (auto pair : evalLayerState.getGates()) {
			eval_gate_id gateId = nextState.getUnusedEvalGateId();
			auto pair1 = subcircuitsPair.first->second.otherSimulatorToThisSimulatorIdMapping.try_emplace(pair.second.gateId, gateId);
			assert(pair1.second);
			auto pair2 = subcircuitsPair.first->second.thisSimulatorIdMappingToOtherSimulator.try_emplace(gateId, pair.second.gateId);
			assert(pair2.second);
			nextState.addGate(gateId, pair.second.type);
		}
		for (auto pair : evalLayerState.getGates()) {
			eval_gate_id thisGateId = subcircuitsPair.first->second.otherSimulatorToThisSimulatorIdMapping.at(pair.second.gateId);
			for (const auto& connectionsPair : pair.second.connections) {
				for (EvalConnectionPoint otherConnectionPoint : connectionsPair.second) {
					// need to add some logic to not double make connections
					if (
						(otherConnectionPoint.gateId > pair.second.gateId) ||
						(otherConnectionPoint.gateId == pair.second.gateId && otherConnectionPoint.connectionEndId.get() > connectionsPair.first.get())
					) {
						eval_gate_id otherGateId = subcircuitsPair.first->second.otherSimulatorToThisSimulatorIdMapping.at(otherConnectionPoint.gateId);
						unsigned int weight = evalLayerState.getConnectionWeight(EvalConnection(
							EvalConnectionPoint(pair.first, connectionsPair.first),
							otherConnectionPoint
						));
						nextState.addConnection(EvalConnection(
							EvalConnectionPoint(thisGateId, connectionsPair.first),
							EvalConnectionPoint(otherGateId, otherConnectionPoint.connectionEndId)
						), weight);
					}
				}
			}
		}
		for (const auto& pair : evaluatorInternal.getPortToInternalPointMapping()) {
			if (pair.second.connectionPoint) {
				const EvalConnectionPoint& bottomConnectionPoint = evaluatorInternal.mapFromTopConnectionPointToBottomConnectionPointForOtherEvals(
					pair.second.connectionPoint.value()
				);
				eval_gate_id thisGateId = subcircuitsPair.first->second.otherSimulatorToThisSimulatorIdMapping.at(bottomConnectionPoint.gateId);
				EvalConnectionPoint connectionPoint(thisGateId, bottomConnectionPoint.connectionEndId);
				nextState.getConnectionPointRemapping().emplace(
					EvalConnectionPoint(iter.first, pair.first),
					connectionPoint
				);
				nextState.getConnectionPointReverseRemapping().emplace(
					connectionPoint,
					EvalConnectionPoint(iter.first, pair.first)
				);
			} else {
				auto emplaceResult = nextState.getConnectionPointRemappingToNothing().emplace(iter.first, pair.first);
				assert(emplaceResult.second);
			}
		}
	}
	for (auto iter : currentState.getAddedConnections()) {
		EvalConnection connection = iter.first;
		auto remappingAIter = nextState.getConnectionPointRemapping().find(connection.connectionPointA);
		if (remappingAIter != nextState.getConnectionPointRemapping().end()) {
			connection.connectionPointA = remappingAIter->second;
		} else {
			if (nextState.getConnectionPointRemappingToNothing().contains(connection.connectionPointA)) continue;
		}
		auto remappingBIter = nextState.getConnectionPointRemapping().find(connection.connectionPointB);
		if (remappingBIter != nextState.getConnectionPointRemapping().end()) {
			connection.connectionPointB = remappingBIter->second;
		} else {
			if (nextState.getConnectionPointRemappingToNothing().contains(connection.connectionPointB)) continue;
		}
		nextState.addConnection(connection, iter.second);
	}
	for (eval_gate_id gateId : currentState.getGateIdRemappingsUpdateds()) {
		assert(nextState.getGate(gateId));
		nextState.addGateIdRemappingsUpdated(gateId);
	}
	for (EvalConnectionPoint connectionPoint : currentState.getConnectionPointRemappingsUpdated()) {
		assert(nextState.getGate(connectionPoint.gateId));
		nextState.addConnectionPointRemappingsUpdated(connectionPoint);
	}
}

std::vector<std::pair<eval_gate_id, circuit_id_t>> SubcircuitEvalLayer::getSubcircuits() const {
	std::vector<std::pair<eval_gate_id, circuit_id_t>> subcircuitPairs;
	for (const auto& subcircuit : subcircuits) {
		subcircuitPairs.emplace_back(subcircuit.first, subcircuit.second.circuitId);
	}
	return subcircuitPairs;
}

EvalConnectionPoint SubcircuitEvalLayer::getMappedAddress(eval_gate_id gateId, const Address& address) const {
	auto subcircuitIter = subcircuits.find(gateId);
	if (subcircuitIter == subcircuits.end()) return EvalConnectionPoint::null();
	const Circuit* circuit = circuitManager.getCircuit(subcircuitIter->second.circuitId);
	assert(circuit);
	EvalConnectionPoint internalBottomPoint = circuit->getEvaluator().getEvaluatorInternal().mapFromAddressToBottomConnectionPointForOtherEvals(address);
	if (internalBottomPoint.isNull()) return EvalConnectionPoint::null();
	auto otherSimulatorToThisSimulatorIdMappingIter = subcircuitIter->second.otherSimulatorToThisSimulatorIdMapping.find(internalBottomPoint.gateId);
	assert(otherSimulatorToThisSimulatorIdMappingIter != subcircuitIter->second.otherSimulatorToThisSimulatorIdMapping.end());
	return EvalConnectionPoint(
		otherSimulatorToThisSimulatorIdMappingIter->second,
		internalBottomPoint.connectionEndId
	);
}
VecVecEvalConnectionPoint SubcircuitEvalLayer::getReversedMappedConnectionPointGroupsWithAddress(
	const VecVecEvalConnectionPoint& connectionPoints,
	eval_gate_id gateId,
	const Address& address
) const {
	auto subcircuitIter = subcircuits.find(gateId);
	if (subcircuitIter == subcircuits.end()) {
		if (address.size() == 0)
		return { };
	}
	VecVecEvalConnectionPoint subcircuitConnectionPoints;
	for (const VecEvalConnectionPoint& connectionPointVec : connectionPoints) {
		subcircuitConnectionPoints.push_back({});
		for (EvalConnectionPoint connectionPoint : connectionPointVec) {
			auto iter = subcircuitIter->second.thisSimulatorIdMappingToOtherSimulator.find(connectionPoint.gateId);
			if (iter == subcircuitIter->second.thisSimulatorIdMappingToOtherSimulator.end()) continue;
			subcircuitConnectionPoints.back().push_back(EvalConnectionPoint(iter->second, connectionPoint.connectionEndId));
		}
	}
	const Circuit* circuit = circuitManager.getCircuit(subcircuitIter->second.circuitId);
	assert(circuit);
	return circuit->getEvaluator().getEvaluatorInternal().mapFromBottomConnectionPointGroupsToTopConnectionPointsForOtherEvals(subcircuitConnectionPoints, address);
}

void SubcircuitEvalLayer::getReversedMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint, VecEvalConnectionPoint& evalConnectionPoints) const {
	auto connectionPointIterPair = nextState.getConnectionPointReverseRemapping().equal_range(evalConnectionPoint);
	for (auto iter = connectionPointIterPair.first; iter != connectionPointIterPair.second; iter++) {
		auto subcircuitsIter = subcircuits.find(iter->second.gateId);
		assert(subcircuitsIter != subcircuits.end());
		const Circuit* circuit = circuitManager.getCircuit(subcircuitsIter->second.circuitId);
		assert(circuit);
		auto portDataIter = circuit->getEvaluator().getEvaluatorInternal().getPortToInternalPointMapping().find(iter->second.connectionEndId);
		if (portDataIter->second.portType == BlockData::ConnectionData::PortType::OUTPUT) {
			evalConnectionPoints.push_back(iter->second);
			continue;
		}
		const EvalGate* gate = currentState.getGate(iter->second.gateId);
		assert(gate);
		auto connectionsContainerIter = gate->connections.find(iter->second.connectionEndId);
		if (connectionsContainerIter == gate->connections.end()) return;
		for (EvalConnectionPoint connectionPoint : connectionsContainerIter->second) {
			evalConnectionPoints.push_back(connectionPoint);
		}
	}
	auto evalGateIdIterPair = nextState.getGateIdReverseRemapping().equal_range(evalConnectionPoint.gateId);
	for (auto iter = evalGateIdIterPair.first; iter != evalGateIdIterPair.second; iter++) {
		evalConnectionPoints.emplace_back(iter->second, evalConnectionPoint.connectionEndId);
	}
	assert(nextState.getGate(evalConnectionPoint.gateId));
}

void SubcircuitEvalLayer::processICEdits(circuit_id_t circuitId, const std::vector<std::tuple<connection_end_id_t, EvalConnectionPoint, EvalConnectionPoint>>& updatedPortIds) {
	evaluator.startEdit();
	const Circuit* circuit = circuitManager.getCircuit(circuitId);
	assert(circuit);
	const EvaluatorInternal& evaluatorInternal = circuit->getEvaluator().getEvaluatorInternal();
	for (auto subcircuitsPair : subcircuits) {
		if (subcircuitsPair.second.circuitId == circuitId) {
			for (auto& [connectionEndId, preConnectionPoint, postConnectionPoint] : updatedPortIds) {
				if (preConnectionPoint.isNull()) {
					if (postConnectionPoint.isNull()) {
						nextState.getConnectionPointRemappingToNothing().emplace(subcircuitsPair.first, connectionEndId);
					}
					continue;
				}
				auto connectionPointRemappingIter = nextState.getConnectionPointRemapping().find(EvalConnectionPoint(subcircuitsPair.first, connectionEndId));
				assert(connectionPointRemappingIter != nextState.getConnectionPointRemapping().end());
				assert(!connectionPointRemappingIter->second.isNull());
				// remove connections
				const EvalGate* evalGate = nextState.getGate(connectionPointRemappingIter->second.gateId);
				assert(evalGate);
				auto connectionsIter = evalGate->connections.find(connectionPointRemappingIter->second.connectionEndId);
				if (connectionsIter != evalGate->connections.end()) {
					std::unordered_set<EvalConnectionPoint> otherConnectionPoints = connectionsIter->second;
					for (const EvalConnectionPoint& otherConnectionPoint : otherConnectionPoints) {
						if (!subcircuitsPair.second.thisSimulatorIdMappingToOtherSimulator.contains(otherConnectionPoint.gateId)) {
							unsigned int weight = nextState.getConnectionWeight(EvalConnection(
								EvalConnectionPoint(connectionPointRemappingIter->second.gateId, connectionPointRemappingIter->second.connectionEndId),
								otherConnectionPoint
							));
							nextState.removeConnection(EvalConnection(
								EvalConnectionPoint(connectionPointRemappingIter->second.gateId, connectionPointRemappingIter->second.connectionEndId),
								otherConnectionPoint
							), weight);
						}
					}
				}
				// update remapping
				auto iterPair = nextState.getConnectionPointReverseRemapping().equal_range(connectionPointRemappingIter->second);
				for (auto iter = iterPair.first; iter != iterPair.second; iter++) {
					if (iter->second == EvalConnectionPoint(subcircuitsPair.first, connectionEndId)) {
						nextState.getConnectionPointReverseRemapping().erase(iter);
						break;
					}
				}
				nextState.getConnectionPointRemapping().erase(connectionPointRemappingIter);
				if (postConnectionPoint.isNull()) {
					nextState.getConnectionPointRemappingToNothing().emplace(subcircuitsPair.first, connectionEndId);
				}
			}
			// internal updates
			for (auto iter : subcircuitsPair.second.outputEvalLayer.getRemovedConnections()) {
				eval_gate_id gateIdA = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.at(iter.first.connectionPointA.gateId);
				eval_gate_id gateIdB = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.at(iter.first.connectionPointB.gateId);
				nextState.removeConnection(EvalConnection(
					EvalConnectionPoint(gateIdA, iter.first.connectionPointA.connectionEndId),
					EvalConnectionPoint(gateIdB, iter.first.connectionPointB.connectionEndId)
				), iter.second);
			}
			for (auto iter : subcircuitsPair.second.outputEvalLayer.getRemovedGates()) {
				auto otherSimulatorToThisSimulatorIdMappingIter = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.find(iter.first);
				assert(otherSimulatorToThisSimulatorIdMappingIter != subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.end());
				bool suc = subcircuitsPair.second.thisSimulatorIdMappingToOtherSimulator.erase(otherSimulatorToThisSimulatorIdMappingIter->second);
				assert(suc);
				nextState.removeGate(otherSimulatorToThisSimulatorIdMappingIter->second);
				nextState.releaseUnusedEvalGateId(otherSimulatorToThisSimulatorIdMappingIter->second);
				subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.erase(otherSimulatorToThisSimulatorIdMappingIter);
			}
			for (auto iter : subcircuitsPair.second.outputEvalLayer.getAddedGates()) {
				eval_gate_id gateId = nextState.getUnusedEvalGateId();
				auto pair1 = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.try_emplace(iter.first, gateId);
				assert(pair1.second);
				auto pair2 = subcircuitsPair.second.thisSimulatorIdMappingToOtherSimulator.try_emplace(gateId, iter.first);
				assert(pair2.second);
				nextState.addGate(gateId, iter.second);
			}
			for (auto pair : subcircuitsPair.second.outputEvalLayer.getGates()) {
				assert(subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.contains(pair.first));
			}
			for (auto iter : subcircuitsPair.second.outputEvalLayer.getAddedConnections()) {
				eval_gate_id gateIdA = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.at(iter.first.connectionPointA.gateId);
				eval_gate_id gateIdB = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.at(iter.first.connectionPointB.gateId);
				nextState.addConnection(EvalConnection(
					EvalConnectionPoint(gateIdA, iter.first.connectionPointA.connectionEndId),
					EvalConnectionPoint(gateIdB, iter.first.connectionPointB.connectionEndId)
				), iter.second);
			}

			// post port updates
			for (auto [connectionEndId, preConnectionPoint, postConnectionPoint] : updatedPortIds) {
				if (postConnectionPoint.isNull()) { continue; }
				assert(evaluatorInternal.getPortToInternalPointMapping().contains(connectionEndId));
				const EvalConnectionPoint& bottomConnectionPoint = evaluatorInternal.mapFromTopConnectionPointToBottomConnectionPointForOtherEvals(postConnectionPoint);
				eval_gate_id thisGateId = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.at(bottomConnectionPoint.gateId);
				EvalConnectionPoint connectionPoint(thisGateId, bottomConnectionPoint.connectionEndId);
				auto emplaceResult = nextState.getConnectionPointRemapping().emplace(
					EvalConnectionPoint(subcircuitsPair.first, connectionEndId),
					connectionPoint
				);
				assert(emplaceResult.second);
				nextState.addConnectionPointRemappingsUpdated(connectionPoint);
				nextState.getConnectionPointReverseRemapping().emplace(
					connectionPoint,
					EvalConnectionPoint(subcircuitsPair.first, connectionEndId)
				);
				const EvalGate* evalGate = currentState.getGate(subcircuitsPair.first);
				auto connectionsIter = evalGate->connections.find(connectionEndId);
				if (connectionsIter != evalGate->connections.end()) {
					for (EvalConnectionPoint otherConnectionPoint : connectionsIter->second) {
						// auto subcircuitDataAIter = subcircuits.find(otherConnectionPoint.gateId);
						// if (subcircuitDataAIter != subcircuits.end()) {
						// 	const Circuit* circuit = circuitManager.getCircuit(subcircuitDataAIter->second.circuitId);
						// 	assert(circuit);
						// 	const EvaluatorInternal& evaluatorInternalval = circuit->getEvaluator().getEvaluatorInternal();
						// 	auto internalPointMappingIter = evaluatorInternalval.getPortToInternalPointMapping().find(otherConnectionPoint.connectionEndId);
						// 	assert(internalPointMappingIter != evaluatorInternalval.getPortToInternalPointMapping().end());
						// 	if (!internalPointMappingIter->second.connectionPoint.has_value()) continue;
						// 	EvalConnectionPoint internalBottomPoint = evaluatorInternalval.mapFromTopConnectionPointToBottomConnectionPointForOtherEvals(
						// 		internalPointMappingIter->second.connectionPoint.value()
						// 	);
						// 	auto otherSimulatorToThisSimulatorIdMappingIter = subcircuitDataAIter->second.otherSimulatorToThisSimulatorIdMapping.find(
						// 		internalBottomPoint.gateId
						// 	);
						// 	assert(otherSimulatorToThisSimulatorIdMappingIter != subcircuitDataAIter->second.otherSimulatorToThisSimulatorIdMapping.end());
						// 	otherConnectionPoint = EvalConnectionPoint(otherSimulatorToThisSimulatorIdMappingIter->second, internalBottomPoint.connectionEndId);
						// }
						EvalConnectionPoint otherConnectionPointMapped = otherConnectionPoint;
						auto remappingIter = nextState.getConnectionPointRemapping().find(otherConnectionPoint);
						if (remappingIter != nextState.getConnectionPointRemapping().end()) {
							otherConnectionPointMapped = remappingIter->second;
						} else {
							if (nextState.getConnectionPointRemappingToNothing().contains(otherConnectionPoint)) continue;
						}
						unsigned int weight = currentState.getConnectionWeight(EvalConnection(
							EvalConnectionPoint(subcircuitsPair.first, connectionEndId),
							otherConnectionPoint
						));
						assert(weight);
						nextState.addConnection(EvalConnection(
							connectionPoint,
							otherConnectionPointMapped
						), weight);
					}
				}
			}
			for (eval_gate_id gateId : subcircuitsPair.second.outputEvalLayer.getGateIdRemappingsUpdateds()) {
				auto iter = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.find(gateId);
				if (iter == subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.end()) {
					logError("could not find eval id while proagating id {} update from IC.", "SubcircuitEvalLayer::processICEdits", gateId);
					continue;
				}
				assert(nextState.getGate(iter->second));
				nextState.addGateIdRemappingsUpdated(iter->second);
			}
			for (EvalConnectionPoint connectionPoint : subcircuitsPair.second.outputEvalLayer.getConnectionPointRemappingsUpdated()) {
				auto iter = subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.find(connectionPoint.gateId);
				if (iter == subcircuitsPair.second.otherSimulatorToThisSimulatorIdMapping.end()) {
					logError("could not find eval id while proagating connectionPoint {} update from IC.", "SubcircuitEvalLayer::processICEdits", connectionPoint);
					continue;
				}
				assert(nextState.getGate(iter->second));
				nextState.addConnectionPointRemappingsUpdated(EvalConnectionPoint(iter->second, connectionPoint.connectionEndId));
			}
		}
	}
	evaluator.endEdit();
}
