#ifndef setEvalPauseStateCommand_h
#define setEvalPauseStateCommand_h

#include "../command.h"

class SetEvalPauseStateCommand : public Command {
public:
	SetEvalPauseStateCommand() : Command("set_eval_pause_state") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Pauses or unpauses an evaluator."; }
};

#endif /* setEvalPauseStateCommand_h */
