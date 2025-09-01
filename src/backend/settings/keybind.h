#ifndef keybind_h
#define keybind_h

class Keybind {
public:
	// Eunms exactly matches RmlUi's mapping (copied from RmlUi)
	enum KeyId : unsigned char {
		KI_UNKNOWN = 0,

		KI_SPACE = 1,

		KI_0 = 2,
		KI_1 = 3,
		KI_2 = 4,
		KI_3 = 5,
		KI_4 = 6,
		KI_5 = 7,
		KI_6 = 8,
		KI_7 = 9,
		KI_8 = 10,
		KI_9 = 11,

		KI_A = 12,
		KI_B = 13,
		KI_C = 14,
		KI_D = 15,
		KI_E = 16,
		KI_F = 17,
		KI_G = 18,
		KI_H = 19,
		KI_I = 20,
		KI_J = 21,
		KI_K = 22,
		KI_L = 23,
		KI_M = 24,
		KI_N = 25,
		KI_O = 26,
		KI_P = 27,
		KI_Q = 28,
		KI_R = 29,
		KI_S = 30,
		KI_T = 31,
		KI_U = 32,
		KI_V = 33,
		KI_W = 34,
		KI_X = 35,
		KI_Y = 36,
		KI_Z = 37,

		KI_OEM_1 = 38,		// US standard keyboard; the ';:' key.
		KI_OEM_PLUS = 39,	// Any region; the '=+' key.
		KI_OEM_COMMA = 40,	// Any region; the ',<' key.
		KI_OEM_MINUS = 41,	// Any region; the '-_' key.
		KI_OEM_PERIOD = 42, // Any region; the '.>' key.
		KI_OEM_2 = 43,		// Any region; the '/?' key.
		KI_OEM_3 = 44,		// Any region; the '`~' key.

		KI_OEM_4 = 45, // US standard keyboard; the '[{' key.
		KI_OEM_5 = 46, // US standard keyboard; the '\|' key.
		KI_OEM_6 = 47, // US standard keyboard; the ']}' key.
		KI_OEM_7 = 48, // US standard keyboard; the ''"' key.
		KI_OEM_8 = 49,

		KI_OEM_102 = 50, // RT 102-key keyboard; the '<>' or '\|' key.

		KI_NUMPAD0 = 51,
		KI_NUMPAD1 = 52,
		KI_NUMPAD2 = 53,
		KI_NUMPAD3 = 54,
		KI_NUMPAD4 = 55,
		KI_NUMPAD5 = 56,
		KI_NUMPAD6 = 57,
		KI_NUMPAD7 = 58,
		KI_NUMPAD8 = 59,
		KI_NUMPAD9 = 60,
		KI_NUMPADENTER = 61,
		KI_MULTIPLY = 62, // Asterisk on the numeric keypad.
		KI_ADD = 63,	  // Plus on the numeric keypad.
		KI_SEPARATOR = 64,
		KI_SUBTRACT = 65, // Minus on the numeric keypad.
		KI_DECIMAL = 66,  // Period on the numeric keypad.
		KI_DIVIDE = 67,	  // Forward Slash on the numeric keypad.

		/*
		 * NEC PC-9800 kbd definitions
		 */
		KI_OEM_NEC_EQUAL = 68, // Equals key on the numeric keypad.

		KI_BACK = 69, // Backspace key.
		KI_TAB = 70,  // Tab key.

		KI_CLEAR = 71,
		KI_RETURN = 72,

		KI_PAUSE = 73,
		KI_CAPITAL = 74, // Capslock key.

		KI_KANA = 75,	// IME Kana mode.
		KI_HANGUL = 76, // IME Hangul mode.
		KI_JUNJA = 77,	// IME Junja mode.
		KI_FINAL = 78,	// IME final mode.
		KI_HANJA = 79,	// IME Hanja mode.
		KI_KANJI = 80,	// IME Kanji mode.

		KI_ESCAPE = 81, // Escape key.

		KI_CONVERT = 82,	// IME convert.
		KI_NONCONVERT = 83, // IME nonconvert.
		KI_ACCEPT = 84,		// IME accept.
		KI_MODECHANGE = 85, // IME mode change request.

