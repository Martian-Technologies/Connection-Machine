#include "circuitTestGroup.h"

#include "backend/backend.h"

CircuitTestGroup::CircuitTestGroupCopy CircuitTestGroup::getMinimalCopy() const {
    return CircuitTestGroupCopy(name, isTruthTable, truthTableTicks, testCases, inputs, outputs);
}

void CircuitTestGroup::sendTestGroupUpdate() {
    backend.getDataUpdateEventManager().sendEvent("testGroupUpdate", name);
}

bool CircuitTestGroup::addTestCase(std::string name, int id) {
    if (testCaseNameToID.contains(name)) {
        logError("Cannot add test case '{}' due to one with the same name already existing", "CircuitTestGroup", name);
        return false;
    }
    if (id <= -1) {
        testCases.emplace_back(name);
        testCaseNameToID.emplace(std::make_pair(name, testCases.size()-1));
        sendTestGroupUpdate();
        return true;
    }
    if (id >= testCases.size()) {
        logError("Attempted insertion at position {} in test case vector exceeds its bounds ({})", "CircuitTestGroup", id, testCases.size());
        return false;
    }
    else {
        testCases.insert(testCases.begin() + id, TestCase(name));
        for (int i=0; i<testCases.size(); i++) {
            std::string name = testCases[i].name;
            testCaseNameToID.emplace(std::make_pair(name, i));
        }
    }
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::removeTestCase(std::string name) {
    if (!testCaseNameToID.contains(name)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", name);
        return false;
    }
    int id = testCaseNameToID[name];
    return removeTestCase(id);
}

bool CircuitTestGroup::removeTestCase(int id) {
    if (testCases.size() <= id || id < 0) {
        logError("ID {} is out of the bounds of the test cases vector ({})", "CircuitTestGroup", id, testCases.size());
        return false;
    }
    testCases.erase(testCases.begin() + id);
    testCaseNameToID.clear();
    for (int i=0; i<testCases.size(); i++) {
        std::string name = testCases[i].name;
        testCaseNameToID.emplace(std::make_pair(name, i));
    }
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::swapTestCases(std::string testCase1, std::string testCase2){
    if (!testCaseNameToID.contains(testCase1)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCase1);
        return false;
    }
    if (!testCaseNameToID.contains(testCase2)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCase2);
        return false;
    }
    if (testCase1 == testCase2) return false;
    sendTestGroupUpdate();
    return swapTestCases(testCaseNameToID[testCase1], testCaseNameToID[testCase2]);
}

