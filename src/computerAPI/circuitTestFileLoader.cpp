#include "./circuitTestFileLoader.h"
#include "backend/circuitTests/circuitTestGroup.h"
#include "backend/evaluator/simulator/logicState.h"
#include "logging/logging.h"
#include "computerAPI/circuits/textParser.h"

std::optional<CircuitTestGroup> CircuitTestFileLoader::getCircuitTestGroupFromTruthTableFilePath(std::ifstream &inputFile, Backend& backend) {
    logInfo("Parsing truth table file (.tst)", "CircuitTestFileLoader");

    std::string token;
    inputFile >> token;
    unsigned int version;
    const unsigned int latestVersion = 0;
    if (token == "version_0") {
        version = 0;
    } else {
        logError("Invalid truth table file version: {}", "CircuitTestFileLoader", token);
        return std::nullopt;
    }

    std::string testName;
    inputFile >> token;
    if (token == "Test:") {
        inputFile >> std::quoted(testName);
    } else {
        logError("Invalid file format, expected 'Test:' on line 2", "CircuitTestFileLoader");
        return std::nullopt;
    }
    inputFile >> std::ws;

    int ticks = 10; // change to -1 later to indicate automatic waiting
    inputFile >> token;
    if (token == "Ticks:") {
        std::string ticksStr;
        inputFile >> ticksStr;
        try {
            ticks = std::stoi(ticksStr);
        } catch (...) {
            logError("Invalid tick count, it should be a reasonably sized integer", "CircuitTestFileLoader");
            return std::nullopt;
        }
        inputFile >> std::ws;
        inputFile >> token;
    }

    CircuitTestGroup testGroup(backend, testName, true, ticks);

    std::string line;
    std::getline(inputFile, line);
    std::stringstream lineStream;
    lineStream << line;

    std::vector<std::string> inputs;
    while (token != "||") {
        lineStream >> std::ws;
        std::string portName;
        lineStream >> portName;
        if (!testGroup.addInput(portName)) {
            logError("Duplicated input port {}", "CircuitTestFileLoader", portName);
            return std::nullopt;
        }
        inputs.push_back(portName);
        logInfo("New input name {}", "CircuitTestFileLoader", portName);
        lineStream >> std::ws;
        lineStream >> token;
    }

    std::vector<std::string> outputs;
    while (true) {
        lineStream >> std::ws;
        if (lineStream.eof()) break;
        std::string portName;
        lineStream >> portName;
        if (!testGroup.addOutput(portName)) {
            logError("Duplicated output port {}", "CircuitTestFileLoader", portName);
            return std::nullopt;
        }
        outputs.push_back(portName);
        logInfo("New output name {}", "CircuitTestFileLoader", portName);
        lineStream >> std::ws;
        lineStream >> token;
    }

    std::getline(inputFile, line); // ignore line below the input/output names

    while (!inputFile.eof()) {
        std::getline(inputFile, line);
        if (line[0] != '|') break;
        std::stringstream lineStream;
        lineStream << line;
        std::string nameStr = "";
        lineStream >> token;

        //TODO: maybe add a special "?" state to denote that the state could be anything
        int inputCount = 0;
        std::vector<std::pair<std::string, logic_state_t>> inputStates = {};
        while (token != "||") {
            lineStream >> std::ws;
            std::string portStateStr;
            lineStream >> portStateStr;
            std::optional<logic_state_t> portState = stringToLogicState(portStateStr);
            if (portState == std::nullopt) {
                logError("Error parsing test file: Unrecognized port state '{}'", "CircuitTestFileLoader", portStateStr);
                return std::nullopt;
            }
            inputStates.push_back(std::make_pair(inputs[inputCount], portState.value()));
            nameStr += portStateStr;
            lineStream >> std::ws;
            lineStream >> token;
            inputCount++;
        }

        int outputCount = 0;
        std::vector<std::pair<std::string, logic_state_t>> outputStates = {};
        while (true) {
            lineStream >> std::ws;
            if (lineStream.eof()) break;
            std::string portStateStr;
            lineStream >> portStateStr;
            std::optional<logic_state_t> portState = stringToLogicState(portStateStr);
            if (portState == std::nullopt) {
                logError("Error parsing test file: Unrecognized port state '{}'", "CircuitTestFileLoader", portStateStr);
                return std::nullopt;
            }
            outputStates.push_back(std::make_pair(outputs[outputCount], portState.value()));
            nameStr += portStateStr;
            lineStream >> std::ws;
            lineStream >> token;
            outputCount++;
        }
        testGroup.addSimpleTestCase(nameStr, inputStates, outputStates);
    }

    return testGroup;
}

