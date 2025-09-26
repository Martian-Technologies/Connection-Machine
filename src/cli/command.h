#ifndef command_h
#define command_h

class Command {
public:
    virtual ~Command() = 0;
    virtual const std::string& getName() = 0;
    virtual void run(std::vector<std::string> commands) = 0;
};

#endif