#ifndef groupLinker_h
#define groupLinker_h

#include "logicGroupRunner.h"
#include "simulatorDefs.h"
#include "../evalDefs.h"

#include "gateGroup.h"

class GroupLinker {
public:
	GroupLinker() = default;

	std::unordered_map<gate_group_id_t, LinkedGateGroup> linkGroups(const std::unordered_map<gate_group_id_t, CompiledGateGroup>& simGroups);

private:
	std::unordered_map<gate_group_id_t, CompiledGateGroup> simGroupsCopy;
	std::unordered_map<eval_gate_id, gate_group_id_t> gateIdToGroupId;
};

#endif /* groupLinker_h */