std::optional<CircuitTestGroup> CircuitTestFileLoader::getCircuitTestGroupFromFilePath(const std::string &path, Backend& backend) {
    if (path.substr(path.size()-4) != ".tst") {
        logError("Unrecognized file extension, expected .tst", "CircuitTestFileLoader");
        return std::nullopt;
    }

	std::ifstream inputFile(path, std::ios::in | std::ios::binary);
	if (!inputFile.is_open()) {
		logError("Couldn't open file at path: " + path, "CircuitTestFileLoader");
		return std::nullopt;
	}

    std::string token;
    inputFile >> token;
    if (token == "tt") {
        inputFile >> std::ws;
        return getCircuitTestGroupFromTruthTableFilePath(inputFile, backend);
    }
    else if (token != "tst") {
        logError("Invalid file format, expected 'tt' or 'tst' on line 1", "CircuitTestFileLoader");
        return std::nullopt;
    }
    logInfo("Parsing test file (.tst)", "CircuitTestFileLoader");
    inputFile >> std::ws;

    inputFile >> token;
    unsigned int version;
    const unsigned int latestVersion = 0;
    if (token == "version_0") {
        version = 0;
    } else {
        logError("Invalid test case file version: {}", "CircuitTestFileLoader", token);
        return std::nullopt;
    }

    std::string testName;
    inputFile >> token;
    if (token == "Test:") {
        inputFile >> std::quoted(testName);
    } else {
        logError("Invalid file format, expected 'Test:' on line 3", "CircuitTestFileLoader");
        return std::nullopt;
    }

    CircuitTestGroup testGroup(backend, testName, false, 0);

    std::set<std::string> inputs;
    inputFile >> token;
    if (token == "Inputs:") {
        inputFile >> std::ws;
        while(inputFile.peek() == '"') {
            std::string portName;
            inputFile >> std::quoted(portName);
            testGroup.addInput(portName);
            inputs.insert(portName);
            logInfo("New input name {}", "CircuitTestFileLoader", portName);
            inputFile >> std::ws;
        }
    } else {
        logError("Invalid file format, expected 'Inputs:' on line 4", "CircuitTestFileLoader");
        return std::nullopt;
    }

    std::set<std::string> outputs;
    inputFile >> token;
    if (token == "Outputs:") {
        inputFile >> std::ws;
        while(inputFile.peek() == '"') {
            std::string portName;
            inputFile >> std::quoted(portName);
            testGroup.addOutput(portName);
            outputs.insert(portName);
            logInfo("New output name {}", "CircuitTestFileLoader", portName);
            inputFile >> std::ws;
        }
    } else {
        logError("Invalid file format, expected 'Outputs:' on line 5", "CircuitTestFileLoader");
        return std::nullopt;
    }

    inputFile >> token;
    if (token == "Cases:") {
        inputFile >> std::ws;
    } else {
        logError("Invalid file format, expected 'Cases:' on line 6", "CircuitTestFileLoader");
        return std::nullopt;
    }

    inputFile >> std::ws;
    std::string groupName = "";
    while (inputFile.peek() == '"' && !inputFile.eof()) {
        inputFile >> std::ws;
        inputFile >> std::quoted(groupName);
        testGroup.addTestCase(groupName);
        logInfo("New group {}", "CircuitTestFileLoader", groupName);
        inputFile >> std::ws;
        char curly;
        inputFile >> curly;
        if (curly != '{') {
            logError("Error parsing test file: unable to find curly brace after group name", "circuitTestFileLoader");
            return std::nullopt;
        }
        inputFile >> std::ws;
        // adjust logic here to better handle grouping maybe
        while (inputFile.peek() != '}' && !inputFile.eof()) {
            inputFile >> token;
            if (token[0] == '>') {
                if (token.substr(1) == "Set:" || token.substr(1) == "Check:") {
                    std::vector<std::pair<std::string, logic_state_t>> states = {};
                    inputFile >> std::ws;
                    while (inputFile.peek() != '>' && inputFile.peek() != '}' && !inputFile.eof()) {
                        std::string portName;
                        inputFile >> std::quoted(portName);
                        if (token.substr(1) == "Set:" && !inputs.contains(portName)) {
                            logError("Error parsing test file: unrecognized input port name '{}', be sure to include it at the top of the file", "CircuitTestFileLoader", portName);
                            return std::nullopt;
                        }
                        if (token.substr(1) == "Check:" && !outputs.contains(portName)) {
                            logError("Error parsing test file: unrecognized input port name '{}', be sure to include it at the top of the file", "CircuitTestFileLoader", portName);
                            return std::nullopt;
                        }
                        inputFile >> std::ws;
                        char colon;
                        inputFile >> colon;
                        std::string portStateStr;
                        inputFile >> portStateStr;
                        std::optional<logic_state_t> portState = stringToLogicState(portStateStr);
                        if (portState == std::nullopt) {
                            logError("Error parsing test file: Unrecognized port state '{}'", "CircuitTestFileLoader", portStateStr);
                            return std::nullopt;
                        }
                        inputFile >> std::ws;
                        states.push_back(std::make_pair(portName, portState.value()));
                    }
                    if (token.substr(1) == "Set:") {
                        testGroup.addSetStatesCommand(groupName, states);
                    } else {
                        testGroup.addCheckStatesCommand(groupName, states);
                    }
                } else if (token.substr(1) == "Step") {
                    int ticks;
                    inputFile >> ticks;
                    testGroup.addTickStepCommand(groupName, ticks);
                } else {
                    logError("Error parsing test file: unknown command '{}'", "CircuitTestFileLoader", token.substr(1));
                    return std::nullopt;
                }
            } else {
                logError("Error parsing test file: looking for '>', did not find", "CircuitTestFileLoader");
                return std::nullopt;
            }
        }
        inputFile >> curly;
        inputFile >> std::ws;
    }

    logInfo("Loaded test", "CircuitTestFileLoader");
    return testGroup;
}

