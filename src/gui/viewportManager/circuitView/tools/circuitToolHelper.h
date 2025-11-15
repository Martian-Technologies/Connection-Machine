#ifndef circuitToolHelper_h
#define circuitToolHelper_h

#include "circuitTool.h"

class ToolStackInterface;

class CircuitToolHelper : public CircuitTool {
public:
	CircuitToolHelper(const Environment& environment) : CircuitTool(environment) { helper = true; }
	inline void restart() { reset(); elementCreator.clear(); }
};

#endif /* circuitToolHelper_h */
