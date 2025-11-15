#ifndef listEvaluatorsCommand_h
#define listEvaluatorsCommand_h

#include "../command.h"

class ListEvaluatorsCommand : public Command {
public:
	ListEvaluatorsCommand() : Command("list_evaluators") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Lists all open evaluators."; }
};

#endif /* listEvaluatorsCommand_h */
