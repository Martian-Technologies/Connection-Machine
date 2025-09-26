#ifndef cliApp_h
#define cliApp_h

class CliApp {
public:
    CliApp() {
        std::string input;
        std::cout << "Enter command: " << std::flush;
        while (std::getline(std::cin, input)) {
            std::istringstream input_stream;
            input_stream.str(input);
            for (std::string arg; input_stream >> std::quoted(arg);)
                std::cout << arg << '\n';
            std::cout << "Enter command: " << std::flush;
        }
    }
};

#endif
/*
input parser, in this, we want a command object. the command will have a name. THe program will run in a terminal and
you will enter commands while it is running. command should be an interface
*/