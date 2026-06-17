#ifndef callCreatePopup_h
#define callCreatePopup_h

class MainWindow;

void callCreatePopup(MainWindow& mainWindow, const std::string& message, const std::vector<std::pair<std::string, std::function<void ()>>>& buttons);

#endif /* callCreatePopup_h */
