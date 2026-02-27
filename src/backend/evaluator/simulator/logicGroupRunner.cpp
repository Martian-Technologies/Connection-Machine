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
        logInfo("Group ID: {}, Group: {}", "LogicGroupRunner::setGroups", groupId, simGroup.toString());
    }
}
