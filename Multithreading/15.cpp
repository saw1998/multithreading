#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <optional>
#include <thread>
#include <vector>

using namespace std;

template<typename T>
class ConditionalVariableQueue{
    queue<T> queue_;
    mutex mutex_;
    condition_variable cv_not_empty_;
    condition_variable cv_not_full_;
    size_t maxCapacity_;
    bool closed_ = false;

public:
    explicit ConditionalVariableQueue(size_t capacity)
        : maxCapacity_(capacity) {}

    //producer
    bool push(T item){
        unique_lock<mutex> lock(mutex_);

        cv_not_full_.wait(lock, [this](){
            return queue_.size() < maxCapacity_ || closed_;
        });

        if(closed_) return false;

        queue_.push(std::move(item));

        //notify one consume (not all - no need to wake everyone)
        cv_not_empty_.notify_one();

        return true;
        //lock released in destroctor
    }

    //consumer
    std::optional<T> pop(){
        unique_lock<mutex> lock(mutex_);

        cv_not_empty_.wait(lock, [this]{
            return !queue_.empty() || closed_;
        });

        if(queue_.empty()) return nullopt;

        auto tp = std::move(queue_.front());
        queue_.pop();

        cv_not_full_.notify_one();

        return tp;
    }

    //consumer with timeout
    optional<T> pop_with_timeout(chrono::milliseconds timeout){
        unique_lock<mutex> lock(mutex_);

        bool success = cv_not_empty_.wait_for(lock, timeout, [this](){
            return !queue_.empty() || closed_;
        });

        if(!success || queue_.empty()) return std::nullopt;

        T item = std::move(queue_.top());
        queue_.pop();

        cv_not_full_.notify_one();
        return item;
    }

    //shutdown -> wake everyone up to exit
    void close(){
        {
            lock_guard<mutex> lock(mutex_);
            closed_ = true;
        }
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    bool empyt() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }
};

//demo: multiple producer and consumer

void producer_consumer_full_demo(){
    ConditionalVariableQueue<int> queue(5);

    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 2;
    const int ITEM_PER_PRODUCER = 10;

    std::vector<thread> producers;
    std::vector<thread> consumers;

    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};

    //start producer
    for(int p=0; p<NUM_PRODUCERS; p++){
        producers.emplace_back([&, p]{
            for(int i=0;i<ITEM_PER_PRODUCER;i++){
                int item = p*100 + i;
                if(queue.push(item)){
                    totalProduced++;
                    std::cout<<"P"<<p<<" -> "<<item<<"\n";
                }
            }
            cout<<"Producer "<<p<<" done"<<endl;
        });
    }

    //start consumers
    for(int c=0;c<NUM_CONSUMERS;c++){
        consumers.emplace_back([&, c]{
            while(true){
                auto item = queue.pop();
                if(!item.has_value()){
                    cout<<"Consumer "<<c<<" exiting\n";
                    break;
                }
                totalConsumed++;
                cout<<"C"<<c<<" <- "<<*item<<endl;
                this_thread::sleep_for(chrono::milliseconds(50));
            }
        });
    }

    for(auto &p : producers) p.join();

    queue.close();

    for(auto &c : consumers) c.join();

    cout<<"Produced: "<<totalProduced<<" Consumed:"<<totalConsumed<<endl;
}

int main(){
    producer_consumer_full_demo();
}
