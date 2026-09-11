#include <condition_variable>
#include<iostream>
#include <mutex>
#include <optional>
#include <queue>

//fair reader-writer (no starvation on either side)

template<typename T>
class BoundedStreamQueue{
    std::queue<T> _queue;

    std::mutex _mutex;
    std::condition_variable cv_not_empty;
    std::condition_variable cv_not_full;

    size_t _max_size;
    bool _terminated = false;

public:
    //constructor etc
    explicit BoundedStreamQueue(size_t max_size) : _max_size(max_size) {
        if(_max_size == 0) throw std::invalid_argument("Size must be > 0");
    }

    bool push(T &data){
        std::unique_lock<std::mutex> lock(_mutex);

        cv_not_full.wait(lock, [this](){
            return _terminated || _queue.size() < _max_size;
        });

        if(_terminated) return false;

        _queue.push(data);

        cv_not_empty.notify_one();

        return true;
    }

    std::optional<T> front(){
        std::unique_lock<std::mutex> lock(_mutex);

        // if(_terminated) return std::nullopt;

        cv_not_empty.wait(lock, [this](){
            return _terminated || !_queue.empty();
        });

        if(_terminated && _queue.empty()) return std::nullopt;

        T data = _queue.front();
        _queue.pop();

        cv_not_full.notify_one();

        return data;
    }

    bool terminate(){
        std::unique_lock<std::mutex> lock(_mutex);

        _terminated = true;

        lock.unlock();

        cv_not_empty.notify_all();
        cv_not_full.notify_all();
    }
};
