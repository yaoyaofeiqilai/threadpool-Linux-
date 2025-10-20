#include<iostream>
#include<chrono>
#include <thread>

#include"threadpool.h"


class MyTask :public Task
{
public:
	MyTask(int start, int end)
		:start_(start)
		, end_(end)
	{};
	Any run()
	{
		long long sum = 0;
		for (int i = start_; i <= end_; i++)
		{
			sum += i;
		}
		//std::this_thread::sleep_for(std::chrono::seconds(1));
		return sum;
	}
private:
	int start_;
	int end_;
};
int main()
{
	/*{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);


		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		long long sum1 = res1.get().castto<long long>();
		long long sum2 = res2.get().castto<long long>();
		long long sum3 = res3.get().castto<long long>();

		std::cout << sum1 + sum2 + sum3 << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(8));
	}
	getchar();*/
	{
		ThreadPool pool;


		std::cout<<"线程开始"<<std::endl;
		pool.setMode(PoolMode::MODE_FIXED);

		pool.start(2);
		Result res = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		std::this_thread::sleep_for(std::chrono::seconds(10));
		long long sum = res.get().castto<long long>();
		std::cout << sum << std::endl;
	}
	
	return 0;
}