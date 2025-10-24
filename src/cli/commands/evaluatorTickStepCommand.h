#ifndef evaluatorTickStepCommand_h
#define evaluatorTickStepCommand_h

#include "../command.h"

class EvaluatorTickStepCommand : public Command {
public:
	EvaluatorTickStepCommand() : Command("evaluator_tick_step") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Steps an evaluator forwards by a specified number of ticks, default of 1."; }
};

#endif /* evaluatorTickStepCommand_h */