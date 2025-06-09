#include "fiber.h"
#include "fiber_pool.h"

static bool debug = false;

namespace sylar {

// 当前线程上的协程控制信息

// // 正在运行的协程
// static thread_local Fiber* t_fiber = nullptr;
// // 主协程
// static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;
// // 调度协程
// static thread_local Fiber* t_scheduler_fiber = nullptr;

static thread_local std::vector<std::shared_ptr<Fiber>> t_fibers(128, nullptr); // 0:scheduler 1:main
static thread_local std::atomic<uint64_t> t_fiber_id{0};
#define SCHEDULER_FIBER_ID 0 // 调度协程id为0
#define MAIN_FIBER_ID 1 // 主协程id为1


// 协程id
static std::atomic<uint64_t> s_fiber_id{0};

// 协程计数器
static std::atomic<uint64_t> s_fiber_count{0};

void Fiber::SetThis(std::shared_ptr<Fiber> f)
{
	// t_fiber = f;
	t_fibers[++t_fiber_id] = f; // 将当前协程加入到线程局部存储的协程列表中
}

// 首先运行该函数创建主协程
std::shared_ptr<Fiber> Fiber::GetThis()
{
	// if(t_fiber)
	// {	
	// 	return t_fiber->shared_from_this();
	// }

	// std::shared_ptr<Fiber> main_fiber(new Fiber());
	// t_thread_fiber = main_fiber;
	// t_scheduler_fiber = main_fiber.get(); // 除非主动设置 主协程默认为调度协程
	
	// assert(t_fiber == main_fiber.get());
	// return t_fiber->shared_from_this();

	if(t_fiber_id != 0){
		return t_fibers[t_fiber_id];
	}
	t_fiber_id++;
	std::shared_ptr<Fiber> main_fiber(new Fiber());
	t_fibers[MAIN_FIBER_ID] = main_fiber;
	t_fibers[SCHEDULER_FIBER_ID] = t_fibers[MAIN_FIBER_ID];
	return t_fibers[MAIN_FIBER_ID];
}

void Fiber::SetSchedulerFiber(Fiber* f)
{
	// t_scheduler_fiber = f;
	t_fibers[SCHEDULER_FIBER_ID] = f->shared_from_this();
}

uint64_t Fiber::GetFiberId()
{
	if(t_fibers[t_fiber_id]){
		return t_fibers[t_fiber_id]->getId();
	}
	return (uint64_t)-1;
}

Fiber::Fiber()
{
	// SetThis(this->shared_from_this());
	m_state = RUNNING;
	
	if(getcontext(&m_ctx))
	{
		std::cerr << "Fiber() failed\n";
		pthread_exit(NULL);
	}
	
	m_id = s_fiber_id++;
	s_fiber_count ++;
	if(debug) std::cout << "Fiber(): main id = " << m_id << std::endl;
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler):
m_cb(cb), m_runInScheduler(run_in_scheduler)
{
	m_state = READY;

	// 分配协程栈空间
	m_stacksize = stacksize ? stacksize : 128000;
	m_stack = malloc(m_stacksize);

	if(getcontext(&m_ctx))
	{
		std::cerr << "Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler) failed\n";
		pthread_exit(NULL);
	}
	
	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;
	makecontext(&m_ctx, &Fiber::MainFunc, 0);
	
	m_id = s_fiber_id++;
	s_fiber_count ++;
	if(debug) std::cout << "Fiber(): child id = " << m_id << std::endl;
}

Fiber::~Fiber()
{
	s_fiber_count --;
	if(m_stack)
	{
		free(m_stack);
	}
	if(debug) std::cout << "~Fiber(): id = " << m_id << std::endl;	
}

void Fiber::reset(std::function<void()> cb)
{
	assert(m_stack != nullptr&&m_state == TERM);

	m_state = READY;
	m_cb = cb;

	if(getcontext(&m_ctx))
	{
		std::cerr << "reset() failed\n";
		pthread_exit(NULL);
	}

	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;
	makecontext(&m_ctx, &Fiber::MainFunc, 0);
}

void Fiber::resume()
{
	assert(m_state==READY);
	
	m_state = RUNNING;
	SetThis(this->shared_from_this());
	if(swapcontext(&(t_fibers[t_fiber_id-1]->m_ctx), &m_ctx))
	{
		std::cerr << "resume() to t_scheduler_fiber failed\n";
		pthread_exit(NULL);
	}		

	// if(m_runInScheduler)
	// {
	// 	SetThis(this->shared_from_this());
	// 	if(swapcontext(&(t_fibers[SCHEDULER_FIBER_ID]->m_ctx), &m_ctx))
	// 	{
	// 		std::cerr << "resume() to t_scheduler_fiber failed\n";
	// 		pthread_exit(NULL);
	// 	}		
	// }
	// else
	// {
	// 	SetThis(this->shared_from_this());
	// 	if(swapcontext(&(t_fibers[MAIN_FIBER_ID]->m_ctx), &m_ctx))
	// 	{
	// 		std::cerr << "resume() to t_thread_fiber failed\n";
	// 		pthread_exit(NULL);
	// 	}	
	// }
}

void Fiber::yield()
{
	assert(m_state==RUNNING || m_state==TERM);

	if(m_state!=TERM)
	{
		m_state = READY;
	}

	if(swapcontext(&m_ctx, &(t_fibers[--t_fiber_id]->m_ctx)))
	{
		std::cerr << "yield() to to t_scheduler_fiber failed\n";
		pthread_exit(NULL);
	}	
	// if(m_runInScheduler)
	// {
	// 	SetThis(t_fibers[MAIN_FIBER_ID]);
	// 	if(swapcontext(&m_ctx, &(t_fibers[MAIN_FIBER_ID]->m_ctx)))
	// 	{
	// 		std::cerr << "yield() to to t_scheduler_fiber failed\n";
	// 		pthread_exit(NULL);
	// 	}		
	// }
	// else
	// {
	// 	SetThis(t_fibers[SCHEDULER_FIBER_ID]);
	// 	if(swapcontext(&m_ctx, &(t_fibers[SCHEDULER_FIBER_ID]->m_ctx)))
	// 	{
	// 		std::cerr << "yield() to t_thread_fiber failed\n";
	// 		pthread_exit(NULL);
	// 	}	
	// }	
}

void Fiber::MainFunc()
{
	std::shared_ptr<Fiber> curr = GetThis();
	assert(curr!=nullptr);

	curr->m_cb(); 
	curr->m_cb = nullptr;
	curr->m_state = TERM;

	auto instance = sylar::FiberPool::GetInstance();
	instance->release(curr);	

	// 运行完毕 -> 让出执行权
	auto raw_ptr = curr.get();
	curr.reset(); 
	raw_ptr->yield(); 
}

}