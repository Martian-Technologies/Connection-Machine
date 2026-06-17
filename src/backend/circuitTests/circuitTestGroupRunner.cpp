#include "circuitTestGroupRunner.h"

#include "backend/circuitTests/circuitTestGroup.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "backend/backend.h"

CircuitTestGroupRunner::CircuitTestGroupRunner(Backend& backend, std::string name, BlockType blockType) :
	name(name), backend(backend), blockType(blockType), dataUpdateEventReceiver(backend.getDataUpdateEventManager()) {
	generateTestCircuit();
	dataUpdateEventReceiver.linkFunction("blockDataUpdate", [this](const DataUpdateEventManager::EventData* event) {
		const DataUpdateEventManager::EventDataWithValue<BlockType>* data = event->cast<BlockType>();
		assert(data);
		if (this->blockType != data->get()) return;
		generateTestCircuit();
	});
}

const CircuitTestGroup* CircuitTestGroupRunner::getCircuitTestGroup() {
    return backend.getCircuitTestGroupManager().getCircuitTestGroup(name);
}

bool CircuitTestGroupRunner::generateTestCircuit() {
    if (blockType == BlockType::NONE) {
        circuitID = 0;
        simID = 0;
        return false;
    }
    const CircuitTestGroup* testGroupData = getCircuitTestGroup();
	if (testGroupData == nullptr) {
		circuitID = 0;
        simID = 0;
		return false;
	}

	if (circuitID == 0) circuitID = backend.getCircuitManager().createNewCircuit(false);
	if (simID == 0) simID = backend.createSimulator(circuitID).value();
	simulator = backend.getSimulatorManager().getSimulator(simID);
    simulator->setPause(true);
    Circuit* circuit = backend.getCircuitManager().getCircuit(circuitID);
    circuit->clear();
    circuit->setEditable(false);
    BlockData* circuitBlockData = backend.getBlockDataManager().getBlockData(circuit->getBlockType());
    assert(circuitBlockData);
    circuitBlockData->setIsPlaceable(false);

    const BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);

    // change implementation of this when BlockData::getConnectionNameToId is implemented
    namePositionMap.clear();

    if (!circuit->tryInsertBlock(Position(0,0), Orientation(), blockType)) {
        logError("Couldn't insert test circuit block '{}'", "circuitTestGroupRunner", blockType);
        return false;
    }

	const Block* testedBlock = circuit->getBlockContainer().getBlock(Position(0, 0));

    // iterate through the connections on the block type and create a switch/light to represent each input/output on a test circuit
    for (const auto& [connectionEndId, connectionData] : blockData->getConnections()) {
        if (connectionData.portType == BlockData::ConnectionData::PortType::INPUT || connectionData.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
            Position externalConnPos = Position(-1-namePositionMap.size(), 0);

            std::optional<std::string> blockNameOpt = blockData->getConnectionIdToName(connectionEndId);
            if (blockNameOpt == std::nullopt) {
                logError("Unable to resolve block name for id {}", "circuitTestGroupRunner", connectionEndId);
                return false;
            }
            std::string blockName = blockNameOpt.value();
            if (std::find(testGroupData->inputs.begin(), testGroupData->inputs.end(), blockName) == testGroupData->inputs.end()) {
                logWarning("Tested circuit's port '{}' does not match any inputs expected by test, this may cause errors", "circuitTestGroupRunner", blockName);
            }
            if (!circuit->tryInsertBlock(externalConnPos, Orientation(), SWITCH)) {
                logError("Couldn't insert switch test circuit block", "circuitTestGroupRunner");
                return false;
            }

            const Block* block = circuit->getBlockContainer().getBlock(externalConnPos);
            if (!circuit->tryCreateConnection(ConnectionEnd(testedBlock->id(), connectionEndId), ConnectionEnd(block->id(), 0))) {
                logError("Couldn't create switch test circuit connection, ext: {}", "circuitTestGroupRunner", externalConnPos);
                return false;
            }
            namePositionMap.insert({blockData->getConnectionIdToName(connectionEndId).value(), externalConnPos});
        }
        if (connectionData.portType == BlockData::ConnectionData::PortType::OUTPUT || connectionData.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
            Position externalConnPos = Position(-1-namePositionMap.size(), 0);

            std::optional<std::string> blockNameOpt = blockData->getConnectionIdToName(connectionEndId);
            if (blockNameOpt == std::nullopt) {
                logError("Unable to resolve block name for id {}", "circuitTestGroupRunner", connectionEndId);
                return false;
            }
            std::string blockName = blockNameOpt.value();
            if (std::find(testGroupData->outputs.begin(), testGroupData->outputs.end(), blockName) == testGroupData->outputs.end()) {
                logWarning("Tested circuit's port '{}' does not match any outputs expected by test, this may cause errors", "circuitTestGroupRunner", blockName);
            }
            if (!circuit->tryInsertBlock(externalConnPos, Orientation(), LIGHT)) {
                logError("Couldn't insert light test circuit block", "circuitTestGroupRunner");
                return false;
            }

            const Block* block = circuit->getBlockContainer().getBlock(externalConnPos);
            if (!circuit->tryCreateConnection(ConnectionEnd(testedBlock->id(), connectionEndId), ConnectionEnd(block->id(), 0))) {
                logError("Couldn't create light test circuit connection, ext: {}", "circuitTestGroupRunner", externalConnPos);
                return false;
            }
            namePositionMap.insert({blockData->getConnectionIdToName(connectionEndId).value(), externalConnPos});
        }
    }
    for (const auto& input : testGroupData->inputs) {
        if (namePositionMap.find(input) == namePositionMap.end()) {
            logWarning("Input '{}' expected by test not found on tested circuit, this may cause errors", "circuitTestGroupRunner", input);
        }
    }
    for (const auto& output : testGroupData->outputs) {
        if (namePositionMap.find(output) == namePositionMap.end()) {
            logWarning("Output '{}' expected by test not found on tested circuit, this may cause errors", "circuitTestGroupRunner", output);
        }
    }
    return true;
}

