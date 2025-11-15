#ifndef getBlockDataCommand_h
#define getBlockDataCommand_h

#include "../command.h"

class GetBlockDataCommand : public Command {
public:
	GetBlockDataCommand() : Command("get_block_data") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Returns the data of a block at the specified position."; }
};

#endif /* getBlockDataCommand_h */