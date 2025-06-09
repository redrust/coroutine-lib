#ifndef _SINGLETON_H_
#define _SINGLETON_H_

#include <memory>
#include <mutex>

namespace sylar {
template<typename T>
class Singleton
{
private:

protected:
    Singleton() {}  
    struct Token{};
    
public:
    // Delete copy constructor and assignment operation
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static T* GetInstance() 
    {
        static T instance{Token{}};
        return &instance;
    }
};
}
#endif