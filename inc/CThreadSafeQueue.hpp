
#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP
#include <queue>
#include <mutex>
/**
 * @class ThreadSafeQueue
 * @brief A thread-safe queue implementation using a mutex and condition variable.
 *
 * This class provides a thread-safe queue that allows multiple threads to push and pop elements
 * concurrently. It ensures that access to the queue is synchronized using a mutex and condition variable.
 *
 * @tparam T The type of elements stored in the queue.
 */

template <typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Pushes a value into the queue.
     *
     * This method locks the mutex, pushes the value into the queue, and notifies one waiting thread.
     *
     * @param value The value to be pushed into the queue.
     */
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_var_.notify_one();
    }

    /**
     * @brief Pops a value from the queue.
     *
     * This method locks the mutex and waits until the queue is not empty. It then pops the front value
     * from the queue and assigns it to the provided reference.
     *
     * @param value Reference to the variable where the popped value will be stored.
     * @return true if a value was successfully popped from the queue.
     */
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); });
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    /**
     * @brief Checks if the queue is empty.
     *
     * This method locks the mutex and checks if the queue is empty.
     *
     * @return true if the queue is empty, false otherwise.
     */
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Accesses the front element of the queue.
     *
     * This method locks the mutex and returns the front element of the queue.
     *
     * @return The front element of the queue.
     */
    T front() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.front();
    }

private:
    std::queue<T> queue_; ///< The underlying queue storing the elements.
    std::mutex mutex_; ///< Mutex for synchronizing access to the queue.
    std::condition_variable cond_var_; ///< Condition variable for notifying waiting threads.
};
#endif