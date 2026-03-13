#include "logicGroupRunner.h"
#include "gateGroup.h"
#include "simBlockData.h"
#include "util/algorithm.h"

using namespace SimBlockData;

logic_state_t LogicGroupRunner::getState(simulator_state_reference simulatorStateIndex) const {
	return logic_state_t::UNDEFINED;
}

void LogicGroupRunner::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	logError("setState not implemented", "LogicGroupRunner::setState");
}

void LogicGroupRunner::setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups) {
	for (const auto& [groupId, simGroup] : simGroups) {
		setGroup(groupId, simGroup);
	}
	// delete groups that got deleted
	for (gate_group_id_t groupId : range(gate_group_id_t(0), gate_group_id_t(runnableGroups.size()))) {
		if (runnableGroups[groupId.get()].isEmpty()) {
			continue;
		}
		if (simGroups.find(groupId) == simGroups.end()) {
			runnableGroups[groupId.get()] = RunnableGateGroup();
			groupsCache[groupId.get()] = LinkedGateGroup();
		}
	}
}

void LogicGroupRunner::setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup) {
	if (groupId.get() < groupsCache.size()) {
		if (groupsCache[groupId.get()] == simGroup) {
			return;
		}
	}
	while (groupsCache.size() <= groupId.get()) {
		groupsCache.emplace_back();
	}
	groupsCache[groupId.get()] = simGroup;

	while (runnableGroups.size() <= groupId.get()) {
		runnableGroups.emplace_back();
	}
	runnableGroups[groupId.get()] = RunnableGateGroup(simGroup, groupId);
}

namespace {
	bool isJunction(BlockType blockType) {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_X;
	}
}

namespace {

	template <class T>
	class Indexer {
	public:
		unsigned int getIndex(const T& value) {
			auto it = valueToIndex.find(value);
			if (it != valueToIndex.end()) {
				return it->second;
			}
			unsigned int index = nextIndex++;
			valueToIndex[value] = index;
			return index;
		}
		unsigned int size() const {
			return valueToIndex.size();
		}
	private:
		std::unordered_map<T, unsigned int> valueToIndex;
		unsigned int nextIndex = 0;
	};

}

RunnableGateGroup::RunnableGateGroup(const LinkedGateGroup& linkedGateGroup, gate_group_id_t groupId) {
	empty = false;
	std::unordered_map<EvalConnectionPoint, unsigned int> pulledConnectionPointToDataFieldIndex;
	std::unordered_map<gate_group_id_t, std::vector<std::pair<unsigned int, EvalConnectionPoint>>> pullIndicesToPullFromGroups;
	for (const auto& [connectionPoint, groupIdAndIndex] : linkedGateGroup.pullConnectionPointsByGroup) {
		pullIndicesToPullFromGroups[groupIdAndIndex.first].push_back({ groupIdAndIndex.second, connectionPoint });
	}
	pullDataBytecode.push_back(0); // num groups
	Indexer<EvalConnectionPoint> dataFieldAllocator;
	for (const auto& [groupId, pullIndicesAndConnectionPoints] : pullIndicesToPullFromGroups) {
		pullDataBytecode.push_back(groupId.get()); // group id
		pullDataBytecode.push_back(pullIndicesAndConnectionPoints.size()); // num connection points to pull from in this group
		for (const auto& [pullIndex, connectionPoint] : pullIndicesAndConnectionPoints) {
			pullDataBytecode.push_back(pullIndex); // pull index within the group
			dataFieldAllocator.getIndex(connectionPoint); // always +1, so we don't need to store the index for the pull phase
		}
		pullDataBytecode[0]++;
	}

	for (EvalConnectionPoint pushConnectionPoint : linkedGateGroup.pushConnectionPoints) {
		publishedStateDataFieldIndices.push_back(dataFieldAllocator.getIndex(pushConnectionPoint));
	}

	std::unordered_map<eval_gate_id, SimulatorGate> gates;
	for (const SimulatorGate& gate : linkedGateGroup.gates) {
		gates[gate.id] = gate;
	}

	calculateGatesBytecode.push_back(0); // num gates, will be filled in later
	for (const SimulatorGate& gate : linkedGateGroup.gates) { // calculate junctions
		if (!isJunction(gate.type)) {
			continue;
		}
		bool hasOutput = false;
		connection_end_id_t connectionEndId = connection_end_id_t(0);
		for (const auto& [connectionPoint, weight] : gate.getConnectionsFromPort(connectionEndId)) {
			InputOutput direction = gate.getDirection(connectionEndId, connectionPoint);
			if (direction == InputOutput::OUTPUT) {
				hasOutput = true;
				break;
			}
		}
		if (!hasOutput) {
			// no calculation needed for junctions without outputs during tick, instead they are computed on fetch
			continue;
		}
		calculateGatesBytecode[0]++;
		calculateGatesBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
		unsigned int numInputsIndex = calculateGatesBytecode.size();
		calculateGatesBytecode.push_back(0); // num inputs, will be filled in later
		for (const auto& [connectionPoint, weight] : gate.getConnectionsFromPort(connectionEndId)) {
			InputOutput direction = gate.getDirection(connectionEndId, connectionPoint);
			if (direction != InputOutput::INPUT) {
				continue;
			}
			calculateGatesBytecode[numInputsIndex]++;
			calculateGatesBytecode.push_back(dataFieldAllocator.getIndex(connectionPoint));
		}
		calculateGatesBytecode.push_back(dataFieldAllocator.getIndex(EvalConnectionPoint{ gate.id, connection_end_id_t(0) }));
	}

	logInfo("Group ID: {}", "RunnableGateGroup::RunnableGateGroup", groupId);
	logInfo("dataField size: {}", "RunnableGateGroup::RunnableGateGroup", dataFieldAllocator.size());
	logInfo("pullBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(pullDataBytecode));
	logInfo("calculateBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(calculateGatesBytecode));
	logInfo("publishedStateDataFieldIndices: {}", "RunnableGateGroup::RunnableGateGroup", to_string(publishedStateDataFieldIndices));

	dataField.resize(dataFieldAllocator.size());
}
