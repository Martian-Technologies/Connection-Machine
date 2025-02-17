#ifndef position_h
#define position_h

#include "util/fastMath.h"

typedef int cord_t;
typedef float f_cord_t;

struct Position;
struct FPosition;
struct Vector;
struct FVector;

struct Vector {
	inline Vector() : dx(0), dy(0) { }
	inline Vector(cord_t dx, cord_t dy) : dx(dx), dy(dy) { }
	inline FVector free() const;

	inline std::string toString() const { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ">"; }

	inline bool operator==(const Vector& other) const { return dx == other.dx && dy == other.dy; }
	inline bool operator!=(const Vector& other) const { return !operator==(other); }

	inline cord_t manhattenlength() const { return Abs(dx) + Abs(dy); }
	inline f_cord_t lengthSquared() const { return FastPower<2>(dx) + FastPower<2>(dy); }
	inline f_cord_t length() const { return sqrt(lengthSquared()); }

	inline Vector operator+(const Vector& other) const { return Vector(dx + other.dx, dy + other.dy); }
	inline Vector& operator+=(const Vector& other) { dx += other.dx; dy += other.dy; return *this; }
	inline Vector operator-(const Vector& other) const { return Vector(dx - other.dx, dy - other.dy); }
	inline Vector& operator-=(const Vector& other) { dx -= other.dx; dy -= other.dy; return *this; }
	inline cord_t operator*(const Vector& vector) const { return dx * vector.dx + dy * vector.dy; }
	inline Vector operator*(cord_t scalar) const { return Vector(dx * scalar, dy * scalar); }
	inline Vector& operator*=(cord_t scalar) { dx *= scalar; dy *= scalar; return *this; }
	inline Vector operator/(cord_t scalar) const { return Vector(dx / scalar, dy / scalar); }
	inline Vector& operator/=(cord_t scalar) { dx /= scalar; dy /= scalar; return *this; }

	cord_t dx, dy;
};

struct FVector {
	inline FVector() : dx(0.0f), dy(0.0f) { }
	inline FVector(f_cord_t dx, f_cord_t dy) : dx(dx), dy(dy) { }
	inline Vector snap() const;

	inline std::string toString() const { return "<" + std::to_string(dx) + ", " + std::to_string(dy) + ">"; }

	inline bool operator==(const FVector& other) const { return dx == other.dx && dy == other.dy; }
	inline bool operator!=(const FVector& other) const { return !operator==(other); }

	inline f_cord_t manhattenlength() const { return std::abs(dx) + std::abs(dy); }
	inline f_cord_t lengthSquared() const { return FastPower<2>(dx) + FastPower<2>(dy); }
	inline f_cord_t length() const { return sqrt(FastPower<2>(dx) + FastPower<2>(dy)); }

	inline FVector operator+(const FVector& other) const { return FVector(dx + other.dx, dy + other.dy); }
	inline FVector& operator+=(const FVector& other) { dx += other.dx; dy += other.dy; return *this; }
	inline FVector operator-(const FVector& other) const { return FVector(dx - other.dx, dy - other.dy); }
	inline FVector& operator-=(const FVector& other) { dx -= other.dx; dy -= other.dy; return *this; }
	inline FVector operator*(f_cord_t scalar) const { return FVector(dx * scalar, dy * scalar); }
	inline f_cord_t operator*(const FVector& vector) const { return dx * vector.dx + dy * vector.dy; }
	inline FVector operator/(f_cord_t scalar) { return FVector(dx / scalar, dy / scalar); }
	inline FVector& operator/=(f_cord_t scalar) { dx /= scalar; dy /= scalar; return *this; }

	inline f_cord_t lengthAlongProjectToVec(const FVector& vector) const { return (*this * vector) / vector.length(); }
	inline FVector projectToVec(const FVector& vector) const { return vector * (*this * vector) / vector.lengthSquared(); }

	f_cord_t dx, dy;
};

struct Position {
	inline Position() : x(0), y(0) { }
	inline Position(cord_t x, cord_t y) : x(x), y(y) { }
	inline FPosition free() const;

	inline std::string toString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
	
	inline bool operator==(const Position& position) const { return x == position.x && y == position.y; }
	inline bool operator!=(const Position& position) const { return !operator==(position); }
	inline bool withinArea(const Position& small, const Position& large) const { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }

	inline cord_t manhattenDistanceTo(const Position& position) const { return Abs(x - position.x) + Abs(y - position.y); }
	inline cord_t manhattenDistanceToOrigin() const { return Abs(x) + Abs(y); }
	inline cord_t distanceToSquared(const Position& position) const { return FastPower<2>(x - position.x) + FastPower<2>(y - position.y); }
	inline cord_t distanceToOriginSquared() const { return FastPower<2>(x) + FastPower<2>(y); }
	inline cord_t distanceTo(const Position& position) const { return sqrt(FastPower<2>(x - position.x) + FastPower<2>(y - position.y)); }
	inline cord_t distanceToOrigin() const { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }

	inline Position operator+(const Vector& vector) const { return Position(x + vector.dx, y + vector.dy); }
	inline Position& operator+=(const Vector& vector) { x += vector.dx; y += vector.dy; return *this; }
	inline Vector operator-(const Position& position) const { return Vector(x - position.x, y - position.y); }
	inline Position operator-(const Vector& vector) const { return Position(x - vector.dx, y - vector.dy); }
	inline Position& operator-=(const Vector& vector) { x -= vector.dx; y -= vector.dy; return *this; }

	cord_t x, y;
};

template<>
struct std::hash<Position> {
	inline std::size_t operator()(const Position& pos) const noexcept {
		std::size_t x = std::hash<cord_t> {}(pos.x);
		std::size_t y = std::hash<cord_t> {}(pos.y);
		return (std::size_t)x ^ ((std::size_t)y << 32);
	}
};

struct FPosition {
	inline FPosition() : x(0.0f), y(0.0f) { }
	inline FPosition(f_cord_t x, f_cord_t y) : x(x), y(y) { }
	inline Position snap() const;

	inline std::string toString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

	inline bool operator==(const FPosition& position) const { return x == position.x && y == position.y; }
	inline bool operator!=(const FPosition& position) const { return !operator==(position); }
	inline bool withinArea(const FPosition& small, const FPosition& large) const { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }

	inline f_cord_t manhattenDistanceTo(const FPosition& other) const { return std::abs(x - other.x) + std::abs(y - other.y); }
	inline f_cord_t manhattenDistanceToOrigin() const { return std::abs(x) + std::abs(y); }
	inline f_cord_t distanceToSquared(const FPosition& other) const { return FastPower<2>(x - other.x) + FastPower<2>(y - other.y); }
	inline f_cord_t distanceToOriginSquared() const { return FastPower<2>(x) + FastPower<2>(y); }
	inline f_cord_t distanceTo(const FPosition& other) const { return sqrt(FastPower<2>(x - other.x) + FastPower<2>(y - other.y)); }
	inline f_cord_t distanceToOrigin() const { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }
	
	inline FPosition operator+(const FVector& vector) const { return FPosition(x + vector.dx, y + vector.dy); }
	inline FPosition& operator+=(const FVector& vector) { x += vector.dx; y += vector.dy; return *this; }
	inline FVector operator-(const FPosition& position) const { return FVector(x - position.x, y - position.y); }
	inline FPosition operator-(const FVector& vector) const { return FPosition(x - vector.dx, y - vector.dy); }
	inline FPosition& operator-=(const FVector& vector) { x -= vector.dx; y -= vector.dy; return *this; }
	inline f_cord_t lengthAlongProjectToVec(const FPosition& orginOfVec, const FVector& vector) const { return (*this - orginOfVec).lengthAlongProjectToVec(vector); }
	inline FPosition projectToVec(const FPosition& orginOfVec, const FVector& vector) const { return orginOfVec + (*this - orginOfVec).projectToVec(vector); }

	f_cord_t x, y;
};

// conversion
inline FPosition Position::free() const { return FPosition(x, y); }
inline Position FPosition::snap() const { return Position(downwardFloor(x), downwardFloor(y)); }
inline FVector Vector::free() const { return FVector(dx, dy); }
inline Vector FVector::snap() const { return Vector(downwardFloor(dx), downwardFloor(dy)); }


// ---- we also define block rotation here so ----
enum Rotation : char {
	ZERO = 0,
	NINETY = 1,
	ONE_EIGHTY = 2,
	TWO_SEVENTY = 3,
};

inline constexpr bool isRotated(Rotation rotation) noexcept { return rotation & 1; }
inline constexpr bool isFlipped(Rotation rotation) noexcept { return rotation > 1; }
inline constexpr Rotation rotate(Rotation rotation, bool clockWise) {
	if (clockWise) {
		if (rotation == Rotation::TWO_SEVENTY) return Rotation::ZERO;
		return (Rotation)((int)rotation + 1);
	}
	if (rotation == Rotation::ZERO) return Rotation::TWO_SEVENTY;
	return (Rotation)((int)rotation - 1);
}
inline constexpr float getDegrees(Rotation rotation) {
	switch (rotation) {
	case Rotation::ZERO:
		return 0.0f;
		break;
	case Rotation::NINETY:
		return 90.0f;
		break;
	case Rotation::ONE_EIGHTY:
		return 180.0f;
		break;
	case Rotation::TWO_SEVENTY:
		return 270.0f;
		break;
	}
	return 0.0f;
}
#endif /* position_h */
