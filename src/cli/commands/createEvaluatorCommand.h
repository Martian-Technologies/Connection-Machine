#ifndef createEvaluatorCommand_h
#define createEvaluatorCommand_h

#include "../command.h"

class CreateEvaluatorCommand : public Command {
public:
	CreateEvaluatorCommand() : Command("create_evaluator") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Creates an evaluator for the specified circuit."; }
};

#endif /* createEvaluatorCommand_h */