std::vector<CircuitTestGroupRunner::TestRunData> CircuitTestGroupRunner::runAllTests() {
    const CircuitTestGroup* testGroupData = getCircuitTestGroup();
    std::vector<TestRunData> testResults;
    for (int i =0; i < testGroupData->testCases.size(); i++) {
        testResults.push_back(runTest(i));
    }
    return testResults;
}

CircuitTestGroupRunner::TestRunData CircuitTestGroupRunner::runTest(const std::string testName) {
    const CircuitTestGroup* testGroupData = getCircuitTestGroup();
    const auto testIter = testGroupData->testCaseNameToID.find(testName);
    if (testIter == testGroupData->testCaseNameToID.end()) {
        logError("Unable to find test case with name '{}'", "circuitTestGroupRunner", testName);
        return TestRunData(CircuitTestGroupRunner::ERROR, "Unable to find test case with name " + testName);
    }
    return runTest(testIter->second);
}

CircuitTestGroupRunner::TestRunData CircuitTestGroupRunner::runTest(const int testIndex) {
    const CircuitTestGroup* testGroupData = getCircuitTestGroup();

    simulator->resetStates();
    simulator->setPause(true); //don't know if this is necessary but playing it safe
    std::pair<CircuitTestGroupRunner::TestResult, std::string> testCommandResult;
    CircuitTestGroupRunner::TestResult testGroupStatus = CircuitTestGroupRunner::SUCCEEDED;
    std::string testGroupMessage = "";
    logInfo("Running test case '{}'", "circuitTestGroupRunner", testGroupData->testCases[testIndex].name);
    for (auto commandIter = testGroupData->testCases[testIndex].testCommands.begin(); commandIter != testGroupData->testCases[testIndex].testCommands.end(); commandIter++) {
        logInfo("Performing a test command of type {}", "circuitTestGroupRunner", CircuitTestGroup::getTestCommandTypeString(commandIter->type));
        if (commandIter->type == CircuitTestGroup::NOP_COMMAND) {
            continue;
        } else if (commandIter->type == CircuitTestGroup::SET_STATES) {
            testCommandResult = runSetStatesCommand(*commandIter, *simulator, namePositionMap);
        } else if (commandIter->type == CircuitTestGroup::CHECK_STATES) {
            testCommandResult = runCheckStatesCommand(*commandIter, *simulator, namePositionMap);
        } else if (commandIter->type == CircuitTestGroup::TICK_STEP) {
            logInfo("Stepping forward by {} ticks", "circuitTestGroupRunner - TICK_STEP", commandIter->ticks);
            simulator->tickStep(commandIter->ticks);
        } else {
            logError("Unrecognized test command type", "circuitTestGroupRunner");
            return TestRunData(CircuitTestGroupRunner::TestResult::ERROR, "Unrecognized test command type");
        }

        if (testCommandResult.first == CircuitTestGroupRunner::ERROR) {
            logError("Test command error detected.", "circuitTestGroupRunner");
            return TestRunData(CircuitTestGroupRunner::TestResult::ERROR, testCommandResult.second);
        }
        if (testCommandResult.first == CircuitTestGroupRunner::FAILED) {
            testGroupStatus = CircuitTestGroupRunner::FAILED;
            if (testGroupMessage == "") testGroupMessage = testGroupMessage + testCommandResult.second;
            else testGroupMessage = testGroupMessage + "\n" + testCommandResult.second;
        }
    }
    if (testGroupStatus == TestResult::SUCCEEDED) testGroupMessage = "Test passed";
    return TestRunData(testGroupStatus, testGroupMessage);
}

