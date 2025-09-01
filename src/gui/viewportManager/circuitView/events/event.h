#ifndef event_h
#define event_h

template <class T>
class EventWithValue;
class Event {
public:
	Event(const std::string& name) : name(name) { }
	virtual ~Event() = default;

	const std::string& getName() const { return name; }

	template<class EventType>
	inline EventType* cast() { return dynamic_cast<EventType*>(*this); }
	template<class EventType>
	inline const EventType* cast() const { return dynamic_cast<const EventType*>(this); }

	template<class ValueType>
	inline EventWithValue<ValueType>* cast2() { return dynamic_cast<EventWithValue<ValueType>*>(*this); }
	template<class ValueType>
	inline const EventWithValue<ValueType>* cast2() const { return dynamic_cast<const EventWithValue<ValueType>*>(this); }

private:
	std::string name;
};

template <class T>
class EventWithValue : public Event {
public:
	EventWithValue(const std::string& name, const T& value) : Event(name), value(value) { }
	inline const T& operator*() const { return value; }
	inline const T* operator->() const { return &value; }
	inline const T& get() const { return value; }
private:
	T value;
};

#endif /* event_h */
