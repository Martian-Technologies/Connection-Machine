#include "setEvalPauseStateCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<SetEvalPauseStateCommand>());)

void SetEvalPauseStateCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 3) {
		logError("Wrong number of arguments passed to set_eval_pause_state. Proper usage is 'set_eval_pause_state {eval_id} {state}'", "SetEvalPauseStateCommand");
		return;
	}

    int evalID;
    int state;
    try {
        evalID = std::stoi(args[1]);
        state = std::stoi(args[2]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "SetEvalPauseStateCommand");
        return;
    }
    if (state < 0 || state > 1) {
        logError("Invalid state. Valid states are 0 for unpaused, 1 for paused.", "SetEvalPauseStateCommand");
        return;
    }
    SharedEvaluator eval = environment.getBackend().getEvaluatorManager().getEvaluator(evaluator_id_t(evalID));
    if (eval == nullptr) {
        logError("Unrecognized evaluator ID. Available evaluators can be found with the 'list_evaluators' command.", "SetEvalPauseStateCommand");
        return;
    }
    eval->setPause((bool)state);
    logInfo("Set pause state of eval {} to: {}", "SetEvalPauseStateCommand", evalID, eval->isPause());
}
