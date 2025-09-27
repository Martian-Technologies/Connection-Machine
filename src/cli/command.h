#ifndef command_h
#define command_h

class Environment;

class Command {
public:
	Command(std::string name) : name(std::move(name)) {}
    virtual ~Command() {};
    virtual const std::string& getName() { return name; };
    virtual void run(const std::vector<std::string>& args, Environment& environment) = 0;
private:
	std::string name;
};

#endif