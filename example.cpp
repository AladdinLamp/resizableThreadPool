#include <iostream>
#include <vector>
#include <chrono>
#include <random>

#include "ThreadPool.h"

using namespace std;

std::mutex g_mutex;

int main()
{
    random_device rd;
    ThreadPool pool(4);
    std::vector< std::future<int> > results;
    int k = 0;

    for (int i = 0; i < 1000000; ++i) {
        results.emplace_back(
            pool.enqueue([i, &k] {
                {
                    std::unique_lock<std::mutex> lock(g_mutex);
                    k++;
                }
                /*
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(0));
                std::cout << "world " << i << std::endl;
                */
                return i;
            })
        );

        if (i % 5 == 0) { 
            int j = rd() %10;
            cout << "ok" << j <<endl;
            pool.resize(j);
            cout << "pool size :"<< pool.getThreadNum()<<endl;
        }
    }


    /*
    for (int i = 0; i < 8; ++i) {
        results.emplace_back(pool.enqueue(func, i, i));
    }
    */

    for(auto && result: results)
        std::cout << "result :" << result.get() << std::endl;
    std::cout << std::endl;
    std::cout << "k = " << k << std::endl;

    
    return 0;
}

