/*#include "resetEvaluatorCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<ResetEvaluatorCommand>());)

void ResetEvaluatorCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 2) {
		logError("Wrong number of arguments passed to reset_evaluator. Proper usage is 'reset_evaluator {eval_id}'", "ResetEvaluatorCommand");
		return;
	}

    int evalID;
    try {
        evalID = std::stoi(args[1]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "ResetEvaluatorCommand");
        return;
    }
    SharedEvaluator eval = environment.getBackend().getEvaluatorManager().getEvaluator(evalID);
    if (eval == nullptr) {
        logError("Unrecognized evaluator ID. Available evaluators can be found with the 'list_evaluators' command.", "ResetEvaluatorCommand");
        return;
    }
    eval->reset();
    logInfo("Reset evaluator of ID {}", "ResetEvaluatorCommand", evalID);
}
*/