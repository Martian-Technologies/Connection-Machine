#ifndef color_h
#define color_h

struct Color {
	inline Color() : r(1.0f), g(1.0f), b(1.0f) { }
	inline Color(float r, float g, float b) : r(r), g(g), b(b) { }

	inline bool operator==(const Color& other) const { return r == other.r && g == other.g && b == other.b; }
	inline bool operator!=(const Color& other) const { return !operator==(other); }
	inline Color operator+(const Color& color) const { return Color(color.r + r, color.g + g, color.b + b); }
	inline Color operator-(const Color& color) const { return Color(color.r - r, color.g - g, color.b - b); }
	inline std::string toString() const { return "(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")"; }

	float r, g, b;
};

#endif /* color_h */
