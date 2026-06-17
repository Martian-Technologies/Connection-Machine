#ifndef portAdder_h
#define portAdder_h

#include "../circuitTool.h"

class PortAdder : public CircuitTool {
public:
	using CircuitTool::CircuitTool;
	void activate() override final;
	static inline std::vector<std::string> getModes_() { return {}; }
	static inline std::string getPath_() { return "NONE/portAdder"; }
	inline std::string getPath() const override final { return getPath_(); }
	inline unsigned int getStackId() const override final { return 2; }
	inline bool showInMenu() const override final { return false; }
	inline bool canMakeEdits() const override final { return false; }

	typedef std::function<void(Position)> OnSelectFunction;
	inline void setup(bool isInput, OnSelectFunction function) {
		if (!getCircuit()) return;
		type = getCircuit()->getBlockType();
		this->isInput = isInput;
		onSelectFunction = function;
	}
	void updateElements() override final;

	inline void reset() override final { type = BlockType::NONE; elementCreator.clear(); setStatusBar(""); }
	bool press(const Event* event);

private:
	OnSelectFunction onSelectFunction;
	BlockType type = BlockType::NONE;
	bool isInput;
};

#endif /* portAdder_h */
