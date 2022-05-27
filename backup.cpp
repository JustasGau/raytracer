#include <thread>
#include <iostream>

void func(int start, int end) {
    for (int i = start; i < end; i++) {
        std::cout << i << std::endl;
    }
}

int main(int argc, char *args[])
{
    const int processor_count = std::thread::hardware_concurrency();
    std::cout << processor_count << std::endl;
    std::thread t1(func, 0, 50);
    std::thread t2(func, 50, 100);
    t1.join();
    t2.join();
    return EXIT_SUCCESS;
}