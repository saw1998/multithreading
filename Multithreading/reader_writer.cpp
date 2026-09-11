#include <mutex>
class ReadWriteLock{
    int readerCount_ = 0;
    int waitingWriterCount_ = 0;
    bool writing_ = false;

    std::mutex mtx_;
    std::condition_variable cv;

public:
    void lockRead(){
        std::unique_lock<std::mutex> lock(mtx_);

        cv.wait(lock, [this](){
            return waitingWriterCount_ == 0 && !writing_;
        });

        readerCount_++;
    }

    void unlockRead(){
        std::unique_lock<std::mutex> lock(mtx_);
        readerCount_--;
        if(readerCount_ == 0){
            cv.notify_all();
        }
    }

    void lockWrite(){
        std::unique_lock<std::mutex> lock(mtx_);
        waitingWriterCount_++;
        cv.wait(lock, [this](){
            return readerCount_ == 0 && !writing_;
        });
        waitingWriterCount_--;
        writing_=true;
    }

    void unloakWrite(){
        std::unique_lock<std::mutex> lock(mtx_);
        writing_=false;
        cv.notify_all();
    }
};

class ReadLockGuard{
    ReadWriteLock &rwl_;
public:
    ReadLockGuard(ReadWriteLock& rwl) : rwl_(rwl) {
        rwl_.lockRead();
    }
    ~ReadLockGuard(){
        rwl_.unlockRead();
    }
    ReadLockGuard(const ReadLockGuard&) = delete;
};

class WriteLockGuard{
    ReadWriteLock &rwl_;
public:
    WriteLockGuard(ReadWriteLock &rwl) : rwl_(rwl) {
        rwl_.lockWrite();
    }
    ~WriteLockGuard(){
        rwl_.unloakWrite();
    }
    WriteLockGuard(const WriteLockGuard&) = delete;
};
