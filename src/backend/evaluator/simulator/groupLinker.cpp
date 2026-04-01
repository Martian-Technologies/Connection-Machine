#include "groupLinker.h"
#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

std::unordered_map<gate_group_id_t, LinkedGateGroup> GroupLinker::linkGroups(const std::unordered_map<gate_group_id_t, CompiledGateGroup>& simGroups) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	gateIdToGroupId.clear();
	for (const auto& [groupId, simGroup] : simGroups) {
		for (const SimulatorGate& gate : simGroup.gates) {
			assert(!gateIdToGroupId.contains(gate.id) && "Gate belongs to multiple compiled groups");
			gateIdToGroupId[gate.id] = groupId;
		}
	}

	// print out the mapping for debugging
	// for (const auto& [gateId, groupId] : gateIdToGroupId) {
	// 	logInfo("Gate {} is in group {}", "GroupLinker::linkGroups", gateId, groupId);
	// }

	std::unordered_map<gate_group_id_t, LinkedGateGroup> linkedGroups;
	std::unordered_set<EvalConnectionPoint> allConnectionPointsToBePushed;

	for (const auto& [groupId, simGroup] : simGroups) {
		linkedGroups[groupId] = LinkedGateGroup(simGroup.gates, {}, {});
		for (const EvalConnectionPoint& connectionPoint : simGroup.pullConnectionPoints) {
			allConnectionPointsToBePushed.insert(connectionPoint);
		}
	}

	std::unordered_map<EvalConnectionPoint, std::pair<gate_group_id_t, unsigned int>> pushConnectionPointToGroupIdAndIndex;
	std::unordered_map<gate_group_id_t, unsigned int> groupIdToNewPullConnectionPointIndexProvider;
	for (const auto& connectionPoint : allConnectionPointsToBePushed) {
		eval_gate_id gateId = connectionPoint.gateId;
		assert(gateIdToGroupId.contains(gateId) && "Pull connection references a gate that is not in any compiled group");
		gate_group_id_t groupId = gateIdToGroupId.at(gateId);
		unsigned int newIndex = groupIdToNewPullConnectionPointIndexProvider[groupId]++;
		pushConnectionPointToGroupIdAndIndex[connectionPoint] = { groupId, newIndex };
		linkedGroups.at(groupId).pushConnectionPoints.push_back(connectionPoint);
	}

	for (const auto& [groupId, simGroup] : simGroups) {
		for (const EvalConnectionPoint& connectionPoint : simGroup.pullConnectionPoints) {
			std::pair<gate_group_id_t, unsigned int> groupIdAndIndex = pushConnectionPointToGroupIdAndIndex.at(connectionPoint);
			linkedGroups.at(groupId).pullConnectionPoints[connectionPoint] = groupIdAndIndex;
		}
	}

	return linkedGroups;
}
