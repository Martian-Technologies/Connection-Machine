#include "circuitTestGroupManager.h"
#include "backend/circuitTests/circuitTestGroup.h"

bool CircuitTestGroupManager::addCircuitTestGroup(CircuitTestGroup&& testGroup) {
    std::string name = testGroup.getName();
    bool ret = testGroups.emplace(name, std::move(testGroup)).second;
    dataUpdateEventManager.sendEvent("newTestGroup", name);
    return ret;
}
