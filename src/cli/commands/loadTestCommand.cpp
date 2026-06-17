#include "loadTestCommand.h"

#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"
#include "backend/circuitTests/circuitTestGroup.h"
#include "computerAPI/circuitTestFileLoader.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<LoadTestCommand>());)

void LoadTestCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 2) {
		logError("Wrong number of arguments passed to load_test. Proper usage is 'load_test {test_file_path}'", "LoadTestCommand");
		return;
	}

    CircuitTestGroupManager& testGroupManager = environment.getBackend().getCircuitTestGroupManager();
    std::optional<CircuitTestGroup> testGroup = CircuitTestFileLoader::getCircuitTestGroupFromFilePath(args[1], environment.getBackend());
    if (testGroup == std::nullopt) {
        logInfo("No tests loaded", "LoadTestCommand");
        return;
    }

    if (testGroupManager.addCircuitTestGroup(std::move(testGroup.value()))) {
        logInfo("Loaded test", "LoadTestCommand");
    } else {
        logError("Unable to load test", "LoadTestCommand");
    }
}
