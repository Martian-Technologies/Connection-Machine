#ifndef singleConnectTool_h
#define singleConnectTool_h

#include "../circuitToolHelper.h"

class SingleConnectTool : public CircuitToolHelper {
public:
	void activate() override final {
		CircuitTool::activate();
		registerFunction("Tool Primary Activate", std::bind(&SingleConnectTool::makeConnection, this, std::placeholders::_1));
		registerFunction("Tool Secondary Activate", std::bind(&SingleConnectTool::cancelConnection, this, std::placeholders::_1));
	}

	void updateElements() override final;

	inline void reset() override final { clicked = false; elementCreator.clear(); }
	bool makeConnection(const Event* event);
	bool cancelConnection(const Event* event);

private:
	bool clicked = false;
	Position clickPosition;
};

#endif /* singleConnectTool_h */
