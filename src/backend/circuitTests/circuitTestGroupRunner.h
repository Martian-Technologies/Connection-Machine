#ifndef circuitTestGroupRunner_h
#define circuitTestGroupRunner_h

#include "backend/circuit/circuitDefs.h"
#include "backend/container/block/blockDefs.h"
#include "backend/evaluator/evalDefs.h"
#include "backend/circuitTests/circuitTestGroup.h"

class Backend;
class EvalLogicSimulator;

class CircuitTestGroupRunner {
    typedef std::unordered_multimap<std::string, Position> NamePositionMap;
public:
    enum TestResult {
        NOT_RUN,
        SUCCEEDED,
        FAILED,
        ERROR
    };

    struct TestRunData {
        TestRunData(const TestResult& result = NOT_RUN, const std::string& message = "Not run") : result(result), message(message){}
        TestResult result = NOT_RUN;
        std::string message = "Not run";
    };

    CircuitTestGroupRunner(Backend& backend, std::string name, BlockType blockType);
    std::string getName() const {return name;}

    const CircuitTestGroup* getCircuitTestGroup();

    std::vector<TestRunData> runAllTests();
    TestRunData runTest(const std::string testName);
    TestRunData runTest(const int testIndex);
    simulator_id_t getSimulatorId() const {return simID;}
    circuit_id_t getCircuitId() const {return circuitID;}
private:
    bool generateTestCircuit();
    std::pair<TestResult, std::string> runSetStatesCommand(CircuitTestGroup::TestCommand testCommand, EvalLogicSimulator& simulator, NamePositionMap& nameToConnectedBlockPosition);
    std::pair<TestResult, std::string> runCheckStatesCommand(CircuitTestGroup::TestCommand testCommand, EvalLogicSimulator& simulator, NamePositionMap& nameToConnectedBlockPosition);

    std::string name;
    circuit_id_t circuitID = 0;
    simulator_id_t simID = 0;
    NamePositionMap namePositionMap;
    EvalLogicSimulator* simulator = nullptr;
	Backend& backend;
    BlockType blockType;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
};

#endif /* circuitTestGroupRunner_h */
