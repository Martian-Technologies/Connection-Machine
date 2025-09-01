#ifndef eventRegister_h
#define eventRegister_h

#include "event.h"

typedef std::function<bool(const Event* event)> EventFunction;
typedef unsigned long long EventRegistrationSignature;

class EventRegister {
public:
	EventRegister() : sigCounter(0), allEventFunctions() { }

	typedef std::pair<EventRegistrationSignature, EventFunction> RegistrationPair;

	EventRegistrationSignature registerFunction(const std::string& eventName, EventFunction function) {
		auto sigCounter = getNewRegistrationSignature();
		allEventFunctions[eventName].push_back({ sigCounter, function });
		return sigCounter;
	}

	void unregisterFunction(const std::string& eventName, EventRegistrationSignature signature) {
		auto iter = allEventFunctions.find(eventName);
		if (iter == allEventFunctions.end()) return;
		auto funcIter = std::find_if(
			iter->second.begin(), iter->second.end(),
			[&signature](const RegistrationPair& i) { return i.first == signature; }
		);
		if (funcIter != iter->second.end()) {
			// keeps order
			iter->second.erase(funcIter);
		}
	}

	std::optional<EventFunction> getEventFunction(const std::string& eventName, EventRegistrationSignature signature) {
		auto iter = allEventFunctions.find(eventName);
		if (iter == allEventFunctions.end()) return std::nullopt;
		auto funcIter = std::find_if(
			iter->second.begin(), iter->second.end(),
			[&signature](const RegistrationPair& i) { return i.first == signature; }
		);
		if (funcIter != iter->second.end()) {
			return funcIter->second;
		}
		return std::nullopt;
	}

	// functions are called in the order that they are added to a event
	bool doEvent(const Event& event) {
		auto iter = allEventFunctions.find(event.getName());
		if (iter == allEventFunctions.end()) return false;
		bool used = false;
		std::vector<RegistrationPair> functions = iter->second; // allows registering during a event
		for (RegistrationPair& function : functions) {
			if (function.second(&event)) used = true;
		}
		return used;
	}
private:
	EventRegistrationSignature sigCounter;
	inline EventRegistrationSignature getNewRegistrationSignature() { return ++sigCounter; }

	std::map<std::string, std::vector<RegistrationPair>> allEventFunctions;
};

#endif /* eventRegister_h */