bool CircuitTestGroup::swapTestCases(int id1, int id2) {
    if (id1 >= testCases.size() || id1 < 0) {
        logError("Attempted swap at position {} in test case vector exceeds its bounds ({})", "CircuitTestGroup", id1, testCases.size());
        return false;
    }
    if (id2 >= testCases.size() || id2 < 0) {
        logError("Attempted swap at position {} in test case vector exceeds its bounds ({})", "CircuitTestGroup", id2, testCases.size());
        return false;
    }
    if (id1 == id2) return false;
    std::swap(testCases[id1], testCases[id2]);
    testCaseNameToID[testCases[id1].name] = id1;
    testCaseNameToID[testCases[id2].name] = id2;
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::renameTestCase(std::string oldName, std::string newName) {
    if (oldName == newName) return false;
    if (!testCaseNameToID.contains(oldName)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", oldName);
        return false;
    }
    sendTestGroupUpdate();
    return renameTestCase(testCaseNameToID[oldName], newName);
}

bool CircuitTestGroup::renameTestCase(int id, std::string newName) {
    if (testCases.size() <= id || id < 0) {
        logError("ID {} is out of the bounds of the test cases vector ({})", "CircuitTestGroup", id, testCases.size());
        return false;
    }
    if (testCaseNameToID.contains(newName)) {
        logError("Cannot rename test case to '{}' due to one of the same name already existing", "CircuitTestGroup", newName);
        return false;
    }
    std::string oldName = testCases[id].name;
    testCases[id].name = newName;
    testCaseNameToID.erase(testCaseNameToID.find(oldName));
    testCaseNameToID.emplace(std::make_pair(newName, id));
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::addSimpleTestCase(std::string name, std::vector<std::pair<std::string, logic_state_t>> inputStates, std::vector<std::pair<std::string, logic_state_t>> outputStates){
    if (!isTruthTable) {
        logWarning("Action not recommended on non-truth table, performing anyways", "CircuitTestGroup");
    }
    if (testCaseNameToID.contains(name)) {
        logError("Cannot add simple test case '{}' due to one with the same name already existing", "CircuitTestGroup", name);
        return false;
    }
    testCaseNameToID.emplace(std::make_pair(name, testCases.size()-1));
    std::vector<TestCommand> testCommands;
    testCommands.emplace_back(SET_STATES, 0, inputStates);
    testCommands.emplace_back(TICK_STEP, truthTableTicks);
    testCommands.emplace_back(CHECK_STATES, 0, outputStates);
    testCases.emplace_back(name, testCommands);
    sendTestGroupUpdate();
    return true;
}

const CircuitTestGroup::TestCase* CircuitTestGroup::getTestCase(int id) const {
    if (id >= testCases.size() || id < 0) return nullptr;
    return &testCases[id];
}

const CircuitTestGroup::TestCase* CircuitTestGroup::getTestCase(std::string name) const {
	auto iter = testCaseNameToID.find(name);
	if (iter == testCaseNameToID.end()) {
		logError("Unable to find test case with name '{}'", "CircuitTestGroup", name);
		return nullptr;
	}
    return &testCases[iter->second];
}

bool CircuitTestGroup::addSetStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id) {
    if (isTruthTable) {
        logError("Method addSetStatesCommand disallowed on truth table, use addSimpleTestCase", "CircuitTestGroup");
        return false;
    }
    auto idIter = testCaseNameToID.find(testCaseName);
    if (idIter == testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[idIter->second];
    if (id <= -1) {
        testCase->testCommands.emplace_back(SET_STATES, 0, states);
        sendTestGroupUpdate();
        return true;
    }
    if (id >= testCase->testCommands.size()) {
        logError("Attempted insertion at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id, testCaseName, testCase->testCommands.size());
        return false;
    }
    else {
        testCase->testCommands.insert(testCase->testCommands.begin() + id, TestCommand(SET_STATES, 0, states));
    }
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::addCheckStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id) {
    if (isTruthTable) {
        logError("Method addCheckStatesCommand disallowed on truth table, use addSimpleTestCase", "CircuitTestGroup");
        return false;
    }
    auto idIter = testCaseNameToID.find(testCaseName);
    if (idIter == testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[idIter->second];
    if (id <= -1) {
        testCase->testCommands.emplace_back(CHECK_STATES, 0, states);
        sendTestGroupUpdate();
        return true;
    }
    if (id >= testCase->testCommands.size()) {
        logError("Attempted insertion at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id, testCaseName, testCase->testCommands.size());
        return false;
    }
    else {
        testCase->testCommands.insert(testCase->testCommands.begin() + id, TestCommand(CHECK_STATES, 0, states));
    }
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::modifyStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id) {
    if (isTruthTable) {
        logError("Method modifyStatesCommand disallowed on truth table, use addSimpleTestCase", "CircuitTestGroup");
        return false;
    }
    auto idIter = testCaseNameToID.find(testCaseName);
    if (idIter == testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[idIter->second];
    if (id >= testCase->testCommands.size() || id < 0) {
        logError("Attempted modification at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id, testCaseName, testCase->testCommands.size());
        return false;
    }
    if (testCase->testCommands[id].type != CHECK_STATES || testCase->testCommands[id].type != SET_STATES) {
        logError("Test command at position {} in test command vector of test case '{}' is not a state-related command", "CircuitTestGroup", id, testCaseName);
        return false;
    }
    testCase->testCommands[id].states = states;
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::addTickStepCommand(std::string testCaseName, int ticks, int id) {
    if (isTruthTable) {
        logError("Method addTickStepCommand disallowed on truth table, use addSimpleTestCase", "CircuitTestGroup");
        return false;
    }
    auto idIter = testCaseNameToID.find(testCaseName);
    if (idIter == testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[idIter->second];
    if (id <= -1) {
        testCase->testCommands.emplace_back(TICK_STEP, ticks);
        sendTestGroupUpdate();
        return true;
    }
    if (id >= testCase->testCommands.size()) {
        logError("Attempted insertion at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id, testCaseName, testCase->testCommands.size());
        return false;
    }
    else {
        testCase->testCommands.insert(testCase->testCommands.begin() + id, TestCommand(TICK_STEP, 0, {}));
    }
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::modifyTickStepCommand(std::string testCaseName, int ticks, int id) {
    if (isTruthTable) {
        logError("Method modifyStatesCommand disallowed on truth table, use addSimpleTestCase", "CircuitTestGroup");
        return false;
    }
    auto idIter = testCaseNameToID.find(testCaseName);
    if (idIter == testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[idIter->second];
    if (id >= testCase->testCommands.size() || id < 0) {
        logError("Attempted modification at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id, testCaseName, testCase->testCommands.size());
        return false;
    }
    if (testCase->testCommands[id].type != TICK_STEP) {
        logError("Test command at position {} in test command vector of test case '{}' is not a state-related command", "CircuitTestGroup", id, testCaseName);
        return false;
    }
    testCase->testCommands[id].ticks = ticks;
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::removeTestCommand(std::string testCaseName, int id) {
    if (isTruthTable) {
        logError("Method removeTestCommand disallowed on truth table", "CircuitTestGroup");
        return false;
    }
    if (!testCaseNameToID.contains(testCaseName)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[testCaseNameToID[testCaseName]];
    if (id >= testCase->testCommands.size()) return false;
    testCase->testCommands.erase(testCase->testCommands.begin() + id);
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::swapTestCommands(std::string testCaseName, int id1, int id2){
    if (isTruthTable) {
        logError("Method swapTestCommands disallowed on truth table", "CircuitTestGroup");
        return false;
    }
    if (!testCaseNameToID.contains(testCaseName)) {
        logError("Unable to find test case with name '{}'", "CircuitTestGroup", testCaseName);
        return false;
    }
    TestCase* testCase = &testCases[testCaseNameToID[testCaseName]];
    if (id1 >= testCase->testCommands.size() || id1 < 0) {
        logError("Attempted swap at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id1, testCaseName, testCase->testCommands.size());
        return false;
    }
    if (id2 >= testCase->testCommands.size() || id2 < 0) {
        logError("Attempted swap at position {} in test command vector of test case '{}' exceeds its bounds ({})", "CircuitTestGroup", id2, testCaseName, testCase->testCommands.size());
        return false;
    }
    std::swap(testCase->testCommands[id1], testCase->testCommands[id2]);
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::addInput(std::string input) {
    for (auto it = inputs.begin(); it != inputs.end(); it++) {
        if (*it == input) {
            logError("Input '{}' already exists in this test group", "CircuitTestGroup", input);
            return false;
        }
    }
    inputs.push_back(input);
    sendTestGroupUpdate();
    return true;
}

bool CircuitTestGroup::addOutput(std::string output) {
    for (auto it = outputs.begin(); it != outputs.end(); it++) {
        if (*it == output) {
            logError("Output '{}' already exists in this test group", "CircuitTestGroup", output);
            return false;
        }
    }
    outputs.push_back(output);
    sendTestGroupUpdate();
    return true;
}