std::pair<CircuitTestGroupRunner::TestResult, std::string> CircuitTestGroupRunner::runSetStatesCommand(CircuitTestGroup::TestCommand testCommand, EvalLogicSimulator& simulator, NamePositionMap& nameToConnectedBlockPosition) {
    for (auto statesIter = testCommand.states.begin(); statesIter != testCommand.states.end(); statesIter++) {
        auto blockPosIter = nameToConnectedBlockPosition.find(statesIter->first);
        if (blockPosIter == nameToConnectedBlockPosition.end()) {
            logError("Port '{}' does not match any on circuit", "circuitTestGroupRunner", statesIter->first);
            return std::make_pair(CircuitTestGroupRunner::ERROR, "Port " + statesIter->first + " does not match any on circuit");
        }
        simulator.setState((Address(blockPosIter->second)), statesIter->second);
        logInfo("Set port '{}' to state '{}'", "circuitTestGroupRunner - SET_STATES", statesIter->first, statesIter->second);
    }
    return std::make_pair(CircuitTestGroupRunner::SUCCEEDED, "");
}

std::pair<CircuitTestGroupRunner::TestResult, std::string> CircuitTestGroupRunner::runCheckStatesCommand(CircuitTestGroup::TestCommand testCommand, EvalLogicSimulator& simulator, NamePositionMap& nameToConnectedBlockPosition) {
// probably needs to return the state of every single block tested instead of whether just one of them fails
    TestResult testCommandStatus = CircuitTestGroupRunner::SUCCEEDED;
    std::string testCommandMessage = "";
    for (auto statesIter = testCommand.states.begin(); statesIter != testCommand.states.end(); statesIter++) {
        auto blockPosIter = nameToConnectedBlockPosition.find(statesIter->first);
        if (blockPosIter == nameToConnectedBlockPosition.end()) {
            logError("Port '{}' does not match any on circuit", "circuitTestGroupRunner", statesIter->first);
            return std::make_pair(CircuitTestGroupRunner::ERROR, "Port " + statesIter->first + " does not match any on circuit");
        }
        logic_state_t actualState = simulator.getState((Address(blockPosIter->second)));
        if (actualState != statesIter->second) {
            logError("Inner test case failed: Expected port '{}' to have output '{}', got '{}'", "circuitTestGroupRunner - CHECK_STATES", statesIter->first, statesIter->second, actualState);
            testCommandStatus = CircuitTestGroupRunner::FAILED;
            testCommandMessage = testCommandMessage + "Expected port " + statesIter->first + " to have state " + std::to_string((unsigned int)statesIter->second) + ", got " + std::to_string((unsigned int)actualState) + "\n";
        } else {
            logInfo("Inner test case succeeded: Expected port '{}' to have output '{}', got '{}'", "circuitTestGroupRunner - CHECK_STATES", statesIter->first, statesIter->second, actualState);
        }
    }
    return std::make_pair(testCommandStatus, testCommandMessage);
}
