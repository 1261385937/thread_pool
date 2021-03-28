# thread_pool
A very high performance lock free thread pool with roundRobin dispatching task.
</br> Two work mode:
</br> 1、Nosteal. It is used for the scene that all tasks cost the very similarity time. A task dispatched for a thread will always excute in the thread.
</br> 2、Steal. It is used for the scene that the tasks cost the different time. If a thread deals the task slowly than other thread, then the task of the thread will be stolen by other thread.
## Usage
```c++
#include "thread_pool.hpp"

int main()
{
    //auto thp = std::make_shared<thp::thread_pool<thp::work_mode::steal>>();
	auto thp = std::make_shared<thp::thread_pool<thp::work_mode::nosteal>>();
	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
	start = std::chrono::high_resolution_clock::now();
	std::thread th1([&thp]() {
		int xt = 200000;
		while (xt--)
		{
			thp->add_task([](int x, int y, const std::string& z) {
				return x + y + atoi(z.data());
			}, 2, 3, "6");
		}
		//printf("th1 exit\n");
	});

	std::thread th2([&thp]() {
		int x = 20000000;
		while (x--)
		{
			thp->add_task([](int x, float z) {
				return x + z;
			}, 2, 5.0f);
		}
		//printf("th2 exit\n");
	});

	std::thread th3([&thp]() {
		int x = 20000000;
		while (x--)
		{
			thp->add_task([](int x, int y, float z) {
				return x + y + z;
			}, 2, 3, 5.0f);
		}
		//printf("th3 exit\n");
	});

	if (th1.joinable())
		th1.join();
	if (th2.joinable())
		th2.join();
	if (th3.joinable())
		th3.join();
	thp->stop();
	thp.reset();
	end = std::chrono::high_resolution_clock::now();
	auto elapsed_seconds1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("%lld\n", elapsed_seconds1);
	return 0;
}
```