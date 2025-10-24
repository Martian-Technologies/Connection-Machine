#include "getBlockStateCommand.h"

#include "backend/evaluator/evaluator.h"
#include "environment/environment.h"
#include "util/runAtStartup.h"
#include "../commandManager.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<GetBlockStateCommand>());)

void GetBlockStateCommand::run(const std::vector<std::string>& args, Environment& environment) {
	if (args.size() != 4) {
		logError("Wrong number of arguments passed to get_block_state. Proper usage is 'get_block_state {eval_id} {x_pos} {y_pos}'", "GetBlockStateCommand");
		return;
	}

    int evalID;
    int xPos;
    int yPos;
    try {
        evalID = std::stoi(args[1]);
        xPos = std::stoi(args[2]);
        yPos = std::stoi(args[3]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "GetBlockStateCommand");
        return;
    }
    SharedEvaluator eval = environment.getBackend().getEvaluatorManager().getEvaluator(evalID);
    if (eval == nullptr) {
        logError("Unrecognized evaluator ID. Available evaluators can be found with the 'list_evaluators' command.", "GetBlockStateCommand");
        return;
    }
    unsigned char state = (unsigned char)eval->getState(Address(Position(xPos, yPos)));
    logInfo("State of block in eval with ID {} at position ({}, {}) is {}", "GetBlockStateCommand", evalID, xPos, yPos, state);
}
