#include "listTestsCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<ListTestsCommand>());)

void ListTestsCommand::run(const std::vector<std::string>& args, Environment& environment) {
	for (auto iter = environment.getBackend().getCircuitTestGroupManager().begin(); iter != environment.getBackend().getCircuitTestGroupManager().end(); iter++) {
        logInfo("{}", "ListTestsCommand", iter->first);
    }
}