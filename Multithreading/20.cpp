#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_map>

class ReaderWriterLock{
    //writer priority

    std::mutex mutex_;
    std::condition_variable cv_reader_can_proceed;
    std::condition_variable cv_writer_can_proceed;

    int activeReaders_ = 0;
    bool activeWriter_ = false;
    int waitingWriters_ = 0;

public:
    //reader lock
    void lockRead(){
        std::unique_lock<std::mutex> lock(mutex_);

        cv_reader_can_proceed.wait(lock, [this](){
            return !activeWriter_ && waitingWriters_ == 0;
        });

        activeReaders_++;
    }

    void unlockRead(){
        std::unique_lock<std::mutex> lock(mutex_);

        activeReaders_--;

        if(activeReaders_ == 0 && waitingWriters_ > 0){
            cv_writer_can_proceed.notify_one();
        }
    }

    void lockWrite(){
        std::unique_lock<std::mutex> lock(mutex_);

        waitingWriters_++;

        cv_writer_can_proceed.wait(lock, [this](){
            return activeReaders_ == 0 && !activeWriter_;
        });


        waitingWriters_--;
        activeWriter_ = true;
    }

    void unlockWrite(){
        std::unique_lock<std::mutex> lock(mutex_);

        activeWriter_ = false;

        if(waitingWriters_ > 0){
            cv_writer_can_proceed.notify_one();
        } else {
            cv_reader_can_proceed.notify_all();
        }
    }
};

class ReadLock{
    ReaderWriterLock &rwl_;
public:
    explicit ReadLock(ReaderWriterLock &rwl) : rwl_(rwl) {
        rwl_.lockRead();
    }
    ~ReadLock(){
        rwl_.unlockRead();
    }

    ReadLock(const ReadLock&) = delete;
};

class WriteLock{
    ReaderWriterLock &rwl_;
public:
    explicit WriteLock(ReaderWriterLock &rwl) : rwl_(rwl){
        rwl_.lockWrite();
    }
    ~WriteLock(){
        rwl_.unlockWrite();
    }
    WriteLock(const WriteLock&) = delete;
};

class Database{
    ReaderWriterLock rwl_;
    std::unordered_map<int,int> data;

public:
    std::optional<int> get(int key){
        ReadLock lock(rwl_);

        if(data.find(key) == data.end()) return std::nullopt;
        return data[key];
    }

    void set(int key, int value){
        WriteLock lock(rwl_);

        data[key] = value;
    }
};

int main(){
    Database db;
    std::optional<int> data = db.get(1);
    if(data.has_value()){
        std::cout<<"data has value -> "<<data.value()<<std::endl;
    } else {
        std::cout<<"data not available for key -> "<<1<<std::endl;
    }
    db.set(1,34524);
    data = db.get(1);
    if(data.has_value()){
        std::cout<<"data has value -> "<<data.value()<<std::endl;
    } else {
        std::cout<<"data not available for key -> "<<1<<std::endl;
    }

}
