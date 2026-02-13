/**
 * @file executor.cpp
 * @brief Work-stealing thread pool implementation
 */

#include "executor.hpp"
#include <algorithm>
#include <random>
#include <chrono>

namespace moltcat::core {

// ========================================================================
// Time utility
// ========================================================================

namespace {

[[nodiscard]] auto get_time_ms() -> uint64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

// ========================================================================
// Executor::Worker - Worker thread implementation
// ========================================================================

/**
 * @brief Worker thread with local deque for work stealing
 *
 * Each worker has:
 * - A local deque for tasks (LIFO for own tasks, FIFO for stealing)
 * - A random number generator for selecting victim to steal from
 * - Thread state tracking
 */
class Executor::Worker {
public:
    /**
     * @brief Worker state
     */
    enum class State : uint8_t {
        IDLE,       ///< Waiting for work
        RUNNING,    ///< Processing tasks
        STOPPING,   ///< Shutting down
        STOPPED     ///< Terminated
    };

    /**
     * @brief Construct worker
     * @param executor Parent executor
     * @param index Worker index (0-based)
     */
    Worker(Executor& executor, size_t index)
        : executor_(executor)
        , index_(index)
        , state_(State::IDLE)
        , random_(std::random_device{}()) {}

    /**
     * @brief Start worker thread
     */
    auto start() -> void {
        thread_ = std::thread([this] { run(); });
    }

    /**
     * @brief Stop worker thread
     */
    auto stop() -> void {
        {
            std::lock_guard lock(state_mutex_);
            state_ = State::STOPPING;
        }
        state_cv_.notify_all();
    }

    /**
     * @brief Wait for worker to finish
     */
    auto join() -> void {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    /**
     * @brief Get worker state
     */
    [[nodiscard]] auto get_state() const noexcept -> State {
        std::lock_guard lock(state_mutex_);
        return state_;
    }

    /**
     * @brief Push task to local queue (called by owner)
     * @param task Task to push
     */
    auto push(Task&& task) -> void {
        {
            std::lock_guard lock(queue_mutex_);
            local_queue_.push_back(std::move(task));
        }
        queue_cv_.notify_one();
    }

    /**
     * @brief Try to steal task from this worker
     * @param[out] task Stolen task
     * @return true if task was stolen
     */
    auto try_steal(Task& task) -> bool {
        std::lock_guard lock(queue_mutex_);
        if (local_queue_.empty()) {
            return false;
        }
        // Steal from front (FIFO) to maintain cache locality
        task = std::move(local_queue_.front());
        local_queue_.pop_front();
        return true;
    }

    /**
     * @brief Get approximate local queue size
     */
    [[nodiscard]] auto local_queue_size() const noexcept -> size_t {
        std::lock_guard lock(queue_mutex_);
        return local_queue_.size();
    }

private:
    /**
     * @brief Main worker loop
     */
    auto run() -> void {
        set_state(State::RUNNING);

        while (true) {
            // Check if we should stop
            if (get_state() == State::STOPPING) {
                break;
            }

            // Try to get a task
            Task task;
            if (try_get_task(task)) {
                // Execute task
                if (task.is_valid()) {
                    task();
                }
            } else {
                // No task available, wait
                std::unique_lock lock(state_mutex_);
                if (state_ == State::STOPPING) {
                    break;
                }
                state_ = State::IDLE;
                state_cv_.wait_for(lock, std::chrono::milliseconds(100));
                if (state_ != State::RUNNING) {
                    break;
                }
            }
        }

        set_state(State::STOPPED);
    }

    /**
     * @brief Try to get a task from various sources
     * @param[out] task Retrieved task
     * @return true if task was obtained
     */
    auto try_get_task(Task& task) -> bool {
        // 1. Try local queue (LIFO - most recently added)
        if (try_get_local_task(task)) {
            return true;
        }

        // 2. Try global queue (priority based)
        if (executor_.try_get_global_task(task)) {
            return true;
        }

        // 3. Try stealing from other workers
        if (try_steal_task(task)) {
            return true;
        }

        return false;
    }

    /**
     * @brief Try to get task from local queue
     */
    auto try_get_local_task(Task& task) -> bool {
        std::lock_guard lock(queue_mutex_);
        if (local_queue_.empty()) {
            return false;
        }
        task = std::move(local_queue_.back());
        local_queue_.pop_back();
        return true;
    }

    /**
     * @brief Try to steal from another worker
     */
    auto try_steal_task(Task& task) -> bool {
        // Pick random victim
        size_t victim_index = random_() % executor_.num_workers();

        // Don't steal from self
        if (victim_index == index_) {
            return false;
        }

        return executor_.try_steal_from(victim_index, task);
    }

    /**
     * @brief Set worker state
     */
    auto set_state(State new_state) -> void {
        {
            std::lock_guard lock(state_mutex_);
            state_ = new_state;
        }
        state_cv_.notify_all();
    }

    Executor& executor_;           ///< Parent executor
    size_t index_;                  ///< Worker index

    // Local task queue (deque for efficient push/pop from both ends)
    std::deque<Task> local_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Worker state
    State state_;
    mutable std::mutex state_mutex_;
    std::condition_variable state_cv_;

    // Random number generator for work stealing
    std::mt19937 random_;

    // Worker thread
    std::thread thread_;
};

// ========================================================================
// Executor::Impl - Internal implementation
// ========================================================================

/**
 * @brief Internal implementation details
 */
class Executor::Impl {
public:
    explicit Impl(size_t num_threads)
        : running_(true)
        , num_workers_(num_threads > 0 ? num_threads : std::thread::hardware_concurrency())
        , active_workers_(0) {}