bool CircuitTestFileLoader::saveToTruthTableFile(const std::string& path, CircuitTestGroup& testGroup) {
    if (!testGroup.truthTable()) {
        logError("Can't save non-truth table test as a truth table", "CircuitTestFileLoader");
        return false;
    }
    if (path.substr(path.size()-4) != ".tst") {
        logError("Unrecognized file extension, expected .tst", "CircuitTestFileLoader");
        return false;
    }
	std::ofstream outputFile(path);
	if (!outputFile.is_open()) {
		logError("Couldn't open file at path: {}", "CircuitTestFileLoader", path);
		return false;
	}

    outputFile << "tt\n";
    outputFile << "version_0\n";
    outputFile << "Test: \"" << testGroup.getName() << "\"\n";
    if (testGroup.getTruthTableTicks() > -1) {
        outputFile << "Ticks: " << testGroup.getTruthTableTicks() << '\n';
    }

    outputFile << '|';
    std::vector<std::string> inputs;
    for (auto it = testGroup.getInputIterator(); it != testGroup.getInputIteratorEnd(); it++) {
        inputs.push_back(*it);
        outputFile << ' ' << *it << ' ' << '|';
    }
    outputFile << '|';
    std::vector<std::string> outputs;
    for (auto it = testGroup.getOutputIterator(); it != testGroup.getOutputIteratorEnd(); it++) {
        outputs.push_back(*it);
        outputFile << ' ' << *it << ' ' << '|';
    }
    outputFile << '\n' << '|';
    for (int i = 0; i < inputs.size(); i++){
        std::string str;
        str.assign(inputs[i].size() + 2, '-');
        outputFile << str << '|';
    }
    outputFile << '|';
    for (int i = 0; i < outputs.size(); i++){
        std::string str;
        str.assign(outputs[i].size() + 2, '-');
        outputFile << str << '|';
    }

    int testID = 0;
    const CircuitTestGroup::TestCase* testCase = testGroup.getTestCase(testID);
    while (testCase != nullptr) {
        outputFile << '\n' << '|';
        for (int i = 0; i < inputs.size(); i++) {
            std::string input = inputs[i];
            for (int j = 0; j < testCase->testCommands[0].states.size(); j++) {
                std::pair<std::string, logic_state_t> statesPair = testCase->testCommands[0].states[j];
                if (statesPair.first != input) continue;
                std::string str;
                str.assign(input.size(), ' ');
                outputFile << ' ' << (unsigned int)statesPair.second << str << '|';
            }
        }
        outputFile << '|';
        for (int i = 0; i < outputs.size(); i++) {
            std::string output = outputs[i];
            for (int j = 0; j < testCase->testCommands[2].states.size(); j++) {
                std::pair<std::string, logic_state_t> statesPair = testCase->testCommands[2].states[j];
                if (statesPair.first != output) continue;
                std::string str;
                str.assign(output.size(), ' ');
                outputFile << ' ' << (unsigned int)statesPair.second << str << '|';
            }
        }
        testID++;
        testCase = testGroup.getTestCase(testID);
    }
    return true;
}

