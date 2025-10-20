#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include<thread>
#include <iostream>
#include <unordered_map>
enum class PoolMode
{
	MODE_FIXED,     //固定模式，
	MODE_CACHED,    //可增长模式
};


//线程类型
class Thread
{
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);//构造函数
	~Thread();//析构函数
	void start();   //创建一个std::thread
	int getThreadId() const;//获取线程id
private:
	static int generateId_;
	ThreadFunc func_;
	int threadId_;
};

class Any
{
public:
	Any(const Any&) = delete;      //unique不能有拷贝构造
	Any& operator=(const Any&) = delete;
	Any() = default;
	Any(Any&& ) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T date)
		:sp_(std::make_unique<Derive<T>>(date))   //基类指针指向子类
	{};
	~Any()=default;
private:
	//基类
	class Base
	{
	public: virtual ~Base() = default;
	};

	//子类
	template<typename T>
	class Derive:public Base
	{
	public:
		Derive(T date)
			:date_(date)
		{};
		~Derive() = default;
		T date_;
	};
public:
	template<typename T>
	T castto()
	{
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(sp_.get());
		if (ptr == nullptr)
		{
			throw"type is unmath!";
		}
		return ptr->date_;
	}
private:
	std::unique_ptr<Base> sp_;
};

class Result;

//任务基类
class Task
{
public:
	Task();
	~Task() = default;
	virtual Any run() = 0;
	void setVal(Result* res);
	void exec();
private:
	Result* result_;
};




class Semaphore    //信号量，实现线程通信
{
public:
	Semaphore(int cnt=0)
		:cnt_(cnt)
	{}
	~Semaphore() = default;

	Semaphore(const Semaphore&) = delete;   //禁止拷贝构造
	Semaphore& operator=(const Semaphore&) = delete;
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cv_.wait(lock, [&]()->bool {return cnt_ > 0; });
		cnt_--;
	}

	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cnt_++;
		std::cout<<"信号量增加，当前值为："<<cnt_<<std::endl;
		cv_.notify_all();
		std::cout<<"通知完成"<<std::endl;
	}
private:
	int cnt_;
	std::mutex mtx_;
	std::condition_variable cv_;
};


//任务返回值类型
class Result
{
public:
	Result(std::shared_ptr<Task> task = NULL, bool isvaild = true);
	~Result() = default;
	Result(const Result&) = delete;      //禁止拷贝构造
	Result& operator=(const Result&) = delete;
	Result(Result&& res)
	{
		task_ = std::move(res.task_);
		isValid_ = res.isValid_.load();
		semPtr_ = std::move(res.semPtr_);
		any_ = std::move(res.any_);
	}
	Result& operator=(Result&&)
	{
		return *this;
	}
	//设置线程运行结果
	void setVal(Any);
	Any get();
private:
	std::unique_ptr<Semaphore> semPtr_;  // 使用指针
	Any any_;   //接收线程放回值
	std::shared_ptr<Task> task_;   //对应的任务
	std::atomic_bool isValid_;   //是否提交成功
};

//线程池类型
class ThreadPool
{
public:
	ThreadPool();     //线程池构造函数
	~ThreadPool();     //线程池析构函数

	void start(int initThreadSize=std::thread::hardware_concurrency());//开启线程池

	void setMode(PoolMode mode);   //设置线程池模式

	void setTaskQueMaxThreshHold(int threshhold); //设置任务队列阈值

	void setThreadSizeMaxThreshHold(int threshhold); //设置最大线程数量
	Result submitTask(std::shared_ptr<Task> sp);   //用户提交任务接口

	//关闭拷贝构造函数
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void threadFunc(int threadid);
	bool checkRunningState() const;   //判断线程池的状态，内部函数，不提供外部接口
private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  //线程列表
	int initThreadSize_;  //初始线程数量
	std::atomic_int idleThreadSize_; //空闲线程数量
	int threadSizeThreshHold_;     //线程数量的上限
	std::atomic_int curThreadSize_;  //当前线程数量

	std::queue<std::shared_ptr<Task>>  taskQue_; //任务队列
	std::atomic_int taskSize_;   //任务数量
	int taskQueMaxThreshHold_;   //最大任务数量

	std::mutex taskQueMtx_;  //任务队列互斥锁
	std::condition_variable notFull_;  //表示任务队列不满，用户可以添加任务
	std::condition_variable notEmpty_; //表示任务队列不空，线程可以消耗任务

	PoolMode poolMode_;   //当前线程池的模式
	std::atomic_bool isPoolRunning_;//当前线程池状态
	std::condition_variable poolExit_;
};
#endif