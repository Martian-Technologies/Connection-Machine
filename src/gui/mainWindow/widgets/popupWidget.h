#ifndef popupWidget_h
#define popupWidget_h

#include "../widget.h"

class PopupWidget : public Widget {
public:
	PopupWidget(WidgetId widgetId, MainWindow& mainWindow, std::string message, const std::vector<std::pair<std::string, std::function<void()>>>& buttons);

private:
	void render() override final;

	std::string message;
	std::vector<std::pair<std::string, std::function<void()>>> buttons;
	int open = 4;
};

#endif /* popupWidget_h */
