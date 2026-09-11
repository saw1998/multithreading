#include <exception>
#include <future>
#include <iostream>
#include <numeric>
#include <vector>

int computeSum(const std::vector<int>& data){
    return std::accumulate(data.begin(), data.end(), 0);
}

void async_demo(){
    std::vector<int> data(100000, 1);

    auto future = std::async(std::launch::async, computeSum, std::ref(data));

    //do other work while sum is being computed
    std::cout<<"Computing...\n";

    int result = future.get(); //blocks until ready
    std::cout<<"Sum : "<<result<<"\n";
}

void workerWithPromise(std::promise<int> promise){
    try{
        int result = 42; //expensive computation
        promise.set_value(result);
    } catch(std::exception ex){
        promise.set_exception(std::current_exception());
    }
}

void packaged_task_demo(){

}
