#ifndef aboutWidget_h
#define aboutWidget_h

#include "../widget.h"

class AboutWidget : public Widget {
public:
	AboutWidget(WidgetId widgetId, MainWindow& mainWindow);

private:
	void render() override final;
};

#endif /* aboutWidget_h */
