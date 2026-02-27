#include "groupLinker.h"

std::unordered_map<gate_group_id_t, LinkedGateGroup> GroupLinker::linkGroups(const std::unordered_map<gate_group_id_t, CompiledGateGroup>& simGroups) {
	// remove groups that got deleted (before remapping, since gates may have moved)
	for (auto it = simGroupsCopy.begin(); it != simGroupsCopy.end(); ) {
		if (simGroups.find(it->first) == simGroups.end()) {
			for (const SimulatorGate& gate : it->second.gates) {
				gateIdToGroupId.erase(gate.id);
			}
			it = simGroupsCopy.erase(it);
		} else {
			++it;
		}
	}

	// compare the copy with the new simGroups to update gateidToGroupId only from the groups that changed
	for (const auto& [groupId, simGroup] : simGroups) {
		auto it = simGroupsCopy.find(groupId);
		if (it != simGroupsCopy.end()) {
			const CompiledGateGroup& oldSimGroup = it->second;
			if (oldSimGroup == simGroup) {
				continue; // group didn't change, skip
			}
		}
		for (const SimulatorGate& gate : simGroup.gates) {
			gateIdToGroupId[gate.id] = groupId;
		}
		simGroupsCopy[groupId] = simGroup;
	}

	// print out the mapping for debugging
	for (const auto& [gateId, groupId] : gateIdToGroupId) {
		logInfo("Gate {} is in group {}", "GroupLinker::linkGroups", gateId, groupId);
	}

	std::unordered_map<gate_group_id_t, LinkedGateGroup> linkedGroups;
	return linkedGroups;
}