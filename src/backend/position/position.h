#ifndef position_h
#define position_h

#include "util/fastMath.h"

typedef int cord_t;

struct Position;
struct FPosition;

struct Position {
    inline Position() : x(0), y(0) {}
    inline Position(cord_t x, cord_t y) : x(x), y(y) {}

    inline cord_t manhattenDistanceTo(const Position& other) const { return Abs(x - other.x) + Abs(y - other.y); }
    inline cord_t manhattenDistanceToOrigin() const { return Abs(x) + Abs(y); }
    inline cord_t distanceToSquared(const Position& other) const { return FastPower<2>(x - other.x) + FastPower<2>(y - other.y); }
    inline cord_t distanceToOriginSquared() const { return FastPower<2>(x) + FastPower<2>(y); }
    inline cord_t distanceTo(const Position& other) const { return sqrt(FastPower<2>(x - other.x) + FastPower<2>(y - other.y)); }
    inline cord_t distanceToOrigin() const { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }
    bool withinArea(const Position& small, const Position& large) const { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }
    FPosition free() const;
    inline bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    inline bool operator!=(const Position& other) const { return !operator==(other); }
    inline Position operator+(const Position& position) const { return Position(x + position.x, y + position.y); }
    inline Position operator-(const Position& position) const { return Position(x - position.x, y - position.y); }
    inline Position operator*(cord_t scalar) const { return Position(x * scalar, y * scalar); }
    inline Position& operator+=(const Position& position) { x += position.x; y += position.y; return *this; }
    inline Position& operator-=(const Position& position) { x -= position.x; y -= position.y; return *this; }
    inline Position& operator/=(cord_t scalar) { x /= scalar; y /= scalar; return *this; }
    inline std::string toString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

    cord_t x, y;
};

template<>
struct std::hash<Position> {
    inline std::size_t operator()(const Position& pos) const noexcept {
        std::size_t x = std::hash<cord_t>{}(pos.x);
        std::size_t y = std::hash<cord_t>{}(pos.y);
        return (std::size_t)x ^ ((std::size_t)y << 32);
    }
};

typedef float f_cord_t;

struct FPosition {
    inline FPosition() : x(0.0f), y(0.0f) {}
    inline FPosition(f_cord_t x, f_cord_t y) : x(x), y(y) {}

    inline f_cord_t manhattenDistanceTo(const FPosition& other) const { return std::abs(x - other.x) + std::abs(y - other.y); }
    inline f_cord_t manhattenDistanceToOrigin() const { return std::abs(x) + std::abs(y); }
    inline f_cord_t distanceToSquared(const FPosition& other) const { return FastPower<2>(x - other.x) + FastPower<2>(y - other.y); }
    inline f_cord_t distanceToOriginSquared() const { return FastPower<2>(x) + FastPower<2>(y); }
    inline f_cord_t distanceTo(const FPosition& other) const { return sqrt(FastPower<2>(x - other.x) + FastPower<2>(y - other.y)); }
    inline f_cord_t distanceToOrigin() const { return sqrt(FastPower<2>(x) + FastPower<2>(y)); }
    bool withinArea(const FPosition& small, const FPosition& large) const { return small.x <= x && small.y <= y && large.x >= x && large.y >= y; }
    Position snap() const;
    inline bool operator==(const FPosition& other) const { return x == other.x && y == other.y; }
    inline bool operator!=(const FPosition& other) const { return !operator==(other); }
    inline FPosition operator+(const FPosition& position) const { return FPosition(x + position.x, y + position.y); }
    inline FPosition operator-(const FPosition& position) const { return FPosition(x - position.x, y - position.y); }
    inline FPosition operator*(f_cord_t scalar) const { return FPosition(x * scalar, y * scalar); }
    inline FPosition& operator+=(const FPosition& position) { x += position.x; y += position.y; return *this; }
    inline FPosition& operator-=(const FPosition& position) { x -= position.x; y -= position.y; return *this; }
    inline FPosition& operator/=(f_cord_t scalar) { x /= scalar; y /= scalar; return *this; }
    inline std::string toString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
    inline f_cord_t lengthAlongProjectToVec(const FPosition& orginOfVec, const FPosition& endOfVec) const {
        FPosition vec = (endOfVec - orginOfVec);
        return ((this->x - orginOfVec.x) * vec.x + (this->y - orginOfVec.y) * vec.y) / vec.distanceToOrigin();
    }
    inline FPosition projectToVec(const FPosition& orginOfVec, const FPosition& endOfVec) const {
        FPosition vec = endOfVec - orginOfVec;
        return  vec * (((this->x - orginOfVec.x) * vec.x + (this->y - orginOfVec.y) * vec.y) / vec.distanceToOriginSquared());
    }

    f_cord_t x, y;
};

// conversion
inline FPosition Position::free() const { return FPosition(x, y); }
inline Position FPosition::snap() const { return Position(downwardFloor(x), downwardFloor(y)); }

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
        if (rotation == TWO_SEVENTY) return ZERO;
        return (Rotation)((int)rotation + 1);
    }
    if (rotation == ZERO) return TWO_SEVENTY;
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
