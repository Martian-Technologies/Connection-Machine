#ifndef treeTraversal_h
#define treeTraversal_h

#include "../circuitTool.h"

class TreeTraversal : public CircuitTool {
public:
	using CircuitTool::CircuitTool;
	void activate() override final;

	static inline std::vector<std::string> getModes_() { return {}; }
	static inline std::string getPath_() { return "NONE/TreeTraversal"; }
	inline std::string getPath() const override final { return getPath_(); }
	inline unsigned int getStackId() const override final { return 2; }
	inline bool showInMenu() const override final { return false; }
	inline bool canMakeEdits() const override final { return false; }

	void updateElements() override final;

	inline void reset() override final { elementCreator.clear(); setStatusBar(""); }
	bool primary(const Event* event);
	bool secondary(const Event* event);
};

#endif /* treeTraversal_h */
