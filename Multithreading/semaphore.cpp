#include <chrono>
#include <iostream>
#include <semaphore.h>
#include <semaphore>
#include <thread>

class ConnectionPool{
    std::counting_semaphore<10> semaphore_{10}; //TODO, why need to provide twice

public:
    void useConnection(int id){
        semaphore_.acquire(); //decrement, block if 0

        std::cout<<"Thread "<<id<<" got connection\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout<<"Thread "<<id<<"releasing connection\n";

        semaphore_.release(); //increment
    }
};
