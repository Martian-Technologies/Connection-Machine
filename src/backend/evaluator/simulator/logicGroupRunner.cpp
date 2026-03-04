#include "logicGroupRunner.h"
#include "gateGroup.h"

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
	runnableGroups[groupId.get()] = RunnableGateGroup(simGroup);
}

namespace {
	bool isJunction(BlockType blockType) {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_X;
	}
}

RunnableGateGroup::RunnableGateGroup(const LinkedGateGroup& linkedGateGroup) {
	empty = false;
	std::unordered_map<EvalConnectionPoint, unsigned int> pullConnectionPointToPullIndex;
	std::unordered_map<gate_group_id_t, std::vector<std::pair<unsigned int, EvalConnectionPoint>>> pullIndicesToPullFromGroups;
	for (const auto& [connectionPoint, groupIdAndIndex] : linkedGateGroup.pullConnectionPointsByGroup) {
		pullIndicesToPullFromGroups[groupIdAndIndex.first].push_back({ groupIdAndIndex.second, connectionPoint });
	}
	pullDataBytecode.push_back(0); // num groups
	unsigned int pullIndexCounter = 0;
	for (const auto& [groupId, pullIndicesAndConnectionPoints] : pullIndicesToPullFromGroups) {
		pullDataBytecode.push_back(groupId.get()); // group id
		pullDataBytecode.push_back(pullIndicesAndConnectionPoints.size()); // num connection points to pull from in this group
		for (const auto& [pullIndex, connectionPoint] : pullIndicesAndConnectionPoints) {
			pullDataBytecode.push_back(pullIndex); // pull index within the group
			pullConnectionPointToPullIndex[connectionPoint] = pullIndexCounter++;
		}
		pullDataBytecode[0]++;
	}

	std::unordered_map<eval_gate_id, SimulatorGate> gates;
	for (const SimulatorGate& gate : linkedGateGroup.gates) {
		gates[gate.id] = gate;
	}

	calculateGatesBytecode.push_back(linkedGateGroup.gates.size()); // num gates
	for (const SimulatorGate& gate : linkedGateGroup.gates) { // calculate junctions
		if (!isJunction(gate.type)) {
			continue;
		}
		calculateGatesBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
		
	}
}
