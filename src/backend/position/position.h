#ifndef position_h
#define position_h

#include "util/fastMath.h"

typedef int coordinate_t;
typedef float f_coordinate_t;

struct Vector;
struct FVector;
struct Position;
struct FPosition;
struct Size;
struct FSize;

struct Vector {
	class Iterator;

	inline Vector() noexcept : dx(0), dy(0) { }
	inline Vector(coordinate_t dx, coordinate_t dy) noexcept : dx(dx), dy(dy) { }
	// allows the easy creation of vectors that are all the same value
	inline Vector(coordinate_t d) noexcept : dx(d), dy(d) { }
	inline FVector free() const noexcept;

	inline void extentToFit(Vector vector) noexcept {
		dx = std::max(dx, vector.dx);
		dy = std::max(dy, vector.dy);
	}
	inline std::string toString() const noexcept { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ">"; }

	inline bool operator==(Vector other) const noexcept { return dx == other.dx && dy == other.dy; }
	inline bool operator!=(Vector other) const noexcept { return !operator==(other); }

	inline bool hasZeros() const noexcept { return !(dx && dy); }
	inline bool widthInSize(Size size) const noexcept;

	inline coordinate_t manhattenLength() const noexcept { return Abs(dx) + Abs(dy); }
	inline f_coordinate_t lengthSquared() const noexcept { return FastPower<2>(dx) + FastPower<2>(dy); }
	inline f_coordinate_t length() const noexcept { return sqrt(lengthSquared()); }

	inline Vector operator+(Vector other) const noexcept { return Vector(dx + other.dx, dy + other.dy); }
	inline Vector& operator+=(Vector other) noexcept {
		dx += other.dx;
		dy += other.dy;
		return *this;
	}
	inline Vector operator-(Vector other) const noexcept { return Vector(dx - other.dx, dy - other.dy); }
	inline Vector& operator-=(Vector other) noexcept {
		dx -= other.dx;
		dy -= other.dy;
		return *this;
	}
	inline coordinate_t operator*(Vector vector) const noexcept { return dx * vector.dx + dy * vector.dy; }
	inline Vector operator*(coordinate_t scalar) const noexcept { return Vector(dx * scalar, dy * scalar); }
	inline Vector& operator*=(coordinate_t scalar) noexcept {
		dx *= scalar;
		dy *= scalar;
		return *this;
	}
	inline Vector operator/(coordinate_t scalar) const noexcept { return Vector(dx / scalar, dy / scalar); }
	inline Vector& operator/=(coordinate_t scalar) noexcept {
		dx /= scalar;
		dy /= scalar;
		return *this;
	}

	inline Iterator iter() const noexcept;

	coordinate_t dx, dy;
};

class Vector::Iterator {
public:
	inline Iterator(Vector vector) {
		if (vector == Vector(0)) {
			end = 0;
			width = 1;
			return;
		}
		xNeg = 1 - 2 * (vector.dx < 0);
		width = xNeg * vector.dx + 1;
		yNeg = 1 - 2 * (vector.dy < 0);
		end = (yNeg * vector.dy + 1) * width - 1;
	}
	inline Iterator& operator++() {
		next();
		return *this;
	}
	inline Iterator& operator--() {
		prev();
		return *this;
	}
	inline Iterator operator++(int) {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const { return notDone; }
	inline Vector operator*() const { return Vector(xNeg * cur % width, yNeg * cur / width); }
	// inline Vector operator->() const { return *(*this); }

private:
	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		notDone = true;
		cur -= cur != 0;
	}
	char xNeg;
	char yNeg;
	unsigned int end;
	unsigned int cur = 0;
	unsigned width;
	bool notDone = true;
};

Vector::Iterator Vector::iter() const noexcept { return Iterator(*this); }

