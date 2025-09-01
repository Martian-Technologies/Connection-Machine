#ifndef keybindHelpers_h
#define keybindHelpers_h

#include <RmlUi/Core.h>

#include "backend/settings/keybind.h"

inline Keybind makeKeybind(Rml::Input::KeyIdentifier key, unsigned int modifier = 0) {
	switch (key) {
	case Rml::Input::KeyIdentifier::KI_LSHIFT:
	case Rml::Input::KeyIdentifier::KI_RSHIFT:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_SHIFT);
	case Rml::Input::KeyIdentifier::KI_LMENU:
	case Rml::Input::KeyIdentifier::KI_RMENU:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_ALT);
#ifdef __APPLE__
	case Rml::Input::KeyIdentifier::KI_LCONTROL:
	case Rml::Input::KeyIdentifier::KI_RCONTROL:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_META);
	case Rml::Input::KeyIdentifier::KI_LMETA:
	case Rml::Input::KeyIdentifier::KI_RMETA:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_CTRL);
#else
	case Rml::Input::KeyIdentifier::KI_LCONTROL:
	case Rml::Input::KeyIdentifier::KI_RCONTROL:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_CTRL);
	case Rml::Input::KeyIdentifier::KI_LMETA:
	case Rml::Input::KeyIdentifier::KI_RMETA:
		return Keybind(modifier | Rml::Input::KeyModifier::KM_META);
#endif
	default: return Keybind((key << 8) + modifier);
	}
}

inline Keybind makeKeybind(const Rml::Event& event) {
	return makeKeybind(
		event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_UNKNOWN),
		event.GetParameter<int>("alt_key", 0) * Rml::Input::KeyModifier::KM_ALT + event.GetParameter<int>("ctrl_key", 0) * Rml::Input::KeyModifier::KM_CTRL +
			event.GetParameter<int>("shift_key", 0) * Rml::Input::KeyModifier::KM_SHIFT + event.GetParameter<int>("meta_key", 0) * Rml::Input::KeyModifier::KM_META
	);
}

#endif /* keybindHelpers_h */
