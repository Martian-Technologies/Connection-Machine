#ifndef movingAverage_h
#define movingAverage_h

template <class T, size_t WindowSize>
class MovingAverage {
public:
    MovingAverage() : sum(0), index(0), count(0) {
        data.fill(T{});
    }

    // Add a new value and get the current moving average
    void next(const T& val) {
        if (count < WindowSize) {
            sum += val;
            data[index] = val;
            ++count;
        } else {
            sum -= data[index];
            sum += val;
            data[index] = val;
        }

        index = (index + 1) % WindowSize;
    }

    // Get the current average without adding a new value
    double getAverage() const {
        if (count == 0) return 0.0;
        return (double)sum / (double)count;
    }

    // Clear the moving average
    void clear() {
        data.fill(T{});
        sum = T{};
        index = 0;
        count = 0;
    }

    // Get current number of elements in the window
    size_t size() const {
        return count;
    }

    // Get maximum window size
    constexpr size_t getMaxSize() const {
        return WindowSize;
    }

private:
    std::array<T, WindowSize> data;
    T sum;
    size_t index;
    size_t count;
};

#endif /* movingAverage_h */