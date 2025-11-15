#ifndef versionCommand_h
#define versionCommand_h

#include "../command.h"

class VersionCommand : public Command {
public:
	VersionCommand() : Command("version") {}

	void run(const std::vector<std::string>& args, Environment& environment) override final;
	const std::string getHelpString() const override final { return "Gets the app version."; }
};

#endif /* versionCommand_h */
