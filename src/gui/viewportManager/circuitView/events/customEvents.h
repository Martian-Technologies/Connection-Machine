#ifndef customEvents_h
#define customEvents_h

#include "backend/position/position.h"
#include "backend/evaluator/simulator/logicState.h"
#include "util/vec2.h"
#include "event.h"

class ViewPositionEvent : public Event {
public:
	inline ViewPositionEvent(const std::string& name, const Vec2& viewPosition) : Event(name), viewPosition(viewPosition) { }

	inline Vec2 getPosition() const { return viewPosition; }

private:
	Vec2 viewPosition;
};

class PositionEvent : public Event {
public:
	inline PositionEvent(const std::string& name, const FPosition& position) : Event(name), position(position) { }

	inline Position getPosition() const { return position.snap(); }
	inline const FPosition& getFPosition() const { return position; }

private:
	FPosition position;
};

class VectorEvent : public Event {
public:
	inline VectorEvent(const std::string& name, const FVector& vector) : Event(name), vector(vector) { }

	inline Vector getVector() const { return vector.snap(); }
	inline const FVector& getFVector() const { return vector; }

private:
	FVector vector;
};

class DeltaEvent : public Event {
public:
	inline DeltaEvent(const std::string& name, float delta) : Event(name), delta(delta) { }

	inline float getDelta() const { return delta; }

private:
	float delta;
};

class DeltaXYEvent : public Event {
public:
	inline DeltaXYEvent(const std::string& name, float deltaX, float deltaY) : Event(name), deltaX(deltaX), deltaY(deltaY) { }

	inline float getDeltaX() const { return deltaX; }
	inline float getDeltaY() const { return deltaY; }

private:
	float deltaX;
	float deltaY;
};

class StateSetEvent : public Event {
public:
	inline StateSetEvent(const std::string& name, const Position& position, logic_state_t state) : Event(name), position(position), state(state) { }

	inline Position getPosition() const { return position; }
	inline logic_state_t getState() const { return state; }

private:
	logic_state_t state;
	Position position;
};

#endif /* customEvents_h */
