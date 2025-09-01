#ifndef keybindHandler_h
#define keybindHandler_h

#include <RmlUi/Core.h>

#include "backend/settings/keybind.h"

class KeybindHandler : public Rml::EventListener {
public:
	typedef std::function<void()> ListenerFunction;
	void addListener(const std::string& keybindSettingPath, ListenerFunction listenerFunction);
	void addListener(Rml::Input::KeyIdentifier key, unsigned int modifier, ListenerFunction listenerFunction);
	void addListener(Rml::Input::KeyIdentifier key, ListenerFunction listenerFunction);
	void ProcessEvent(Rml::Event& event) override;
private:
	unsigned int bindId = 0;
	unsigned int getBindId() { return ++bindId; }
	std::unordered_multimap<Keybind, std::pair<unsigned int, ListenerFunction>> listenerFunctions;
};

#endif /* keybindHandler_h */