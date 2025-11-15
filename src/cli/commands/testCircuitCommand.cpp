#include "testCircuitCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"
#include "backend/circuitTestCase/circuitTestCase.h"
#include "backend/container/block/blockDefs.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<TestCircuitCommand>());)

void TestCircuitCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 2) {
		logError("Wrong number of arguments passed to test_circuit.", "TestCircuitCommand");
		return;
	}

    CircuitTestCase testCase;
    if (!testCase.runTest(BlockType::TRISTATE_BUFFER, false, environment)) {
        logInfo("Test case failed.", "TestCircuitCommand");
    }
    else {
        logInfo("Test case succeeded.", "TestCircuitCommand");
    }
}
