#ifndef threadPool_h
#define threadPool_h

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

class ThreadPool {
public:
	explicit ThreadPool(size_t nthreads = 0)
		: stop(false)
	{
		workers.reserve(nthreads);
		for (size_t i = 0; i < nthreads; ++i) spawnOne();
	}

	~ThreadPool() {
		// Ensure all jobs have fully finished (not just claimed)
		waitForCompletion();
		stop.store(true, std::memory_order_relaxed);
		for (auto& w : workers) w->retire.store(true, std::memory_order_relaxed);
		cv.notify_all(); // wake any sleepers
		for (auto& w : workers) if (w->th.joinable()) w->th.join();
	}

	struct Job {
		void (*fn)(void*);
		void* arg;
	};

	// Load a new round that references the caller-owned jobs (no copy/move).
	void resetAndLoad(const std::vector<Job>& new_jobs) {
		{
			std::lock_guard lk(mtx);
			jobsRef = &new_jobs;
			next.store(0, std::memory_order_relaxed);
			completed.store(0, std::memory_order_relaxed);
			end.store(static_cast<uint32_t>(new_jobs.size()), std::memory_order_release);
			round.fetch_add(1, std::memory_order_release);
		}
		cv.notify_all();
	}

	void waitForEmpty() {
		bool sprintingNow = sprinting.load(std::memory_order_acquire);
		while (true) {
			if (tryRunOne()) continue;
			uint32_t n = next.load(std::memory_order_acquire);
			uint32_t e = end.load(std::memory_order_acquire);
			if (n >= e) break;
			if (!sprintingNow) { std::this_thread::yield(); }
		}
	}

	void waitForCompletion() {
		bool sprintingNow = sprinting.load(std::memory_order_acquire);
#ifdef TRACY_PROFILER
		ZoneScoped;
#endif
		while (true) {
			uint32_t c = completed.load(std::memory_order_acquire);
			uint32_t e = end.load(std::memory_order_acquire);
			if (c >= e) break;
			if (tryRunOne()) continue;
			if (!sprintingNow) { std::this_thread::yield(); }
		}
	}

	void resizeThreads(size_t new_count) {
		size_t cur = workers.size();
		if (new_count > cur) {
			size_t add = new_count - cur;
			workers.reserve(workers.size() + add);
			for (size_t i = 0; i < add; ++i) spawnOne();
			return;
		}
		if (new_count < cur) {
			size_t kill = cur - new_count;
			waitForCompletion();
			for (size_t i = 0; i < kill; ++i)
				workers[workers.size() - 1 - i]->retire.store(true, std::memory_order_relaxed);
			cv.notify_all(); // wake sleepers so they can retire
			for (size_t i = 0; i < kill; ++i) {
				auto idx = workers.size() - 1;
				auto& w = workers[idx];
				if (w->th.joinable()) w->th.join();
				workers.pop_back();
			}
		}
	}

	size_t threadCount() const { return workers.size(); }

	void setSprinting(bool sprint) {
		sprinting.store(sprint, std::memory_order_release);
		// logInfo("ThreadPool: sprinting mode {}", "ThreadPool::setSprinting", sprint ? "enabled" : "disabled");
	}

private:
	struct Worker {
		std::thread th;
		std::atomic<bool> retire{false};
	};

	void spawnOne() {
		auto w = std::make_unique<Worker>();
		Worker* self = w.get();
		w->th = std::thread([this, self]{ workerLoop(self); });
		workers.emplace_back(std::move(w));
	}

	void workerLoop(Worker* self) {
		uint64_t local_round = round.load(std::memory_order_acquire);
		while (true) {
			if (tryRunOne()) continue;

			if (sprinting.load(std::memory_order_acquire)) {
				while (true) {
					if (self->retire.load(std::memory_order_relaxed)
						|| stop.load(std::memory_order_relaxed)
						|| round.load(std::memory_order_acquire) != local_round) {
						break;
					}
				}
			} else {
				std::unique_lock lk(mtx);
				cv.wait(lk, [this, self, &local_round]{
					return self->retire.load(std::memory_order_relaxed)
					   || stop.load(std::memory_order_relaxed)
					   || round.load(std::memory_order_acquire) != local_round;
				});
			}
			if (self->retire.load(std::memory_order_relaxed) ||
				stop.load(std::memory_order_relaxed)) {
				return;
			}
			local_round = round.load(std::memory_order_acquire);
		}
	}

	inline bool tryRunOne() {
		uint32_t i = next.fetch_add(1, std::memory_order_acq_rel);
		uint32_t e = end.load(std::memory_order_acquire);
		if (i < e) {
#ifdef TRACY_PROFILER
			ZoneScoped;
#endif
			// Safe because resetAndLoad keeps jobsRef stable for the duration of the round.
			const Job j = (*jobsRef)[i];
			j.fn(j.arg);
			completed.fetch_add(1, std::memory_order_acq_rel);
			return true;
		}
		return false;
	}

	std::vector<std::unique_ptr<Worker>> workers;
	const std::vector<Job>* jobsRef{nullptr}; // reference to current round’s jobs
	std::atomic<uint32_t> next{0}; // next index to claim
	std::atomic<uint32_t> end{0}; // jobsRef->size()
	std::atomic<uint32_t> completed{0}; // number of jobs fully executed
	std::atomic<bool> stop{false}; // global shutdown (idle-only)
	std::atomic<bool> sprinting{false}; // will make waiting for completion not yield
	std::atomic<uint64_t> round{0}; // generation/epoch for cv wakeups

	std::mutex mtx;
	std::condition_variable cv;
};

#endif /* threadPool_h */
