#ifndef circuitTestGroup_h
#define circuitTestGroup_h

#include "backend/evaluator/simulator/logicState.h"
#include "backend/position/position.h"

class Backend;
class EvalLogicSimulator;

class CircuitTestGroup {
    typedef std::unordered_multimap<std::string, Position> NamePositionMap;
    friend class CircuitTestGroupRunner;
public:
    enum TestCommandType {
        NOP_COMMAND,
        SET_STATES,
        CHECK_STATES,
        TICK_STEP
    };

    static std::string getTestCommandTypeString(TestCommandType type) {
        switch (type) {
            case 0:
                return "NOP_COMMAND";
            case 1:
                return "SET_STATES";
            case 2:
                return "CHECK_STATES";
            case 3:
                return "TICK_STEP";
            default:
                return "UNKNOWN";
       }
    }

    struct TestCommand {
        TestCommand(const TestCommandType& type = NOP_COMMAND, const int& ticks = 0, const std::vector<std::pair<std::string, logic_state_t>>& states = {}) : type(type), ticks(ticks), states(states) {}
        TestCommandType type = NOP_COMMAND;
        int ticks = 0;
        std::vector<std::pair<std::string, logic_state_t>> states = {};
    };

    struct TestCase {
        TestCase(const std::string& name, const std::vector<TestCommand>& testCommands = {}) : name(name), testCommands(testCommands) {}
        std::string name;
        std::vector<TestCommand> testCommands;
    };

    struct CircuitTestGroupCopy {
        std::string name;
        bool isTruthTable;
        int truthTableTicks;
        std::vector<TestCase> testCases;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
    };

    CircuitTestGroup(Backend& backend, std::string name, bool isTruthTable, int truthTableTicks) :
		name(name), isTruthTable(isTruthTable), truthTableTicks(truthTableTicks), backend(backend) {}
    std::string getName() const {return name;}
    CircuitTestGroupCopy getMinimalCopy() const;
    int getTruthTableTicks() const {return truthTableTicks;}
    bool truthTable() const {return isTruthTable;}

    bool addTestCase(std::string name, int id=-1);
    bool removeTestCase(std::string name);
    bool removeTestCase(int id);
    bool swapTestCases(std::string testCase1, std::string testCase2);
    bool swapTestCases(int id1, int id2);
    bool renameTestCase(std::string oldName, std::string newName);
    bool renameTestCase(int id, std::string newName);
    bool addSimpleTestCase(std::string name, std::vector<std::pair<std::string, logic_state_t>> inputStates, std::vector<std::pair<std::string, logic_state_t>> outputStates);
    const TestCase* getTestCase(int id) const;
    const TestCase* getTestCase(std::string name) const;

    bool addSetStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id=-1);
    bool addCheckStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id=-1);
    bool modifyStatesCommand(std::string testCaseName, std::vector<std::pair<std::string, logic_state_t>> states, int id);
    bool addTickStepCommand(std::string testCaseName, int ticks, int id=-1);
    bool modifyTickStepCommand(std::string testCaseName, int ticks, int id);
    bool removeTestCommand(std::string testCaseName, int id);
    bool swapTestCommands(std::string testCaseName, int id1, int id2);

    bool addInput(std::string input);
    bool addOutput(std::string output);
    std::vector<std::string>::const_iterator getInputIterator() const {return inputs.cbegin();}
    std::vector<std::string>::const_iterator getOutputIterator() const {return outputs.cbegin();}
    std::vector<std::string>::const_iterator getInputIteratorEnd() const {return inputs.cend();}
    std::vector<std::string>::const_iterator getOutputIteratorEnd() const {return outputs.cend();}
private:
    void sendTestGroupUpdate();
    std::string name;
    // truth tables follow a strict format of every test case having a set state, a tick step (universal across all test cases,
    // stored in the truthTableTicks value), and a get state. it only allows adding commands via the addSimpleTestCase method.
    bool isTruthTable;
    int truthTableTicks; // setting this to -1 should make the timing automatic
    std::unordered_map<std::string, int> testCaseNameToID;
    std::vector<TestCase> testCases;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
	Backend& backend;
};

#endif /* circuitTestGroup_h */
