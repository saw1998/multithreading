#include <iostream>
#include <queue>
#include <thread>
#include <vector>

template<typename T>
class ThreadSafeQueue{
    std::queue<T> queue_;
    std::mutex mtx_;

    std::condition_variable cv_not_full;
    std::condition_variable cv_not_empty;

    int maxSize_;
    int terminated_;

public:
    explicit ThreadSafeQueue(int maxSize) : maxSize_(maxSize), terminated_(false) {
        if(maxSize <= 0){
            throw std::runtime_error("maxsize should be positive number");
        }
    }

    bool push(T data){
        std::unique_lock<std::mutex> lock(mtx_);

        cv_not_full.wait(lock, [this](){
            return terminated_ || queue_.size() < maxSize_;
        });

        if(terminated_) return false;

        queue_.push(data);

        cv_not_empty.notify_one();

        return true;
    }

    std::optional<T> pop(){
        std::unique_lock<std::mutex> lock(mtx_);

        cv_not_empty.wait(lock, [this](){
            return terminated_ || !queue_.empty();
        });

        if(terminated_ && queue_.empty()) return std::nullopt;

        T data = queue_.front();
        queue_.pop();

        cv_not_full.notify_one();
        return data;
    }

    void terminate(){
        std::unique_lock<std::mutex> lock(mtx_);
        terminated_ = true;
        cv_not_empty.notify_all();
        cv_not_full.notify_all();
    }
};

int main(){
    ThreadSafeQueue<int> tsq(1000);

    auto pusher = [&](int i){
        tsq.push(i);
    };

    auto poper = [&](){
        auto data = tsq.pop();
        if(data.has_value()){
            std::cout<<data.value()<<" ";
        } else {
            std::cout<<"null"<<" ";
        }
    };

    std::vector<std::thread> pushThread;
    std::vector<std::thread> popThread;


    for(int i=0;i<1000; i++){
        int r = rand() % 1000;
        if(r < 500) pushThread.emplace_back(pusher, i);
        popThread.emplace_back(poper);
        if(r < 1) tsq.terminate();
    }



    for(auto& pusher : pushThread){
        pusher.join();
    }
    for(auto& poper : popThread){
        poper.join();
    }

    tsq.terminate();
}
