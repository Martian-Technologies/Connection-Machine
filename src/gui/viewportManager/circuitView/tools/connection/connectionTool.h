#ifndef connectionTool_h
#define connectionTool_h

#include "../circuitTool.h"

class ConnectionTool : public CircuitTool {
public:
	ConnectionTool();

	void activate() override final;

	static inline std::vector<std::string> getModes_() { return { "Single", "Tensor" }; }
	static inline std::string getPath_() { return "connection"; }
	inline std::vector<std::string> getModes() const override final { return getModes_(); }
	inline std::string getPath() const override final { return getPath_(); }
	void setMode(std::string toolMode) override final;

private:
	std::string mode = "None";
	SharedCircuitTool activeConnectionTool = nullptr;
};

typedef std::shared_ptr<ConnectionTool> SharedConnectionTool;

#endif /* connectionTool_h */
