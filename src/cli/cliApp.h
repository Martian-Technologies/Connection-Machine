#ifndef cliApp_h
#define cliApp_h

#include "commandManager.h"
#include "environment/environment.h"
#include "util/multiDelimiterQuoted.h"

class CliApp {
public:
    CliApp() {
        std::string input;
		std::cout << "Connection Machine CLI" << std::endl;
        std::cout << "> " << std::flush;
        while (std::getline(std::cin, input)) {
            std::istringstream input_stream;
            input_stream.str(input);
			std::vector<std::string> args;
            for (std::string arg; input_stream >> MultiDelimiterQuoted(arg, {'"', '\''});)
                args.emplace_back(std::move(arg));
			CommandManager::get().run(args, environment);
			std::cout << "> " << std::flush;
        }
    }

private:
	Environment environment;
};

#endif
/*
input parser, in this, we want a command object. the command will have a name. THe program will run in a terminal and
you will enter commands while it is running. command should be an interface
*/