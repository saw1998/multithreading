#include <future>
#include<iostream>

int expensiveComputation(int n){
    std::cout<<"Computation on thread: "<<std::this_thread::get_id()<<std::endl;

    long long sum = 0;
    for(int i=0;i<n;i++) sum+=i;

    return (int)(sum % 100000);
}

void async_example(){
    auto future = std::async(std::launch::async, expensiveComputation, 100000);

    std::cout<<"Task submitted. Main thread continuing...\n";

    int result = future.get();
    std::cout<<"Result: "<< result << "\n";

    auto lazyFuture = std::async(std::launch::deferred, expensiveComputation, 50000);

    std::cout<<"Deferred task created (NOT running yet)\n";

    int lazyResult = lazyFuture.get();

    // Collect all results
}
