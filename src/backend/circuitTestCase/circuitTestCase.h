#ifndef circuitTestCase_h
#define circuitTestCase_h

#include "backend/evaluator/evalDefs.h"
#include "environment/environment.h"
#include "backend/evaluator/simulator/logicState.h"
#include <unordered_map>

// add upper level logic later
class CircuitTestCase {
    typedef std::unordered_multimap<std::string, Position> NamePositionMap;
public:
    CircuitTestCase() {}
    bool runTest(BlockType blockType, bool haltOnFailure, Environment& environment);

private:
    enum TestCommandType {
        NOP_COMMAND,
        SET_STATES,
        CHECK_STATES,
        TICK_STEP
    };

    std::string getTestCommandTypeString(TestCommandType type) {
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
        TestCommandType type = NOP_COMMAND;
        int ticks = 0;
        std::vector<std::pair<std::string, logic_state_t>> states = {};
    };

    void generateTestCircuit();
    void runSetStatesCommand(TestCommand testCommand, const SharedEvaluator eval, NamePositionMap& nameToConnectedBlockPosition);
    bool runCheckStatesCommand(TestCommand testCommand, const SharedEvaluator eval, NamePositionMap& nameToConnectedBlockPosition);

    std::unordered_map<std::string, std::vector<TestCommand>> testCommandGroups;
};

#endif