#ifndef getBlockStateCommand_h
#define getBlockStateCommand_h

#include "../command.h"

class GetBlockStateCommand : public Command {
public:
	GetBlockStateCommand() : Command("get_block_state") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Returns the state of a block in a certain evaluator at the specified position."; }
};

#endif /* getBlockStateCommand_h */