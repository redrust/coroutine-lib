#include <deque>
#include "fiber.h"

namespace sylar {
class FiberPool {
public:
    FiberPool(size_t size) : m_fibers(size) {
        for (size_t i = 0; i < size; ++i) {
            m_fibers[i] = getFiber();
        }
    }

    std::shared_ptr<Fiber> acquire() {
        if (m_fibers.empty() == false) {
            auto fiber = m_fibers.front();
            m_fibers.pop_front();
            return fiber;
        }
        // If no free fibers, create a new one
        auto fiber = getFiber();
        return fiber;
    }

    void release(std::shared_ptr<Fiber> fiber) {
        if (fiber) {
            m_fibers.push_back(fiber); // Return it to the pool
        } else {
            // Handle the case where the fiber is null or invalid
            std::cerr << "Attempted to release a null or invalid fiber." << std::endl;
        }
    }

    static thread_local FiberPool& getInstance(size_t size = 10) {
        static thread_local FiberPool instance(size);
        return instance;
    }

    
private:

    FiberPool(const FiberPool&) = delete; // Disable copy constructor
    FiberPool& operator=(const FiberPool&) = delete; // Disable assignment operator
    FiberPool(FiberPool&&) = delete; // Disable move constructor
    FiberPool& operator=(FiberPool&&) = delete; // Disable move assignment operator

    std::shared_ptr<Fiber> getFiber() {
        auto fiber = std::make_shared<Fiber>([](){}, 0, false);
        m_size++;
        fiber->setState(Fiber::TERM);
        return fiber;
    }

    size_t m_size; // Total number of fibers in the pool
    std::deque<std::shared_ptr<Fiber>> m_fibers; // All fibers
};
}