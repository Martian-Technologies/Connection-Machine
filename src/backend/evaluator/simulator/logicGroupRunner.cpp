#include "logicGroupRunner.h"

logic_state_t LogicGroupRunner::getState(simulator_state_index_t simulatorStateIndex) const {
    return logic_state_t::UNDEFINED;
}

void LogicGroupRunner::setState(simulator_state_index_t simulatorStateIndex, logic_state_t state) {
    logError("setState not implemented", "LogicGroupRunner::setState");
}