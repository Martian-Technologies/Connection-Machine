#ifndef setBlockStateCommand_h
#define setBlockStateCommand_h

#include "../command.h"

class SetBlockStateCommand : public Command {
public:
	SetBlockStateCommand() : Command("set_block_state") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Sets the state of a block contained by a specific evaluator ID."; }
};

#endif /* setBlockStateCommand_h */