		KI_PRIOR = 86, // Page Up key.
		KI_NEXT = 87,  // Page Down key.
		KI_END = 88,
		KI_HOME = 89,
		KI_LEFT = 90,  // Left Arrow key.
		KI_UP = 91,	   // Up Arrow key.
		KI_RIGHT = 92, // Right Arrow key.
		KI_DOWN = 93,  // Down Arrow key.
		KI_SELECT = 94,
		KI_PRINT = 95,
		KI_EXECUTE = 96,
		KI_SNAPSHOT = 97, // Print Screen key.
		KI_INSERT = 98,
		KI_DELETE = 99,
		KI_HELP = 100,

		KI_APPS = 103, // Applications key.

		KI_POWER = 104,
		KI_SLEEP = 105,
		KI_WAKE = 106,

		KI_F1 = 107,
		KI_F2 = 108,
		KI_F3 = 109,
		KI_F4 = 110,
		KI_F5 = 111,
		KI_F6 = 112,
		KI_F7 = 113,
		KI_F8 = 114,
		KI_F9 = 115,
		KI_F10 = 116,
		KI_F11 = 117,
		KI_F12 = 118,
		KI_F13 = 119,
		KI_F14 = 120,
		KI_F15 = 121,
		KI_F16 = 122,
		KI_F17 = 123,
		KI_F18 = 124,
		KI_F19 = 125,
		KI_F20 = 126,
		KI_F21 = 127,
		KI_F22 = 128,
		KI_F23 = 129,
		KI_F24 = 130,

		KI_NUMLOCK = 131, // Numlock key.
		KI_SCROLL = 132,  // Scroll Lock key.

		KI_BROWSER_BACK = 144,
		KI_BROWSER_FORWARD = 145,
		KI_BROWSER_REFRESH = 146,
		KI_BROWSER_STOP = 147,
		KI_BROWSER_SEARCH = 148,
		KI_BROWSER_FAVORITES = 149,
		KI_BROWSER_HOME = 150,

		KI_VOLUME_MUTE = 151,
		KI_VOLUME_DOWN = 152,
		KI_VOLUME_UP = 153,
		KI_MEDIA_NEXT_TRACK = 154,
		KI_MEDIA_PREV_TRACK = 155,
		KI_MEDIA_STOP = 156,
		KI_MEDIA_PLAY_PAUSE = 157,
		KI_LAUNCH_MAIL = 158,
		KI_LAUNCH_MEDIA_SELECT = 159,
		KI_LAUNCH_APP1 = 160,
		KI_LAUNCH_APP2 = 161,

		/*
		 * Various extended or enhanced keyboards
		 */
		KI_OEM_AX = 162,
		KI_ICO_HELP = 163,
		KI_ICO_00 = 164,

		KI_PROCESSKEY = 165, // IME Process key.

		KI_ICO_CLEAR = 166,

		KI_ATTN = 167,
		KI_CRSEL = 168,
		KI_EXSEL = 169,
		KI_EREOF = 170,
		KI_PLAY = 171,
		KI_ZOOM = 172,
		KI_PA1 = 173,
		KI_OEM_CLEAR = 174,

		/*
		 * Custom keys that users can assign their own meaning to.
		 */
		KI_FIRST_CUSTOM_KEY,
		KI_LAST_CUSTOM_KEY = 250,
	};

	enum KeyMod : unsigned char {
		KM_UNKNOWN = 0,
		KM_CTRL = 1 << 0,	   // Set if at least one Ctrl key is depressed.
		KM_SHIFT = 1 << 1,	   // Set if at least one Shift key is depressed.
		KM_ALT = 1 << 2,	   // Set if at least one Alt key is depressed.
		KM_META = 1 << 3,	   // Set if at least one Meta key (the command key) is depressed.
		KM_CAPSLOCK = 1 << 4,  // Set if caps lock is enabled.
		KM_NUMLOCK = 1 << 5,   // Set if num lock is enabled.
		KM_SCROLLLOCK = 1 << 6 // Set if scroll lock is enabled.
	};

public:
	Keybind() = default;
	Keybind(unsigned int keyCombined) : keyCombined(keyCombined) { }
	Keybind(KeyId key) : Keybind((key << 8)) { }
	Keybind(KeyId key, KeyMod modifier) : Keybind((key << 8) + modifier) { }
	Keybind(KeyId key, unsigned int modifier) : Keybind((key << 8) + modifier) { } // because "a|b" outputs a int
	bool operator==(Keybind keybind) const { return keybind.getKeyCombined() == keyCombined; }
	bool operator!=(Keybind keybind) const { return keybind.getKeyCombined() != keyCombined; }