    // Global priority queue
    std::priority_queue<Task, std::vector<Task>, TaskComparator> global_queue_;
    mutable std::mutex global_queue_mutex_;
    std::condition_variable global_queue_cv_;

    // Workers
    std::vector<std::unique_ptr<Worker>> workers_;

    // Executor state
    std::atomic<bool> running_;
    size_t num_workers_;
    std::atomic<size_t> active_workers_;
    std::atomic<size_t> pending_tasks_{0};

    // Wait synchronization
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    std::atomic<size_t> tasks_completed_{0};
    std::atomic<size_t> tasks_submitted_{0};
};

// ========================================================================
// Executor implementation
// ========================================================================

Executor::Executor(size_t num_threads)
    : impl_(std::make_unique<Impl>(num_threads)) {
    // Create worker threads
    impl_->workers_.reserve(impl_->num_workers_);
    for (size_t i = 0; i < impl_->num_workers_; ++i) {
        auto worker = std::make_unique<Worker>(*this, i);
        worker->start();
        impl_->workers_.push_back(std::move(worker));
    }
}

Executor::~Executor() {
    stop();
}

auto Executor::submit(Task&& task) -> void {
    if (!impl_->running_.load(std::memory_order_acquire)) {
        return;  // Executor is stopping
    }

    // Set enqueue time for FIFO ordering within same priority
    if (task.enqueue_time == 0) {
        task.enqueue_time = get_time_ms();
    }

    // If task has affinity, route directly to that worker
    if (task.affinity >= 0 && static_cast<size_t>(task.affinity) < impl_->num_workers_) {
        impl_->workers_[task.affinity]->push(std::move(task));
        impl_->tasks_submitted_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Otherwise, add to global queue
    {
        std::lock_guard lock(impl_->global_queue_mutex_);
        impl_->global_queue_.push(std::move(task));
    }
    impl_->global_queue_cv_.notify_one();

    impl_->tasks_submitted_.fetch_add(1, std::memory_order_relaxed);
    impl_->pending_tasks_.fetch_add(1, std::memory_order_relaxed);
}

auto Executor::submit(Task::Func&& func) -> void {
    submit(Task(std::move(func)));
}

auto Executor::submit_priority(Task::Func&& func, Task::Priority priority) -> void {
    submit(Task(std::move(func), TaskType::PRIORITIZED, priority));
}

auto Executor::submit_affinity(Task&& task, size_t thread_index) -> void {
    task.affinity = static_cast<int>(thread_index);
    submit(std::move(task));
}

auto Executor::submit_affinity(Task::Func&& func, size_t thread_index) -> void {
    Task task(std::move(func));
    task.affinity = static_cast<int>(thread_index);
    submit(std::move(task));
}

[[nodiscard]] auto Executor::num_workers() const noexcept -> size_t {
    return impl_->workers_.size();
}

[[nodiscard]] auto Executor::pending_tasks() const noexcept -> size_t {
    // Count tasks in global queue + local queues
    size_t count = 0;

    {
        std::lock_guard lock(impl_->global_queue_mutex_);
        count += impl_->global_queue_.size();
    }

    for (const auto& worker : impl_->workers_) {
        count += worker->local_queue_size();
    }

    return count;
}

auto Executor::wait() -> void {
    std::unique_lock lock(impl_->wait_mutex_);

    impl_->wait_cv_.wait(lock, [this] {
        const auto submitted = impl_->tasks_submitted_.load(std::memory_order_relaxed);
        const auto completed = impl_->tasks_completed_.load(std::memory_order_relaxed);
        return completed >= submitted;
    });
}

auto Executor::stop() -> void {
    if (!impl_->running_.exchange(false, std::memory_order_acq_rel)) {
        return;  // Already stopped
    }

    // Wake up all workers
    impl_->global_queue_cv_.notify_all();

    // Stop all workers
    for (auto& worker : impl_->workers_) {
        worker->stop();
    }

    // Wait for all workers to finish
    for (auto& worker : impl_->workers_) {
        worker->join();
    }

    impl_->workers_.clear();

    // Notify waiting threads
    impl_->wait_cv_.notify_all();
}

// ========================================================================
// Internal helper methods (called by Worker)
// ========================================================================

auto Executor::try_get_global_task(Task& task) -> bool {
    std::lock_guard lock(impl_->global_queue_mutex_);
    if (impl_->global_queue_.empty()) {
        return false;
    }

    task = std::move(const_cast<Task&>(impl_->global_queue_.top()));
    impl_->global_queue_.pop();

    impl_->pending_tasks_.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

auto Executor::try_steal_from(size_t victim_index, Task& task) -> bool {
    if (victim_index >= impl_->workers_.size()) {
        return false;
    }

    return impl_->workers_[victim_index]->try_steal(task);
}

auto Executor::notify_task_completed() -> void {
    impl_->tasks_completed_.fetch_add(1, std::memory_order_relaxed);
    impl_->wait_cv_.notify_all();
}

} // namespace moltcat::core
