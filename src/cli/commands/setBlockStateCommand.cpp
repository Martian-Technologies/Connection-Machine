#include "setBlockStateCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<SetBlockStateCommand>());)

void SetBlockStateCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 5) {
		logError("Wrong number of arguments passed to set_block_state. Proper usage is 'set_block_state {eval_id} {state} {x_pos} {y_pos}'", "SetBlockStateCommand");
		return;
	}

    int evalID;
    int state;
    int xPos;
    int yPos;
    try {
        evalID = std::stoi(args[1]);
        state = std::stoi(args[2]);
        xPos = std::stoi(args[3]);
        yPos = std::stoi(args[4]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "SetBlockStateCommand");
        return;
    }
    if (state < 0 || state > 3) {
        logError("Invalid state. Valid states are 0 for low, 1 for high, 2 for floating, and 3 for undefined.", "SetBlockStateCommand");
        return;
    }
    SharedEvaluator eval = environment.getBackend().getEvaluatorManager().getEvaluator(evaluator_id_t(evalID));
    if (eval == nullptr) {
        logError("Unrecognized evaluator ID. Available evaluators can be found with the 'list_evaluators' command.", "SetBlockStateCommand");
        return;
    }
    eval->setState(Address(Position(xPos, yPos)), (logic_state_t)state);
    logInfo("Set state of block belonging to eval with ID {} in position ({}, {}) to {}", "SetBlockStateCommand", evalID, xPos, yPos, state);
}
