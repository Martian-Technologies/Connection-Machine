#include "dataUpdateEventManager.h"

DataUpdateEventManager::DataUpdateEventReceiver::DataUpdateEventReceiver(DataUpdateEventManager& eventManager) : eventManager(&eventManager) {
	eventManager.dataUpdateEventReceivers.emplace(this);
}

DataUpdateEventManager::DataUpdateEventReceiver::DataUpdateEventReceiver(DataUpdateEventReceiver&& other) : eventManager(other.eventManager), functions(std::move(other.functions)) {
	if (eventManager) {
		eventManager->dataUpdateEventReceivers.erase(&other);
		other.eventManager = nullptr;
		eventManager->dataUpdateEventReceivers.emplace(this);
	}
}

DataUpdateEventManager::DataUpdateEventReceiver::~DataUpdateEventReceiver() {
	if (eventManager) eventManager->dataUpdateEventReceivers.erase(this);
}

DataUpdateEventManager::~DataUpdateEventManager() {
	for (DataUpdateEventReceiver* dataUpdateEventReceiver : dataUpdateEventReceivers) {
		dataUpdateEventReceiver->eventManager = nullptr;
	}
}