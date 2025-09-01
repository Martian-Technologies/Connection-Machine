#ifndef modeChangerTool_h
#define modeChangerTool_h

#include "../circuitTool.h"
#include "../selectionHelpers/selectionHelperTool.h"

class ModeChangerTool : public CircuitTool {
public:
	void reset() override final;
	void activate() override final;

	inline std::vector<std::string> getModes() { return { }; }
	static inline std::vector<std::string> getModes_() { return { "Area", "Tensor" }; }
	static inline std::string getPath_() { return "mode changer"; }
	inline std::vector<std::string> getModes() const override final { return getModes_(); }
	inline std::string getPath() const override final { return getPath_(); }
	void setMode(std::string toolMode) override final;

	void updateElements() override final;

	bool click(const Event* event);
	bool unclick(const Event* event);
	bool changeModeUp(const Event* event);
	bool changeModeDown(const Event* event);

private:
	unsigned int type = 0;
	std::string mode = "None";
	SharedSelectionHelperTool activeSelectionHelper = nullptr;
};

#endif /* modeChangerTool_h */
