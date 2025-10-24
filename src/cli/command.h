#ifndef command_h
#define command_h

class Environment;

class Command {
public:
	Command(std::string name) : name(std::move(name)) {}
    virtual ~Command() {};
    const std::string& getName() { return name; }
    virtual void run(const std::vector<std::string>& args, Environment& environment) = 0;
    virtual const std::string getHelpString() const { return "Generic help string"; }
private:
	std::string name;
};

#endif