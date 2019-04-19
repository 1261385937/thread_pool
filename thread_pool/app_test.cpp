
#include "thread_pool.h"

int main()
{
	thp::ThreadPool thp;
	//std::thread th1([&thp]()
	//{
	//	int xt = 100000;
	//	while (xt--)
	//	{
	//		auto ret = thp.addTask(
	//			[](int x, int y, const std::string & f, float z)
	//		{
	//			printf("%d %d %s %f\n", x, y, f.c_str(), z);
	//			//std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
	//			return 3;
	//		}, std::move(xt), 3, "4", 5.0f);
	//		//auto retvalue = ret.get();
	//		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	printf("th1 exit\n");
	//});

	//std::thread th2([&thp]()
	//{
	//	int x = 1000;
	//	while (x--)
	//	{
	//		auto ret = thp.addTask(
	//			[](int x, int y, const std::string & f, float z)
	//		{
	//			//printf("%d %d %s %f\n", x, y, f.c_str(), z);
	//			std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
	//			return 3;
	//		}, 2, 3, "4", 5.0f);
	//		//auto retvalue = ret.get();
	//		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	printf("th2 exit\n");
	//});

	//std::thread th3([&thp]()
	//{
	//	int x = 1000;
	//	while (x--)
	//	{
	//		auto ret = thp.addTask(
	//			[](int x, int y, const std::string & f, float z)
	//		{
	//			//printf("%d %d %s %f\n", x, y, f.c_str(), z);
	//			std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
	//			return 3;
	//		}, 2, 3, "4", 5.0f);
	//		//auto retvalue = ret.get();
	//		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//	}
	//	printf("th3 exit\n");
	//});
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	/*if (th1.joinable())
		th1.join();
	if (th2.joinable())
		th2.join();
	if (th3.joinable())
		th3.join();*/

	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
	int xt = 128;
	start = std::chrono::high_resolution_clock::now();
	while (xt--)
	{
		auto ret = thp.addTask(
			[](int x, int y, const std::string & f, float z)
		{
			//printf("%d %d %s %f\n", x, y, f.c_str(), z);
			//std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
			for (int i = 0; i < 100000; ++i)
			{
				auto tt = x * y - z;
				x = x * tt;
				y = x * y;
				z = y * z;
				x = z * y;
				x = x * x;
				y = x + x;
				z = y * z;
			}
			
			
		
			return 3;
		}, std::move(xt), 3, "4", 5.0f);
		//auto retvalue = ret.get();
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	
	thp.stop();
	end = std::chrono::high_resolution_clock::now();
	auto elapsed_seconds1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	printf("%lld\n", elapsed_seconds1);
	return 0;
}