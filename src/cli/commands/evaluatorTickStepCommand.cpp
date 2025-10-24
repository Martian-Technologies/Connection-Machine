#include "evaluatorTickStepCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<EvaluatorTickStepCommand>());)

void EvaluatorTickStepCommand::run(const std::vector<std::string>& args, Environment& environment) {
    if (args.size() != 2 && args.size() != 3) {
		logError("Wrong number of arguments passed to reset_evaluator. Proper usage is 'evaluator_tick_step {eval_id} {num_ticks}' or 'evaluator_tick_step {eval_id}'", "EvaluatorTickStepCommand");
		return;
    }

    int evalID;
    int ticks = 1;
    try {
        evalID = std::stoi(args[1]);
        if (args.size() == 3) {
            ticks = std::stoi(args[2]);
        }
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "EvaluatorTickStepCommand");
        return;
    }
    SharedEvaluator eval = environment.getBackend().getEvaluatorManager().getEvaluator(evalID);
    if (eval == nullptr) {
        logError("Unrecognized evaluator ID. Available evaluators can be found with the 'list_evaluators' command.", "EvaluatorTickStepCommand");
        return;
    }
    eval->tickStep(ticks);
    logInfo("Stepped evaluator with ID of {} forward by {} tick(s)", "EvaluatorTickStepCommand", evalID, ticks);
}
