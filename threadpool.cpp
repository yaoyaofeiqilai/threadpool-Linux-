#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 4;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 5;  //线程最大空闲时间，单位秒


//////////////////////////////////////////////////////////////////////////////线程池构造函数
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	,idleThreadSize_(0)
	,isPoolRunning_(false)
	,curThreadSize_(0)
	,threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
{}

//线程池析构函数
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;     //设置线程状态
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();     //唤醒所有等待线程
	poolExit_.wait(lock, [&]()->bool {return curThreadSize_ == 0; });    //等待线程全部回收
}

void ThreadPool::start(int initThreadSize)
{
	//设置线程池状态
	isPoolRunning_ = true;
	//初始化线程数量
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//创建线程
	for (int i = 0; i < initThreadSize; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		threads_.emplace(ptr->getThreadId(),std::move(ptr));
	}

	//启动线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;
	}
};//开启线程池

void  ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())    //如果处于运行状态则不可以修改
		return;
	poolMode_ = mode;
};   //设置线程池模式

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())    //如果处于运行状态则不可以修改
		return;
	taskQueMaxThreshHold_ = threshhold;
};//设置任务队列阈值

void ThreadPool::setThreadSizeMaxThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;
	}
}//设置chached模式下最大线程数量


Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//等待任务队列右空余,超过一秒钟返回失败
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue if full , submit is fail" << std::endl;
		return {sp,false};
	};
	//提交任务
	taskQue_.emplace(sp);
	taskSize_++;
	notEmpty_.notify_all();

	//根据空闲线程数量和任务数量，判断是否需要添加新的线程
	if (poolMode_ == PoolMode::MODE_CACHED &&
		taskSize_ > idleThreadSize_ &&
		curThreadSize_ < threadSizeThreshHold_)
	{
		//创建新的线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadid = ptr->getThreadId();         //获取id
		threads_.emplace(ptr->getThreadId(),std::move(ptr));
		threads_[threadid]->start();   //启动线程
		curThreadSize_++;           //当前以及空闲线程加一
		idleThreadSize_++;
	}
	return {sp};  //返回result对象
};   //用户提交任务接口

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;   //未来可能有更多的判断
}


//////////////////////////////////////////////////////定义线程函数
void ThreadPool::threadFunc(int threadid)
{
	
	while(true)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::cout << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			std::unique_lock<std::mutex> lock(taskQueMtx_);     //尝试获取锁

				//每秒返回一次     区分超时返回，还是有任务待执行返回
				while (taskQue_.size() == 0)     //有任务直接取执行任务
				{
					if (!isPoolRunning_)     //没任务时，判断线程池是否结束，结束直接退出函数，结束线程
					{
						std::cout << "threadid:" << std::this_thread::get_id() << "已回收" << std::endl;
						curThreadSize_--;
						poolExit_.notify_all();
						return;
					}

					//线程池未结束，进入等待状态
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//cached模式下，空闲时间超过60s的线程,进行回收
					    //当前时间-last上次线程执行完成时间
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))   //超时退出
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto cur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);    //计算停止时间
							if (cur.count() >= THREAD_MAX_IDLE_TIME &&
								curThreadSize_ > initThreadSize_)
							{
								//超时回收线程
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;

								std::cout << "threadid:" << std::this_thread::get_id() <<
									"已回收" << std::endl;
								return;
							}
						}
					}
					else
	                {
						//等待notempty,fixed模式
						notEmpty_.wait(lock);
					}
				}
		}
			idleThreadSize_--;
			//从队列中取一个任务
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//通知其他线程
			if(taskSize_>0)notEmpty_.notify_all();
			std::cout << std::this_thread::get_id() << "获取任务成功" << std::endl;
			notFull_.notify_all();
	    //执行任务
		
		if (task != nullptr)
		{
			task->exec();
		}
		lastTime = std::chrono::high_resolution_clock().now();//更新上一次执行时间
		idleThreadSize_++;
	}
}


////////////////////////////////////////////////////线程相关定义

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)
	:func_(func)    //获取bind返回的函数对象，即threadpool中定义的线程函数
	,threadId_(generateId_++)
{}


Thread::~Thread()
{
	   
}

int Thread::getThreadId() const
{
	return threadId_;
}//获取线程id

//启动线程
void Thread::start()
{
	std::thread t(func_,threadId_);   //线程对象t 和线程函数func_
	std::cout << "线程id:" << t.get_id() << "线程已创建" << std::endl;
	t.detach();     //设置分离线程
}

Result::Result(std::shared_ptr<Task> task , bool isValid )
	:task_(task)
	,isValid_(isValid)
{
	semPtr_= std::make_unique<Semaphore>(0);  //初始化信号量为0
	task_->setVal(this);
};

//设置线程运行结果
void Result::setVal(Any any)
{
	any_ = std::move(any);  //赋值，信号量增加
	if(semPtr_==nullptr)
	{
		std::cout<<"semptr为空"<<std::endl;
	}
	semPtr_->post();
}

Any Result::get()
{
	if (!isValid_)    //获取线程计算结果
	{
		return "";
	}
	semPtr_->wait();  //如果未计算完成，阻塞主线程
	return std::move(any_);
}


void Task::setVal(Result* res)
{
	result_ = res;
}

void Task::exec()
{
	if(result_!=nullptr)      //设置result的值，即线程的运行结果
	{
		result_->setVal(run());
	}
}

Task::Task()
	:result_ (nullptr)
{};