ThreadPool
==========

A simple C++11 resizable Thread Pool implementation.


Basic usage:
```c++
// create thread pool with 4 worker threads
ThreadPool pool(4);

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);
pool.resize(10);

// get result from future
std::cout << result.get() << std::endl;

```
