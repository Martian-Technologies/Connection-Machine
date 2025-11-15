/*#ifndef resetEvaluatorCommand_h
#define resetEvaluatorCommand_h

#include "../command.h"

class ResetEvaluatorCommand : public Command {
public:
	ResetEvaluatorCommand() : Command("reset_evaluator") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Resets the evaluator of specified ID."; }
};

#endif /* resetEvaluatorCommand_h */
