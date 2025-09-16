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
	void resetAndLoad(const std::vector<std::vector<Job>>& new_jobs) {
		{
			std::lock_guard lk(mtx);
			jobsRef = &new_jobs;

			threadsWaiting.store(0, std::memory_order_relaxed);
			next.resize((*jobsRef).size());
			for (auto& index : next) {
				if (!index) index = std::make_shared<std::atomic<unsigned int>>(0);
				else index->store(0, std::memory_order_relaxed);
			}
			round.fetch_add(1, std::memory_order_release);
		}
		cv.notify_all();
	}

	void waitForCompletion() {
		bool sprintingNow = sprinting.load(std::memory_order_acquire);
#ifdef TRACY_PROFILER
		ZoneScoped;
#endif
		if (jobsRef != nullptr && (*jobsRef).size() != 0)
			runTillDone((*jobsRef).size()-1); // if your waiting might as well help do the compute
		uint32_t w = threadsWaiting.fetch_add(1, std::memory_order_acq_rel);
		while (true) {
			if (w >= workers.size()+1) break;
			if (!sprintingNow) { std::this_thread::yield(); }
			w = threadsWaiting.load(std::memory_order_acquire);
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
		unsigned int threadIndex;
	};

	void spawnOne() {
		auto w = std::make_unique<Worker>();
		Worker* self = w.get();
		w->threadIndex = workers.size();
		w->th = std::thread([this, self]{ workerLoop(self); });
		workers.emplace_back(std::move(w));
	}

	void workerLoop(Worker* self) {
		uint64_t local_round = round.load(std::memory_order_acquire);
		while (true) {
			threadsWaiting.fetch_add(1, std::memory_order_acq_rel);
			if (sprinting.load(std::memory_order_acquire)) {
				while (true) {
					if (self->retire.load(std::memory_order_relaxed) || stop.load(std::memory_order_relaxed)) {
						return;
					}
					uint64_t cur_round = round.load(std::memory_order_acquire);
					if (cur_round != local_round) {
						local_round = cur_round;
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
				if (self->retire.load(std::memory_order_relaxed) || stop.load(std::memory_order_relaxed)) {
					return;
				}
				local_round = round.load(std::memory_order_acquire);
			}
			runTillDone(self->threadIndex);
		}
	}

	inline void runTillDone(unsigned int threadIndex) {
		unsigned int jobArrayIndex = threadIndex;
		auto* ptr = next[jobArrayIndex].get();
		uint32_t i = ptr->fetch_add(1, std::memory_order_acq_rel);
		while (true) {
#ifdef TRACY_PROFILER
			ZoneScoped;
#endif
			while (i >= (*jobsRef)[jobArrayIndex].size()) {
				uint32_t w = threadsWaiting.load(std::memory_order_acquire);
				if (w > 0 || (jobArrayIndex + 1)%(*jobsRef).size() == threadIndex) return;
				jobArrayIndex = (jobArrayIndex + 1) % (*jobsRef).size();
				i = next[jobArrayIndex]->fetch_add(1, std::memory_order_acq_rel);
			}
			// Safe because resetAndLoad keeps jobsRef stable for the duration of the round.

			const Job j = (*jobsRef)[jobArrayIndex][i];
			j.fn(j.arg);
			i = next[jobArrayIndex]->fetch_add(1, std::memory_order_acq_rel);
		}
	}

	std::vector<std::unique_ptr<Worker>> workers;
	const std::vector<std::vector<Job>>* jobsRef{nullptr}; // reference to current round’s jobs
	std::vector<std::shared_ptr<std::atomic<uint32_t>>> next; // next index to claim
	std::atomic<bool> stop{false}; // global shutdown (idle-only)
	std::atomic<bool> sprinting{false}; // will make waiting for completion not yield
	std::atomic<uint64_t> round{0}; // generation/epoch for cv wakeups
	std::atomic<uint32_t> threadsWaiting{0}; // generation/epoch for cv wakeups

	std::mutex mtx;
	std::condition_variable cv;
};

#endif /* threadPool_h */
