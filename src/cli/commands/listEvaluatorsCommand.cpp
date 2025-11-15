#include "listEvaluatorsCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<ListEvaluatorsCommand>());)

void ListEvaluatorsCommand::run(const std::vector<std::string>& args, Environment& environment) {
//    std::string evaluatorDetails = "";
	for (auto iter = environment.getBackend().getEvaluatorManager().begin(); iter != environment.getBackend().getEvaluatorManager().end(); iter++) {
//        evaluatorDetails = evaluatorDetails + iter->second->getEvaluatorName() + ", Paused: " + + "\n";
        logInfo("{}, Paused: {}", "ListEvaluatorsCommand", iter->second->getEvaluatorName(), iter->second->isPause());
    }
//    std::cout << evaluatorDetails << std::flush;
}