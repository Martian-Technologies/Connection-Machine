#include "cornerLog.h"

#include <RmlUi/Core.h>

#include "backend/settings/settings.h"

CornerLog::CornerLog(Rml::ElementDocument* document) : document(document) {
	cornerLogElement = document->GetElementById("corner-log");
	Settings::registerListener<SettingType::DECIMAL>("Corner Log/Message Timeout", [this](const double& timeout) { this->timeout = timeout; } );
	const double* timeout = Settings::get<SettingType::DECIMAL>("Corner Log/Message Timeout");
	if (timeout) this->timeout = *timeout;
}

void CornerLog::updateMessages() {
	if (messageCreationTime.empty()) return;
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	while (((std::chrono::duration<float>)(now - messageCreationTime.front())).count() >= timeout) {
		messageCreationTime.pop_front();
		Rml::Element* message = cornerLogElement->GetFirstChild();
		if (message) cornerLogElement->RemoveChild(message);
		if (messageCreationTime.empty()) return;
	}
}

void CornerLog::log(const std::string& message) {
	cornerLogElement->AppendChild(std::move(document->CreateElement("p")))->AppendChild(std::move(document->CreateTextNode(message)));
	messageCreationTime.push_back(std::chrono::system_clock::now());
}

void CornerLog::logError(const std::string& message) {
	Rml::Element* p = cornerLogElement->AppendChild(std::move(document->CreateElement("p")));
	p->SetClass("corner-log-error", true);
	p->AppendChild(std::move(document->CreateTextNode(message)));
	messageCreationTime.push_back(std::chrono::system_clock::now());
}