bool CircuitTestFileLoader::saveToFile(const std::string& path, CircuitTestGroup& testGroup){
    if (path.substr(path.size()-4) != ".tst") {
        logError("Unrecognized file extension, expected .tst", "CircuitTestFileLoader");
        return false;
    }

	std::ofstream outputFile(path);
	if (!outputFile.is_open()) {
		logError("Couldn't open file at path: {}", "CircuitTestFileLoader", path);
		return false;
	}

    outputFile << "tst\n";
    outputFile << "version_0\n";
    outputFile << "Test: \"" << testGroup.getName() << "\"\n";

    outputFile << "Inputs: ";
    for (auto it = testGroup.getInputIterator(); it != testGroup.getInputIteratorEnd(); it++) {
        outputFile << "\"" << *it << "\" ";
    }

    outputFile << "\nOutputs: ";
    for (auto it = testGroup.getOutputIterator(); it != testGroup.getOutputIteratorEnd(); it++) {
        outputFile << "\"" << *it << "\"";
    }

    outputFile << "\nCases:\n";

    int i = 0;
    const CircuitTestGroup::TestCase* testCase = testGroup.getTestCase(i);
    while (testCase != nullptr) {
        outputFile << "\"" << testCase->name << "\" {\n";
        for (int j=0; j<testCase->testCommands.size(); j++) {
            const CircuitTestGroup::TestCommand testCommand = testCase->testCommands[j];
            if (testCommand.type == CircuitTestGroup::TestCommandType::NOP_COMMAND) {
                logInfo(">Command NOP", "CircuitTestFileLoader");
            } else if (testCommand.type == CircuitTestGroup::TestCommandType::SET_STATES) {
                logInfo(">Command SET", "CircuitTestFileLoader");
                outputFile << "\t>Set:\n";
                for (int x=0; x<testCommand.states.size(); x++) {
                    outputFile << "\t\t\"" << testCommand.states[x].first << "\":" << (unsigned int)testCommand.states[x].second << "\n";
                    logInfo(">>Port {}: {}", "CircuitTestFileLoader", testCommand.states[x].first, (unsigned int)testCommand.states[x].second);
                }
            } else if (testCommand.type == CircuitTestGroup::TestCommandType::CHECK_STATES) {
                logInfo(">Command CHECK", "CircuitTestFileLoader");
                outputFile << "\t>Check:\n";
                for (int x=0; x<testCommand.states.size(); x++) {
                    outputFile << "\t\t\"" << testCommand.states[x].first << "\":" << (unsigned int)testCommand.states[x].second << "\n";
                    logInfo(">>Port {}: {}", "CircuitTestFileLoader", testCommand.states[x].first, (unsigned int)testCommand.states[x].second);
                }
            } else if (testCommand.type == CircuitTestGroup::TestCommandType::TICK_STEP) {
                logInfo(">Command STEP {}", "CircuitTestFileLoader", testCommand.ticks);
                outputFile << "\t>Step " << testCommand.ticks << "\n";
            } else {
                logInfo(">Command UNKNOWN");
            }
        }
        outputFile << "}\n";
        i++;
        testCase = testGroup.getTestCase(i);
    }

    return true;
}
