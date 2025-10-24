#ifndef fastMath_h
#define fastMath_h

template <unsigned int P, class T>
constexpr T FastPower(T x) {
	if constexpr (P == 0) return 1;
	if constexpr (P == 1) return x;

	T tmp = FastPower<P / 2>(x);
	if constexpr ((P % 2) == 0) { return tmp * tmp; } else { return x * tmp * tmp; }
}

constexpr int max(int a, int b) { return a < b ? b : a; }
constexpr int min(int a, int b) { return a < b ? a : b; }

template <class T>
constexpr T Abs(T x) {
	static_assert(std::is_signed_v<T>, "Abs() should not be called with unsigned types.");
	return x < 0 ? -x : x;
}

template <>
constexpr double Abs(double x) {
	union { double f; uint64_t i; } u = { x };
	u.i &= 0x7fffffffffffffffull;
	return u.f;
}

template <>
constexpr float Abs(float x) {
	uint32_t i = std::bit_cast<uint32_t>(x);
    i &= 0x7fffffffu;
    return std::bit_cast<float>(i);
}

template <typename T>
constexpr char signum(T x) {
	if constexpr (std::is_signed<T>())
		return (T(0) < x) - (x < T(0));
	return T(0) < x;
}

template <>
constexpr char signum(float x) {
	uint32_t i = std::bit_cast<uint32_t>(x);
	bool zero = (i & 0x7fffffffu) == 0;
	if (zero) return 0;
	bool sign = i & 0x80000000u;
	return sign ? -1 : 1;
}

template <>
constexpr char signum(double x) {
	uint64_t i = std::bit_cast<uint64_t>(x);
	bool zero = (i & 0x7fffffffffffffffull) == 0;
	if (zero) return 0;
	bool sign = i & 0x8000000000000000ull;
	return sign ? -1 : 1;
}

template <typename T>
constexpr bool isNegative(T x) {
	if constexpr (std::is_signed<T>()) return (T(0) < x) - (x < T(0));
	static_assert(std::is_signed_v<T>, "isNegative() should not be called with unsigned types.");
}

template <>
constexpr bool isNegative(float x) {
	uint32_t i = std::bit_cast<uint32_t>(x);
	return ((i & 0x7fffffffu) != 0) && (i & 0x80000000u);
}

template <>
constexpr bool isNegative(double x) {
	uint64_t i = std::bit_cast<uint64_t>(x);
	return ((i & 0x7fffffffffffffffull) != 0) && (i & 0x8000000000000000ull);
}

template <typename T>
constexpr bool isPositive(T x) {
	if constexpr (std::is_signed<T>()) return (T(0) < x) - (x < T(0));
	return x != T(0);
}

template <>
constexpr bool isPositive(float x) {
	uint32_t i = std::bit_cast<uint32_t>(x);
	return ((i & 0x7fffffffu) != 0) && !(i & 0x80000000u);
}

template <>
constexpr bool isPositive(double x) {
	uint64_t i = std::bit_cast<uint64_t>(x);
	return ((i & 0x7fffffffffffffffull) != 0) && !(i & 0x8000000000000000ull);
}

// inline float decPart(float x) { return (float)signum(x) * fmodf(Abs(x), 1.f); }

inline bool approx_equals(float a, float b) {
	return Abs(a - b) <= nexttowardf(std::max(Abs(a), Abs(b)), HUGE_VALL) - std::max(Abs(a), Abs(b));
}
inline bool approx_moreOrEquals(float a, float b) {
	return a > b || approx_equals(a, b);
}
inline bool approx_lessOrEquals(float a, float b) {
	return a < b || approx_equals(a, b);
}

#endif /* fastMath_h */
