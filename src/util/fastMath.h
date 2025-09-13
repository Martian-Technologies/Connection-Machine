#ifndef fastMath_h
#define fastMath_h

template <unsigned int P, class T>
constexpr T FastPower(T x) {
	if constexpr (P == 0) return 1;
	if constexpr (P == 1) return x;

	T tmp = FastPower<P / 2>(x);
	if constexpr ((P % 2) == 0) { return tmp * tmp; } else { return x * tmp * tmp; }
}

constexpr unsigned int Abs(unsigned int x) { assert(false || "WHY DO YOU DO THIS?????"); return x; }

constexpr int max(int a, int b) { return a < b ? b : a; }
constexpr int min(int a, int b) { return a < b ? a : b; }

constexpr int Abs(int x) { return x < 0 ? -x : x; }

constexpr double Abs(double x) {
	union { double f; uint64_t i; } u = { x };
	u.i &= 0x7fffffff;
	return u.f;
}

constexpr float Abs(float x) {
	union { float f; uint32_t i; } u = { x };
	u.i &= 0x7fffffff;
	return u.f;
}

template <typename T>
constexpr char signum(T x) {
	if constexpr (std::is_signed<T>())
		return (T(0) < x) - (x < T(0));
	return T(0) < x;
}

inline float decPart(float x) { return (float)signum(x) * fmodf(Abs(x), 1.f); }

inline bool approx_equals(float a, float b) {
	return Abs(a - b) <= nexttowardf(std::max(a, b), HUGE_VALL) - std::max(a, b);
}

#endif /* fastMath_h */
