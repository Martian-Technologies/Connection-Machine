#include "createEvaluatorCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<CreateEvaluatorCommand>());)

void CreateEvaluatorCommand::run(const std::vector<std::string>& args, Environment& environment) {
    if (args.size() != 2) {
  		logError("Wrong number of arguments passed to create_evaluator. Proper usage is 'create_evaluator {ID}'.", "CreateEvaluatorCommand");
	  	return;
    }

    circuit_id_t cirID;
    try {
        cirID = std::stoi(args[1]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "CreateEvaluatorCommand");
        return;
    }

    Backend& backend = environment.getBackend();
    if (backend.getCircuitManager().getCircuit(cirID) == nullptr) {
        logError("Unrecognized circuit ID. Available circuits can be found with the 'list_circuits' command.", "CreateEvaluatorCommand");
        return;
    }
    backend.createEvaluator(cirID);
}