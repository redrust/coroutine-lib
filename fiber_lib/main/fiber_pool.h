#ifndef FIBER_POOL_H
#define FIBER_POOL_H

#include <deque>
#include "fiber.h"
#include "singleton.h"

namespace sylar {
class FiberPool:public Singleton<FiberPool>
{
public:
    FiberPool(Token) : m_size(10), m_fibers(m_size) {
        init(); // Initialize the pool with default fibers
    }

    ~FiberPool() {
        clear(); // Clear the pool when destroyed
    }

    std::shared_ptr<Fiber> acquire(std::function<void()> cb) {
        std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex to protect access to m_fibers
        if (m_fibers.empty() == true) {
            // enlarge(m_size * 2); // Double the size if no fibers are available
        }
        auto fiber = m_fibers.front();
        m_fibers.pop_front();
        // auto fiber = getFiber();
        fiber->reset(cb); // Reset the fiber with the new callback
        return fiber;
    }

    void release(std::shared_ptr<Fiber> fiber) {
        std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex to protect access to m_fibers
        if (fiber) {
            fiber->setState(Fiber::TERM);
            m_fibers.push_back(fiber); // Return it to the pool
        } else {
            // Handle the case where the fiber is null or invalid
            std::cerr << "Attempted to release a null or invalid fiber." << std::endl;
        }
    }

private:

    FiberPool(const FiberPool&) = delete; // Disable copy constructor
    FiberPool& operator=(const FiberPool&) = delete; // Disable assignment operator
    FiberPool(FiberPool&&) = delete; // Disable move constructor
    FiberPool& operator=(FiberPool&&) = delete; // Disable move assignment operator

    std::shared_ptr<Fiber> getFiber() {
        auto fiber = std::make_shared<Fiber>([](){});
        fiber->setState(Fiber::TERM);
        return fiber;
    }

    void init() {
        for (size_t i = 0; i < m_size; ++i) {
            m_fibers[i] = getFiber();
        }
    }

    void enlarge(size_t new_size) {
        m_fibers.resize(new_size); // Reserve space for new fibers
        for (size_t i = m_size; i < new_size; ++i) {
            m_fibers[i] = getFiber(); // Create new fibers and add them to the pool
        }
        m_size = new_size; // Update the size
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex to protect access to m_fibers
        m_fibers.clear(); // Clear all fibers in the pool
        m_size = 0; // Reset size
    }
    
    size_t m_size; // Total number of fibers in the pool
    std::deque<std::shared_ptr<Fiber>> m_fibers; // All fibers
    std::mutex m_mutex; // Mutex to protect access to the fiber pool
};
}

#endif // FIBER_POOL_H