template <>
struct std::hash<Vector> {
	inline std::size_t operator()(Vector vec) const noexcept {
		std::size_t x = std::hash<coordinate_t>{}(vec.dx);
		std::size_t y = std::hash<coordinate_t>{}(vec.dy);
		return (std::size_t)x ^ ((std::size_t)y << 32);
	}
};

template <>
struct fmt::formatter<Vector> : fmt::formatter<std::string> {
	auto format(Vector v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

struct FVector {
	inline FVector() noexcept : dx(0.0f), dy(0.0f) { }
	inline FVector(f_coordinate_t dx, f_coordinate_t dy) noexcept : dx(dx), dy(dy) { }
	// allows the easy creation of fvectors that are all the same value
	inline FVector(f_coordinate_t d) noexcept : dx(d), dy(d) { }
	inline Vector snap() const noexcept;

	inline std::string toString() const noexcept { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ">"; }

	inline bool operator==(FVector other) const noexcept { return approx_equals(dx, other.dx) && approx_equals(dy, other.dy); }
	inline bool operator!=(FVector other) const noexcept { return !operator==(other); }

	inline f_coordinate_t manhattenLength() const noexcept { return Abs(dx) + Abs(dy); }
	inline f_coordinate_t lengthSquared() const noexcept { return FastPower<2>(dx) + FastPower<2>(dy); }
	inline f_coordinate_t length() const noexcept { return sqrt(FastPower<2>(dx) + FastPower<2>(dy)); }

	inline FVector operator+(FVector other) const noexcept { return FVector(dx + other.dx, dy + other.dy); }
	inline FVector& operator+=(FVector other) noexcept {
		dx += other.dx;
		dy += other.dy;
		return *this;
	}
	inline FVector operator-(FVector other) const noexcept { return FVector(dx - other.dx, dy - other.dy); }
	inline FVector& operator-=(FVector other) noexcept {
		dx -= other.dx;
		dy -= other.dy;
		return *this;
	}
	inline FVector operator*(f_coordinate_t scalar) const noexcept { return FVector(dx * scalar, dy * scalar); }
	inline FVector& operator*=(f_coordinate_t scalar) noexcept {
		dx *= scalar, dy *= scalar;
		return *this;
	}
	inline f_coordinate_t operator*(FVector vector) const noexcept { return dx * vector.dx + dy * vector.dy; }
	inline FVector operator/(f_coordinate_t scalar) noexcept { return FVector(dx / scalar, dy / scalar); }
	inline FVector& operator/=(f_coordinate_t scalar) noexcept {
		dx /= scalar;
		dy /= scalar;
		return *this;
	}

	inline f_coordinate_t lengthAlongProjectToVec(FVector vector) const noexcept { return (*this * vector) / vector.length(); }
	inline FVector projectToVec(FVector vector) const noexcept { return vector * (*this * vector) / vector.lengthSquared(); }

	f_coordinate_t dx, dy;
};

template <>
struct fmt::formatter<FVector> : fmt::formatter<std::string> {
	auto format(FVector v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

struct Position {
	class Iterator;

	inline Position() noexcept : x(0), y(0) { }
	inline Position(coordinate_t x, coordinate_t y) noexcept : x(x), y(y) { }
	inline FPosition free() const noexcept;

	inline std::string toString() const noexcept { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

	inline bool operator==(Position position) const noexcept { return x == position.x && y == position.y; }
	inline bool operator!=(Position position) const noexcept { return !operator==(position); }
	inline bool withinArea(Position small, Position large) const noexcept { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }

	inline coordinate_t manhattenDistanceTo(Position position) const noexcept { return Abs(x - position.x) + Abs(y - position.y); }
	inline coordinate_t manhattenDistanceToOrigin() const noexcept { return Abs(x) + Abs(y); }
	inline coordinate_t distanceToSquared(Position position) const noexcept { return FastPower<2>(x - position.x) + FastPower<2>(y - position.y); }
	inline coordinate_t distanceToOriginSquared() const noexcept { return FastPower<2>(x) + FastPower<2>(y); }
	inline f_coordinate_t distanceTo(Position position) const noexcept { return sqrt(FastPower<2>(x - position.x) + FastPower<2>(y - position.y)); }
	inline f_coordinate_t distanceToOrigin() const noexcept { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }

	inline Position operator+(Vector vector) const noexcept { return Position(x + vector.dx, y + vector.dy); }
	inline Position& operator+=(Vector vector) noexcept {
		x += vector.dx;
		y += vector.dy;
		return *this;
	}
	inline Vector operator-(Position position) const noexcept { return Vector(x - position.x, y - position.y); }
	inline Position operator-(Vector vector) const noexcept { return Position(x - vector.dx, y - vector.dy); }
	inline Position& operator-=(Vector vector) noexcept {
		x -= vector.dx;
		y -= vector.dy;
		return *this;
	}

	inline Iterator iterTo(Position other) const noexcept;

	coordinate_t x, y;
};

class Position::Iterator {
public:
	inline Iterator(Position start, Position end) noexcept {
		if (start == end) {
			this->end = 0;
			this->start = start;
			width = 1;
			return;
		}
		if (start.x > end.x) {
			this->start.x = end.x;
			width = start.x - end.x + 1;
		} else {
			this->start.x = start.x;
			width = end.x - start.x + 1;
		}
		if (start.y > end.y) {
			this->end = (start.y - end.y + 1) * width - 1;
			this->start.y = end.y;
		} else {
			this->end = (end.y - start.y + 1) * width - 1;
			this->start.y = start.y;
		}
	}
	inline Iterator& operator++() noexcept {
		next();
		return *this;
	}
	inline Iterator& operator--() noexcept {
		prev();
		return *this;
	}
	inline Iterator operator++(int) noexcept {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) noexcept {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const noexcept { return notDone; }
	inline const Position operator*() const noexcept { return start + Vector(cur % width, cur / width); }
	inline const Position operator->() const noexcept { return *(*this); }

private:
	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		cur -= notDone && cur != 0;
		notDone = true;
	}
	Position start;
	unsigned int end;
	unsigned int cur = 0;
	unsigned width;
	bool notDone = true;
};

Position::Iterator Position::iterTo(Position other) const noexcept { return Iterator(*this, other); }

inline bool areaWithinArea(Position area1Small, Position area1Large, Position area2Small, Position area2Large) {
	return (
		area2Small.withinArea(area1Small, area1Large) ||
		area2Large.withinArea(area1Small, area1Large) ||
		area1Small.withinArea(area2Small, area2Large) ||
		area2Large.withinArea(area2Small, area2Large));
}

template <>
struct std::hash<Position> {
	inline std::size_t operator()(Position pos) const noexcept {
		std::size_t x = std::hash<coordinate_t>{}(pos.x);
		std::size_t y = std::hash<coordinate_t>{}(pos.y);
		return y + 0x9e3779b9 + (x << 6) + (x >> 2);
	}
};

template <>
struct std::hash<std::pair<Position, Position>> {
	inline std::size_t operator()(const std::pair<Position, Position>& posPair) const noexcept {
		std::size_t a = std::hash<Position>{}(posPair.first);
		std::size_t b = std::hash<Position>{}(posPair.second);
		return a + 0x9e3779b9 + (b << 6) + (b >> 2);
	}
};

template <>
struct fmt::formatter<Position> : fmt::formatter<std::string> {
	auto format(Position v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

struct FPosition {
	static FPosition getInvalid() { return FPosition(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()); }
	inline FPosition() noexcept : x(0.0f), y(0.0f) { }
	inline FPosition(f_coordinate_t x, f_coordinate_t y) noexcept : x(x), y(y) { }
	inline Position snap() const noexcept;

	inline std::string toString() const noexcept { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

	inline void setInvalid() { x = y = std::numeric_limits<double>::quiet_NaN(); }
	inline bool isValid() const { return !isInvalid(); }
	inline bool isInvalid() const { return (std::isnan(x) || std::isnan(y)); }

	inline bool operator==(FPosition position) const noexcept { return approx_equals(x, position.x) && approx_equals(y, position.y); }
	inline bool operator!=(FPosition position) const noexcept { return !operator==(position); }
	inline bool withinArea(FPosition small, FPosition large) const noexcept { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }

	inline f_coordinate_t manhattenDistanceTo(FPosition other) const noexcept { return Abs(x - other.x) + Abs(y - other.y); }
	inline f_coordinate_t manhattenDistanceToOrigin() const noexcept { return Abs(x) + Abs(y); }
	inline f_coordinate_t distanceToSquared(FPosition other) const noexcept { return FastPower<2>(x - other.x) + FastPower<2>(y - other.y); }
	inline f_coordinate_t distanceToOriginSquared() const noexcept { return FastPower<2>(x) + FastPower<2>(y); }
	inline f_coordinate_t distanceTo(FPosition other) const noexcept { return sqrt(FastPower<2>(x - other.x) + FastPower<2>(y - other.y)); }
	inline f_coordinate_t distanceToOrigin() const noexcept { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }

	inline FPosition operator+(FVector vector) const noexcept { return FPosition(x + vector.dx, y + vector.dy); }
	inline FPosition& operator+=(FVector vector) noexcept {
		x += vector.dx;
		y += vector.dy;
		return *this;
	}
	inline FVector operator-(FPosition position) const noexcept { return FVector(x - position.x, y - position.y); }
	inline FPosition operator-(FVector vector) const noexcept { return FPosition(x - vector.dx, y - vector.dy); }
	inline FPosition& operator-=(FVector vector) noexcept {
		x -= vector.dx;
		y -= vector.dy;
		return *this;
	}
	inline FPosition operator*(f_coordinate_t scalar) const noexcept { return FPosition(x * scalar, y * scalar); }
	inline f_coordinate_t lengthAlongProjectToVec(FPosition orginOfVec, FVector vector) const noexcept { return (*this - orginOfVec).lengthAlongProjectToVec(vector); }
	inline FPosition projectToVec(FPosition orginOfVec, FVector vector) const noexcept { return orginOfVec + (*this - orginOfVec).projectToVec(vector); }

	f_coordinate_t x, y;
};

inline bool areaWithinArea(FPosition area1Small, FPosition area1Large, FPosition area2Small, FPosition area2Large) noexcept {
	return (
		area2Small.withinArea(area1Small, area1Large) ||
		area2Large.withinArea(area1Small, area1Large) ||
		area1Small.withinArea(area2Small, area2Large) ||
		area2Large.withinArea(area2Small, area2Large));
}

template <>
struct fmt::formatter<FPosition> : fmt::formatter<std::string> {
	auto format(FPosition v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

struct Size {
	class Iterator;

	inline Size() noexcept : w(0), h(0) { }
	inline Size(coordinate_t w, coordinate_t h) noexcept : w(w), h(h) { }
	// makes the size for hypercube with some edges length
	inline Size(coordinate_t sideLength) noexcept : w(sideLength), h(sideLength) { }
	inline Size(Position cornerA, Position cornerB) noexcept : w(Abs(cornerA.x - cornerB.x)), h(Abs(cornerA.y - cornerB.y)) { }
	inline FSize free() const noexcept;

	inline void extentToFit(Vector vector) noexcept {
		w = std::max(w, vector.dx + 1);
		h = std::max(h, vector.dy + 1);
	}
	inline std::string toString() const noexcept { return std::to_string(w) + "x" + std::to_string(h); }

	inline bool operator==(Size other) const noexcept { return w == other.w && h == other.h; }
	inline bool operator!=(Size other) const noexcept { return !operator==(other); }

	// w != 0 and h != 0
	inline bool isValid() const noexcept { return w > 0 && h > 0; }

	inline coordinate_t area() const noexcept { return w * h; }
	inline coordinate_t perimeter() const noexcept { return w * 2 + h * 2; }

	inline Iterator iter() const noexcept;

	inline Vector getLargestVectorInArea() { return Vector(w - 1, h - 1); }

	coordinate_t w, h;
};

template <>
struct fmt::formatter<Size> : fmt::formatter<std::string> {
	auto format(Size v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

class Size::Iterator {
public:
	inline Iterator(Size size) {
		if (!size.isValid()) {
			notDone = false;
			end = 0;
			width = 0;
			return;
		}
		width = size.w;
		end = size.area() - 1;
	}
	inline Iterator& operator++() {
		next();
		return *this;
	}
	inline Iterator& operator--() {
		prev();
		return *this;
	}
	inline Iterator operator++(int) {
		Iterator tmp = *this;
		next();
		return tmp;
	}
	inline Iterator operator--(int) {
		Iterator tmp = *this;
		prev();
		return tmp;
	}
	inline explicit operator bool() const { return notDone; }
	inline Vector operator*() const {
#ifndef DEBUG // I dont know if this works
		if (!width) {
			logError("Reading Size::Iterator iterating over invalid size not valid. Fix this!");
		}
#endif
		return Vector(cur % width, cur / width);
	}
	// inline Vector operator->() const { return *(*this); }

private:

	inline void next() {
		notDone = cur != end;
		cur += notDone;
	}
	inline void prev() {
		cur -= cur != 0;
		notDone = (bool)end;
	}
	unsigned int end;
	unsigned int cur = 0;
	unsigned width;
	bool notDone = true;
};

Size::Iterator Size::iter() const noexcept { return Iterator(*this); }

bool Vector::widthInSize(Size size) const noexcept { return dx < size.w && dx >= 0 && dy < size.h && dy >= 0; }

struct FSize {
	inline FSize() noexcept : w(0), h(0) { }
	inline FSize(f_coordinate_t w, f_coordinate_t h) noexcept : w(w), h(h) { }
	inline Size snap() const noexcept;

	inline void extentToFit(FVector vector) noexcept {
		w = std::max(w, vector.dx + 1);
		h = std::max(h, vector.dy + 1);
	}
	inline std::string toString() const noexcept { return std::to_string(w) + "x" + std::to_string(h); }

	inline bool operator==(FSize other) const noexcept { return w == other.w && h == other.h; }
	inline bool operator!=(FSize other) const noexcept { return !operator==(other); }

	// w != 0 and h != 0
	inline bool isValid() const noexcept { return !(w && h); }

	inline f_coordinate_t area() const noexcept { return w * h; }
	inline f_coordinate_t perimeter() const noexcept { return w * 2 + h * 2; }

	// inline Iterator iter() const noexcept;

	f_coordinate_t w, h;
};

template <>
struct fmt::formatter<FSize> : fmt::formatter<std::string> {
	auto format(FSize v, format_context& ctx) const {
		return formatter<std::string>::format(v.toString(), ctx);
	}
};

// conversion
inline FVector Vector::free() const noexcept { return FVector(dx, dy); }
inline Vector FVector::snap() const noexcept { return Vector(std::floor(dx), std::floor(dy)); }
inline FSize Size::free() const noexcept { return FSize(w, h); }
inline Size FSize::snap() const noexcept { return Size(std::floor(w), std::floor(h)); }
inline FPosition Position::free() const noexcept { return FPosition(x, y); }
inline Position FPosition::snap() const noexcept { return Position(std::floor(x), std::floor(y)); }

// ---- we also define block rotation here so ----
enum Rotation : char
{
	ZERO = 0,
	NINETY = 1,
	ONE_EIGHTY = 2,
	TWO_SEVENTY = 3,
};

template <>
struct fmt::formatter<Rotation> : fmt::formatter<std::string> {
	auto format(Rotation v, format_context& ctx) const {
		switch (v) {
		case Rotation::TWO_SEVENTY: return "TWO_SEVENTY";
		case Rotation::ONE_EIGHTY: return "ONE_EIGHTY";
		case Rotation::NINETY: return "NINETY";
		default: return "ZERO";
		}
	}
};

inline Vector rotateVector(Vector vector, Rotation rotationAmount) noexcept {
	switch (rotationAmount) {
	case Rotation::TWO_SEVENTY: return Vector(vector.dy, -vector.dx);
	case Rotation::ONE_EIGHTY: return Vector(-vector.dx, -vector.dy);
	case Rotation::NINETY: return Vector(-vector.dy, vector.dx);
	default: return vector;
	}
}
inline FVector rotateVector(FVector vector, Rotation rotationAmount) noexcept {
	switch (rotationAmount) {
	case Rotation::TWO_SEVENTY: return FVector(vector.dy, -vector.dx);
	case Rotation::ONE_EIGHTY: return FVector(-vector.dx, -vector.dy);
	case Rotation::NINETY: return FVector(-vector.dy, vector.dx);
	default: return vector;
	}
}
inline Size rotateSize(Rotation rotationAmount, Size size) noexcept {
	if (rotationAmount & 1) return Size(size.h, size.w);
	return size;
}
inline FSize rotateSize(Rotation rotationAmount, FSize size) noexcept {
	if (rotationAmount & 1) return FSize(size.h, size.w);
	return size;
}
inline constexpr Rotation rotate(Rotation rotation, bool clockWise) {
	if (clockWise) {
		if (rotation == Rotation::TWO_SEVENTY) return Rotation::ZERO;
		return (Rotation)((int)rotation + 1);
	}
	if (rotation == Rotation::ZERO) return Rotation::TWO_SEVENTY;
	return (Rotation)((int)rotation - 1);
}
inline constexpr Rotation addRotations(Rotation rotationA, Rotation rotationB) {
	char output = rotationA + rotationB;
	if ((char)output > (char)Rotation::TWO_SEVENTY)
		output -= 4;
	return (Rotation)output;
}
inline constexpr Rotation rotationNeg(Rotation rotation) { return (Rotation)((4 - (char)rotation) & 0b11); }
inline constexpr Rotation subRotations(Rotation rotationA, Rotation rotationB) {
	return addRotations(rotationA, rotationNeg(rotationB));
}
inline constexpr int getDegrees(Rotation rotation) { return rotation * 90; }
inline Vector rotateVectorWithArea(Vector vector, Size size, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return Vector(size.h - vector.dy - 1, vector.dx);
	case Rotation::ONE_EIGHTY: return Vector(size.w - vector.dx - 1, size.h - vector.dy - 1);
	case Rotation::TWO_SEVENTY: return Vector(vector.dy, size.w - vector.dx - 1);
	default: return vector;
	}
}
inline Vector reverseRotateVectorWithArea(Vector vector, Size size, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return Vector(vector.dy, size.w - vector.dx - 1);
	case Rotation::ONE_EIGHTY: return Vector(size.w - vector.dx - 1, size.h - vector.dy - 1);
	case Rotation::TWO_SEVENTY: return Vector(size.h - vector.dy - 1, vector.dx);
	default: return vector;
	}
}
inline FVector rotateVectorWithArea(FVector vector, FSize size, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return FVector(size.h - vector.dy - 1.f, vector.dx);
	case Rotation::ONE_EIGHTY: return FVector(size.w - vector.dx - 1.f, size.h - vector.dy - 1.f);
	case Rotation::TWO_SEVENTY: return FVector(vector.dy, size.w - vector.dx - 1.f);
	default: return vector;
	}
}
inline FVector reverseRotateVectorWithArea(FVector vector, FSize size, Rotation rotationAmount) {
	switch (rotationAmount) {
	case Rotation::NINETY: return FVector(vector.dy, size.w - vector.dx - 1.f);
	case Rotation::ONE_EIGHTY: return FVector(size.w - vector.dx - 1.f, size.h - vector.dy - 1.f);
	case Rotation::TWO_SEVENTY: return FVector(size.h - vector.dy - 1.f, vector.dx);
	default: return vector;
	}
}

// change to 1 byte later
// flip then rotate
struct Orientation {
	Rotation rotation = Rotation::ZERO;
	bool flipped = false;

	Orientation(Rotation rotation = Rotation::ZERO, bool flipped = false) noexcept : rotation(rotation), flipped(flipped) { }

	inline std::string toString() const { return "(r:" + std::to_string(rotation) + ", f:" + std::to_string(flipped) + ")"; }

	inline void nextOrientation() {
		rotate(true);
		if (rotation == Rotation::ZERO) flip();
	}

	inline void lastOrientation() {
		if (rotation == Rotation::ZERO) flip();
		rotate(false);
	}

	inline Vector operator*(Vector vector) const noexcept {
		Vector vec(vector.dx, flipped ? -vector.dy : vector.dy);
		return rotateVector(vec, rotation);
	}
	inline FVector operator*(FVector vector) const noexcept {
		FVector vec(vector.dx, flipped ? -vector.dy : vector.dy);
		return rotateVector(vec, rotation);
	}
	inline Size operator*(Size size) const noexcept { return rotateSize(rotation, size); }
	inline FSize operator*(FSize size) const noexcept { return rotateSize(rotation, size); }
	inline bool operator==(Orientation other) const noexcept { return this->rotation == other.rotation && this->flipped == other.flipped; }
	inline bool operator!=(Orientation other) const noexcept { return !(*this == other); }
	inline void rotate(bool clockWise) { rotation = ::rotate(rotation, clockWise); }
	inline void flip() { flipped = !flipped; }
	inline Orientation operator*(Orientation other) const noexcept {
		return Orientation(addRotations(rotation, flipped ? rotationNeg(other.rotation) : other.rotation), other.flipped ^ flipped);
	}
	inline const Orientation& operator*=(Orientation other) noexcept {
		rotation = addRotations(rotation, flipped ? rotationNeg(other.rotation) : other.rotation);
		flipped ^= other.flipped;
		return *this;
	}
	inline Orientation inverse() const noexcept {
		return Orientation(flipped ? rotation : rotationNeg(rotation), flipped);
	}
	inline Orientation relativeTo(Orientation orientation) const noexcept {
		return (*this) * (orientation.inverse());
	}
	inline Vector transformVectorWithArea(Vector vector, Size size) const noexcept {
		Vector vec(vector.dx, flipped ? (size.h - vector.dy - 1) : vector.dy);
		return rotateVectorWithArea(vec, size, rotation);
	}
	inline Vector inverseTransformVectorWithArea(Vector vector, Size size) const noexcept {
		return inverse().transformVectorWithArea(vector, size);
		// Vector vec = reverseRotateVectorWithArea(vector, size, rotation);
		// return Vector(vec.dx, flipped ? (rotateSize(rotation, size).h - vec.dy - 1) : vec.dy);
	}
	inline FVector transformVectorWithArea(FVector vector, FSize size) const noexcept {
		FVector vec(vector.dx, flipped ? (size.h - vector.dy - 1.f) : vector.dy);
		return rotateVectorWithArea(vec, size, rotation);
	}
	inline FVector inverseTransformVectorWithArea(FVector vector, FSize size) const noexcept {
		return inverse().transformVectorWithArea(vector, size);
		// FVector vec = reverseRotateVectorWithArea(vector, size, rotation);
		// return FVector(vec.dx, flipped ? (rotateSize(rotation, size).h - vec.dy - 1) : vec.dy);
	}
};

#endif /* position_h */
