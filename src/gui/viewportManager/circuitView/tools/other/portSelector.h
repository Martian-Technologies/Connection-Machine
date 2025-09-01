#ifndef portSelector_h
#define portSelector_h

#include "../circuitTool.h"

class PortSelector : public CircuitTool {
public:
	void activate() override final;
	static inline std::vector<std::string> getModes_() { return {}; }
	static inline std::string getPath_() { return "NONE"; }
	inline std::string getPath() const override final { return getPath_(); }
	inline unsigned int getStackId() const override final { return 2; }
	inline bool showInMenu() const override final { return false; }

	typedef std::function<void(Position)> OnSelectFunction;
	inline void setPort(connection_end_id_t endId, OnSelectFunction function) { if (!circuit) return; type = circuit->getBlockType(); this->endId = endId; onSelectFunction = function; }

	inline void reset() override final { type = BlockType::NONE; elementCreator.clear(); setStatusBar(""); }
	bool press(const Event* event);

private:
	OnSelectFunction onSelectFunction;
	BlockType type = BlockType::NONE;
	connection_end_id_t endId;
};

#endif /* portSelector_h */