	unsigned int getKeyCombined() const { return keyCombined; }
	std::string toString() const {
		std::string keyString;
		if (keyCombined & 4U) {
			if (keyString.size()) keyString += " + ";
			keyString += "ALT";
		};
		if (keyCombined & 1U) {
			if (keyString.size()) keyString += " + ";
			#ifdef __APPLE__
				keyString += "COMMAND";
			#else
				keyString += "CTRL";
			#endif
		};
		if (keyCombined & 2U) {
			if (keyString.size()) keyString += " + ";
			keyString += "SHIFT";
		};
		if (keyCombined & 8U) {
			if (keyString.size()) keyString += " + ";
			#ifdef __APPLE__
				keyString += "CTRL";
			#else
				keyString += "META";
			#endif
		};
		KeyId key = (KeyId)(keyCombined >> 8);
		if (key != 0) {
			if (keyString.size()) keyString += " + ";
			switch (key) {
			case KeyId::KI_SPACE: keyString += "SPACE"; break;
			case KeyId::KI_0: keyString += "0"; break;
			case KeyId::KI_1: keyString += "1"; break;
			case KeyId::KI_2: keyString += "2"; break;
			case KeyId::KI_3: keyString += "3"; break;
			case KeyId::KI_4: keyString += "4"; break;
			case KeyId::KI_5: keyString += "5"; break;
			case KeyId::KI_6: keyString += "6"; break;
			case KeyId::KI_7: keyString += "7"; break;
			case KeyId::KI_8: keyString += "8"; break;
			case KeyId::KI_9: keyString += "9"; break;
			case KeyId::KI_A: keyString += "A"; break;
			case KeyId::KI_B: keyString += "B"; break;
			case KeyId::KI_C: keyString += "C"; break;
			case KeyId::KI_D: keyString += "D"; break;
			case KeyId::KI_E: keyString += "E"; break;
			case KeyId::KI_F: keyString += "F"; break;
			case KeyId::KI_G: keyString += "G"; break;
			case KeyId::KI_H: keyString += "H"; break;
			case KeyId::KI_I: keyString += "I"; break;
			case KeyId::KI_J: keyString += "J"; break;
			case KeyId::KI_K: keyString += "K"; break;
			case KeyId::KI_L: keyString += "L"; break;
			case KeyId::KI_M: keyString += "M"; break;
			case KeyId::KI_N: keyString += "N"; break;
			case KeyId::KI_O: keyString += "O"; break;
			case KeyId::KI_P: keyString += "P"; break;
			case KeyId::KI_Q: keyString += "Q"; break;
			case KeyId::KI_R: keyString += "R"; break;
			case KeyId::KI_S: keyString += "S"; break;
			case KeyId::KI_T: keyString += "T"; break;
			case KeyId::KI_U: keyString += "U"; break;
			case KeyId::KI_V: keyString += "V"; break;
			case KeyId::KI_W: keyString += "W"; break;
			case KeyId::KI_X: keyString += "X"; break;
			case KeyId::KI_Y: keyString += "Y"; break;
			case KeyId::KI_Z: keyString += "Z"; break;
			case KeyId::KI_OEM_1: keyString += ";"; break;
			case KeyId::KI_OEM_PLUS: keyString += "="; break;
			case KeyId::KI_OEM_COMMA: keyString += ","; break;
			case KeyId::KI_OEM_MINUS: keyString += "-"; break;
			case KeyId::KI_OEM_PERIOD: keyString += "."; break;
			case KeyId::KI_OEM_2: keyString += "/"; break;
			case KeyId::KI_OEM_3: keyString += "`"; break;
			case KeyId::KI_OEM_4: keyString += "["; break;
			case KeyId::KI_OEM_5: keyString += "\\"; break;
			case KeyId::KI_OEM_6: keyString += "]"; break;
			case KeyId::KI_OEM_7: keyString += "'"; break;
			case KeyId::KI_OEM_8: keyString += "OEM_8"; break;
			case KeyId::KI_OEM_102: keyString += "OEM_102"; break;
			case KeyId::KI_NUMPAD0: keyString += "Num0"; break;
			case KeyId::KI_NUMPAD1: keyString += "Num1"; break;
			case KeyId::KI_NUMPAD2: keyString += "Num2"; break;
			case KeyId::KI_NUMPAD3: keyString += "Num3"; break;
			case KeyId::KI_NUMPAD4: keyString += "Num4"; break;
			case KeyId::KI_NUMPAD5: keyString += "Num5"; break;
			case KeyId::KI_NUMPAD6: keyString += "Num6"; break;
			case KeyId::KI_NUMPAD7: keyString += "Num7"; break;
			case KeyId::KI_NUMPAD8: keyString += "Num8"; break;
			case KeyId::KI_NUMPAD9: keyString += "Num9"; break;
			case KeyId::KI_NUMPADENTER: keyString += "Num Enter"; break;
			case KeyId::KI_MULTIPLY: keyString += "Num Multiply"; break;
			case KeyId::KI_ADD: keyString += "Num Plus"; break;
			case KeyId::KI_SEPARATOR: keyString += "Separator"; break;
			case KeyId::KI_SUBTRACT: keyString += "Num Minus"; break;
			case KeyId::KI_DECIMAL: keyString += "NumpadDecimal"; break;
			case KeyId::KI_DIVIDE: keyString += "NumDivide"; break;
			case KeyId::KI_OEM_NEC_EQUAL: keyString += "NumEquals"; break;
			case KeyId::KI_BACK: keyString += "Backspace"; break;
			case KeyId::KI_TAB: keyString += "Tab"; break;
			case KeyId::KI_CLEAR: keyString += "Clear"; break;
			case KeyId::KI_PAUSE: keyString += "Pause"; break;
			case KeyId::KI_CAPITAL: keyString += "CapsLock"; break;
			case KeyId::KI_KANA: keyString += "Kana"; break;
			case KeyId::KI_HANGUL: keyString += "Hangul"; break;
			case KeyId::KI_JUNJA: keyString += "Junja"; break;
			case KeyId::KI_FINAL: keyString += "Final"; break;
			case KeyId::KI_HANJA: keyString += "Hanja"; break;
			case KeyId::KI_KANJI: keyString += "Kanji"; break;
			case KeyId::KI_CONVERT: keyString += "Convert"; break;
			case KeyId::KI_NONCONVERT: keyString += "Nonconvert"; break;
			case KeyId::KI_ACCEPT: keyString += "Accept"; break;
			case KeyId::KI_MODECHANGE: keyString += "Mode Change"; break;
			case KeyId::KI_PRIOR: keyString += "Page Up"; break;
			case KeyId::KI_NEXT: keyString += "Page Down"; break;
			case KeyId::KI_END: keyString += "End"; break;
			case KeyId::KI_HOME: keyString += "Home"; break;
			case KeyId::KI_LEFT: keyString += "Left Arrow"; break;
			case KeyId::KI_UP: keyString += "Up Arrow"; break;
			case KeyId::KI_RIGHT: keyString += "Right Arrow"; break;
			case KeyId::KI_DOWN: keyString += "Down Arrow"; break;
			case KeyId::KI_SELECT: keyString += "Select"; break;
			case KeyId::KI_PRINT: keyString += "Print"; break;
			case KeyId::KI_EXECUTE: keyString += "Execute"; break;
			case KeyId::KI_SNAPSHOT: keyString += "Print Screen"; break;
			case KeyId::KI_INSERT: keyString += "Insert"; break;
			case KeyId::KI_DELETE: keyString += "Delete"; break;
			case KeyId::KI_HELP: keyString += "Help"; break;
			case KeyId::KI_APPS: keyString += "Apps"; break;
			case KeyId::KI_POWER: keyString += "Power"; break;
			case KeyId::KI_SLEEP: keyString += "Sleep"; break;
			case KeyId::KI_WAKE: keyString += "Wake"; break;
			case KeyId::KI_F1: keyString += "F1"; break;
			case KeyId::KI_F2: keyString += "F2"; break;
			case KeyId::KI_F3: keyString += "F3"; break;
			case KeyId::KI_F4: keyString += "F4"; break;
			case KeyId::KI_F5: keyString += "F5"; break;
			case KeyId::KI_F6: keyString += "F6"; break;
			case KeyId::KI_F7: keyString += "F7"; break;
			case KeyId::KI_F8: keyString += "F8"; break;
			case KeyId::KI_F9: keyString += "F9"; break;
			case KeyId::KI_F10: keyString += "F10"; break;
			case KeyId::KI_F11: keyString += "F11"; break;
			case KeyId::KI_F12: keyString += "F12"; break;
			case KeyId::KI_F13: keyString += "F13"; break;
			case KeyId::KI_F14: keyString += "F14"; break;
			case KeyId::KI_F15: keyString += "F15"; break;
			case KeyId::KI_F16: keyString += "F16"; break;
			case KeyId::KI_F17: keyString += "F17"; break;
			case KeyId::KI_F18: keyString += "F18"; break;
			case KeyId::KI_F19: keyString += "F19"; break;
			case KeyId::KI_F20: keyString += "F20"; break;
			case KeyId::KI_F21: keyString += "F21"; break;
			case KeyId::KI_F22: keyString += "F22"; break;
			case KeyId::KI_F23: keyString += "F23"; break;
			case KeyId::KI_F24: keyString += "F24"; break;
			case KeyId::KI_NUMLOCK: keyString += "NumLock"; break;
			case KeyId::KI_SCROLL: keyString += "Scroll Lock"; break;
			case KeyId::KI_BROWSER_BACK: keyString += "Browser Back"; break;
			case KeyId::KI_BROWSER_FORWARD: keyString += "Browser Forward"; break;
			case KeyId::KI_BROWSER_REFRESH: keyString += "Browser Refresh"; break;
			case KeyId::KI_BROWSER_STOP: keyString += "Browser Stop"; break;
			case KeyId::KI_BROWSER_SEARCH: keyString += "Browser Search"; break;
			case KeyId::KI_BROWSER_FAVORITES: keyString += "Browser Favorites"; break;
			case KeyId::KI_BROWSER_HOME: keyString += "Browser Home"; break;
			case KeyId::KI_VOLUME_MUTE: keyString += "Volume Mute"; break;
			case KeyId::KI_VOLUME_DOWN: keyString += "Volume Down"; break;
			case KeyId::KI_VOLUME_UP: keyString += "Volume Up"; break;
			case KeyId::KI_MEDIA_NEXT_TRACK: keyString += "Media Next"; break;
			case KeyId::KI_MEDIA_PREV_TRACK: keyString += "Media Previous"; break;
			case KeyId::KI_MEDIA_STOP: keyString += "Media Stop"; break;
			case KeyId::KI_MEDIA_PLAY_PAUSE: keyString += "Media Play/Pause"; break;
			case KeyId::KI_LAUNCH_MAIL: keyString += "Launch Mail"; break;
			case KeyId::KI_LAUNCH_MEDIA_SELECT: keyString += "Launch Media Select"; break;
			case KeyId::KI_LAUNCH_APP1: keyString += "Launch App1"; break;
			case KeyId::KI_LAUNCH_APP2: keyString += "Launch App2"; break;
			case KeyId::KI_OEM_AX: keyString += "OEM AX"; break;
			case KeyId::KI_ICO_HELP: keyString += "ICO Help"; break;
			case KeyId::KI_ICO_00: keyString += "ICO 00"; break;
			case KeyId::KI_PROCESSKEY: keyString += "Process Key"; break;
			case KeyId::KI_ICO_CLEAR: keyString += "ICO Clear"; break;
			case KeyId::KI_ATTN: keyString += "ATTN"; break;
			case KeyId::KI_CRSEL: keyString += "CRSEL"; break;
			case KeyId::KI_EXSEL: keyString += "EXSEL"; break;
			case KeyId::KI_EREOF: keyString += "EREOF"; break;
			case KeyId::KI_PLAY: keyString += "Play"; break;
			case KeyId::KI_ZOOM: keyString += "Zoom"; break;
			case KeyId::KI_PA1: keyString += "PA1"; break;
			case KeyId::KI_OEM_CLEAR: keyString += "OEM Clear"; break;
			default: keyString += "<" + std::to_string(key) + ">";
			}
		}

		return keyString;
	}

private:
	unsigned int keyCombined = 0;
};

template <> struct std::hash<Keybind> {
	inline std::size_t operator()(Keybind keybind) const noexcept { return std::hash<unsigned int>{}(keybind.getKeyCombined()); }
};

#endif /* keybind_h */
