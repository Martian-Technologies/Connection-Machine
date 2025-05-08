#ifndef selectTool_h
#define selectTool_h

#include "../circuitTool.h"
#include "../selectionHelpers/selectionHelperTool.h"

class SelectionMakerTool : public CircuitTool {
public:
	void reset() override final;
	void activate() override final;

	static inline std::vector<std::string> getModes_() { return { "Area", "Tensor" }; }
	static inline std::string getPath_() { return "selection maker"; }
	inline std::vector<std::string> getModes() const override final { return getModes_(); }
	inline std::string getPath() const override final { return getPath_(); }
	void setMode(std::string toolMode) override final;

	void updateElements() override final;

	bool click(const Event* event);
	bool unclick(const Event* event);
	bool copy(const Event* event);

private:
	std::string mode = "None";
	SharedSelectionHelperTool activeSelectionHelper = nullptr;
};

#endif /* selectTool_h */
