# Comprehensive Guide: Multi-Threading & Stream Processing in C++ for Google Interview

## 📋 TABLE OF CONTENTS & LEARNING ROADMAP

```
Phase 1: Foundations (Week 1-2)
Phase 2: Synchronization Primitives (Week 2-3)
Phase 3: Lock-Free & Advanced Patterns (Week 3-4)
Phase 4: Stream Processing & Queues (Week 4-5)
Phase 5: Google-Style Problems (Week 5-6)
```

---

## PHASE 1: FOUNDATIONS

### 1.1 Thread Basics

```cpp
#include <iostream>
#include <thread>
#include <vector>

// ===== BASIC THREAD CREATION =====
void simpleFunction(int id) {
    std::cout << "Thread " << id << " running on thread: " 
              << std::this_thread::get_id() << "\n";
}

// ===== THREAD WITH REFERENCE PARAMETERS =====
void modifyValue(int& val) {
    val += 100;
}

// ===== THREAD WITH CLASS (Callable Object) =====
class Worker {
public:
    void operator()(int id) const {
        std::cout << "Worker " << id << " executing\n";
    }
};

int main() {
    // Method 1: Function pointer
    std::thread t1(simpleFunction, 1);
    
    // Method 2: Lambda
    std::thread t2([](int id) {
        std::cout << "Lambda thread " << id << "\n";
    }, 2);
    
    // Method 3: Callable object
    Worker worker;
    std::thread t3(worker, 3);
    
    // Method 4: Pass by reference (MUST use std::ref)
    int value = 42;
    std::thread t4(modifyValue, std::ref(value));
    
    // Method 5: Member function
    // std::thread t5(&MyClass::memberFunc, &obj, args...);
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    
    std::cout << "Modified value: " << value << "\n"; // 142
    
    // Spawning multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(simpleFunction, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### 1.2 Thread Lifecycle & RAII

```cpp
#include <thread>
#include <iostream>
#include <stdexcept>

// ===== PROBLEM: Exception Safety =====
// If exception thrown between thread creation and join(), 
// the program calls std::terminate()

// ===== SOLUTION: RAII Thread Guard =====
class ThreadGuard {
    std::thread& t_;
public:
    explicit ThreadGuard(std::thread& t) : t_(t) {}
    
    ~ThreadGuard() {
        if (t_.joinable()) {
            t_.join();  // or t_.detach()
        }
    }
    
    // Non-copyable
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
};

// ===== C++20: std::jthread (auto-joining thread) =====
#include <stop_token>

void stoppableWork(std::stop_token stoken, int id) {
    while (!stoken.stop_requested()) {
        std::cout << "Working... " << id << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Thread " << id << " stopped gracefully\n";
}

void demo_jthread() {
    std::jthread jt(stoppableWork, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    jt.request_stop(); // cooperative cancellation
    // auto-joins in destructor
}

// ===== Key Concepts =====
// - join(): Blocks calling thread until target completes
// - detach(): Thread runs independently (daemon thread)
// - joinable(): Returns true if join/detach hasn't been called
// - NEVER destroy a joinable thread (std::terminate called)
// - hardware_concurrency(): Returns hint of supported concurrent threads

void basics() {
    unsigned int n = std::thread::hardware_concurrency();
    std::cout << n << " concurrent threads supported\n";
    
    // Thread IDs
    std::cout << "Main thread ID: " << std::this_thread::get_id() << "\n";
    
    // Yielding
    std::this_thread::yield(); // hint to reschedule
    
    // Sleeping
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::this_thread::sleep_until(
        std::chrono::steady_clock::now() + std::chrono::seconds(1)
    );
}
```

### 1.3 Data Races — Understanding the Problem

```cpp
#include <thread>
#include <vector>
#include <iostream>

// ===== CLASSIC DATA RACE =====
int counter = 0; // shared mutable state — DANGER!

void unsafeIncrement(int iterations) {
    for (int i = 0; i < iterations; i++) {
        counter++; // NOT atomic: read-modify-write
        // Equivalent to:
        // int temp = counter;    (READ)
        // temp = temp + 1;       (MODIFY)  
        // counter = temp;        (WRITE)
        // Another thread can interleave between these steps!
    }
}

void demonstrateRace() {
    const int ITERATIONS = 1000000;
    
    std::thread t1(unsafeIncrement, ITERATIONS);
    std::thread t2(unsafeIncrement, ITERATIONS);
    
    t1.join();
    t2.join();
    
    // Expected: 2000000, Actual: some value < 2000000
    std::cout << "Counter: " << counter << "\n";
    // This is UNDEFINED BEHAVIOR per C++ standard
}
```

---

## PHASE 2: SYNCHRONIZATION PRIMITIVES

### 2.1 Mutex — The Fundamental Lock

```cpp
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>

// ===== std::mutex =====
class SafeCounter {
    mutable std::mutex mtx_;
    int count_ = 0;
    
public:
    void increment() {
        std::lock_guard<std::mutex> lock(mtx_);
        ++count_;
    } // lock released here (RAII)
    
    int get() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }
};

// ===== LOCK TYPES COMPARISON =====

// 1. lock_guard: Simple RAII, no manual control
void example_lock_guard(std::mutex& m) {
    std::lock_guard<std::mutex> lg(m);
    // critical section
} // auto unlock

// 2. unique_lock: Flexible, can unlock/relock, moveable
void example_unique_lock(std::mutex& m) {
    std::unique_lock<std::mutex> ul(m);           // locks immediately
    // ... critical section ...
    ul.unlock();                                    // manual unlock
    // ... non-critical work ...
    ul.lock();                                      // re-lock
    // ... critical section ...
    
    // Deferred locking:
    std::unique_lock<std::mutex> ul2(m, std::defer_lock);
    // ... do something ...
    ul2.lock(); // lock when ready
    
    // Try locking:
    std::unique_lock<std::mutex> ul3(m, std::try_to_lock);
    if (ul3.owns_lock()) {
        // got the lock
    }
    
    // Timed try:
    std::timed_mutex tm;
    std::unique_lock<std::timed_mutex> ul4(tm, std::chrono::milliseconds(100));
}

// 3. scoped_lock (C++17): Locks multiple mutexes, deadlock-free
void example_scoped_lock(std::mutex& m1, std::mutex& m2) {
    std::scoped_lock lock(m1, m2); // locks both, avoids deadlock
    // critical section with both resources
}
```

### 2.2 Deadlock — Problem & Solutions

```cpp
#include <mutex>
#include <thread>

// ===== DEADLOCK SCENARIO =====
class BankAccount {
    std::mutex mtx_;
    double balance_;
    int id_;
    
public:
    BankAccount(int id, double bal) : id_(id), balance_(bal) {}
    
    // BAD: Can deadlock!
    // Thread 1: transfer(A, B) locks A then tries B
    // Thread 2: transfer(B, A) locks B then tries A
    void bad_transfer(BankAccount& to, double amount) {
        std::lock_guard<std::mutex> lock1(this->mtx_);  // lock self
        std::lock_guard<std::mutex> lock2(to.mtx_);      // lock other — DEADLOCK RISK
        this->balance_ -= amount;
        to.balance_ += amount;
    }
    
    // SOLUTION 1: std::lock + std::lock_guard with adopt_lock
    void transfer_v1(BankAccount& to, double amount) {
        std::lock(this->mtx_, to.mtx_); // lock both atomically
        std::lock_guard<std::mutex> lock1(this->mtx_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(to.mtx_, std::adopt_lock);
        this->balance_ -= amount;
        to.balance_ += amount;
    }
    
    // SOLUTION 2: std::scoped_lock (C++17) — PREFERRED
    void transfer_v2(BankAccount& to, double amount) {
        std::scoped_lock lock(this->mtx_, to.mtx_);
        this->balance_ -= amount;
        to.balance_ += amount;
    }
    
    // SOLUTION 3: Lock ordering (always lock lower ID first)
    void transfer_v3(BankAccount& to, double amount) {
        BankAccount* first = this;
        BankAccount* second = &to;
        if (first->id_ > second->id_) std::swap(first, second);
        
        std::lock_guard<std::mutex> lock1(first->mtx_);
        std::lock_guard<std::mutex> lock2(second->mtx_);
        this->balance_ -= amount;
        to.balance_ += amount;
    }
    
    double getBalance() const { return balance_; }
};

/*
 * DEADLOCK PREVENTION RULES (Google Interview):
 * 1. Lock ordering: Always acquire locks in a consistent global order
 * 2. std::lock / scoped_lock: Atomic multi-lock acquisition
 * 3. Try-lock with backoff: Use try_lock, release all on failure
 * 4. Lock hierarchy: Assign levels, only lock lower levels
 * 5. Avoid nested locks when possible
 */
```

### 2.3 Shared (Reader-Writer) Locks

```cpp
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <map>
#include <string>
#include <iostream>
#include <vector>

// ===== READER-WRITER PATTERN =====
// Multiple readers can read simultaneously
// Writers need exclusive access
// Readers block writers, writers block everyone

class ThreadSafeCache {
    mutable std::shared_mutex smtx_;  // C++17
    std::map<std::string, std::string> cache_;
    
public:
    // READER: shared_lock allows concurrent reads
    std::string read(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(smtx_);
        auto it = cache_.find(key);
        return (it != cache_.end()) ? it->second : "";
    }
    
    // WRITER: unique_lock gives exclusive access
    void write(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(smtx_);
        cache_[key] = value;
    }
    
    // WRITER: erase
    bool erase(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(smtx_);
        return cache_.erase(key) > 0;
    }
    
    // READ then conditionally WRITE (lock upgrade pattern)
    // NOTE: C++ does NOT support lock upgrade directly!
    void readThenWrite(const std::string& key, const std::string& value) {
        // First check with shared lock
        {
            std::shared_lock<std::shared_mutex> readLock(smtx_);
            auto it = cache_.find(key);
            if (it != cache_.end() && it->second == value) {
                return; // already up to date
            }
        } // release shared lock
        
        // Then acquire exclusive lock to write
        {
            std::unique_lock<std::shared_mutex> writeLock(smtx_);
            // MUST re-check (another thread may have written)
            cache_[key] = value;
        }
    }
    
    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(smtx_);
        return cache_.size();
    }
};

// ===== PERFORMANCE DEMO =====
void demo_shared_lock() {
    ThreadSafeCache cache;
    cache.write("config", "v1");
    
    std::vector<std::thread> readers;
    // 10 readers can run CONCURRENTLY
    for (int i = 0; i < 10; i++) {
        readers.emplace_back([&cache, i]() {
            for (int j = 0; j < 10000; j++) {
                cache.read("config");
            }
        });
    }
    
    // 2 writers get EXCLUSIVE access
    std::thread writer1([&cache]() {
        for (int j = 0; j < 1000; j++) {
            cache.write("config", "v" + std::to_string(j));
        }
    });
    
    for (auto& r : readers) r.join();
    writer1.join();
}

/*
 * WHEN TO USE shared_mutex vs mutex:
 * - shared_mutex: Read-heavy workloads (90%+ reads)
 * - mutex: Write-heavy or balanced read/write
 * - shared_mutex has higher overhead per operation
 * - Beware writer starvation with many readers
 */
```

### 2.4 Recursive Mutex

```cpp
#include <mutex>

// ===== RECURSIVE MUTEX =====
// Same thread can lock multiple times (must unlock same number of times)
// Generally considered a code smell — prefer redesigning

class TreeNode {
    std::recursive_mutex mtx_;
    int value_;
    std::vector<TreeNode*> children_;
    
public:
    // This function locks the mutex
    void process() {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        // do work with value_
        for (auto* child : children_) {
            child->process();
        }
    }
    
    // This also locks — without recursive_mutex, calling 
    // updateAndProcess would deadlock on second lock
    void updateAndProcess(int newVal) {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        value_ = newVal;
        process(); // locks mtx_ again — OK with recursive_mutex
    }
};

// BETTER DESIGN: Separate locked and unlocked versions
class BetterTreeNode {
    std::mutex mtx_;
    int value_;
    
    void processImpl() { // private, no lock
        // actual work
    }
    
public:
    void process() {
        std::lock_guard<std::mutex> lock(mtx_);
        processImpl();
    }
    
    void updateAndProcess(int newVal) {
        std::lock_guard<std::mutex> lock(mtx_);
        value_ = newVal;
        processImpl(); // no double-locking
    }
};
```

### 2.5 Condition Variables

```cpp
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <iostream>

// ===== PRODUCER-CONSUMER WITH CONDITION VARIABLE =====
template<typename T>
class ThreadSafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
    size_t max_size_;
    bool shutdown_ = false;
    
public:
    explicit ThreadSafeQueue(size_t max_size = 100) 
        : max_size_(max_size) {}
    
    // PRODUCER: Push with blocking when full
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // Wait until queue is not full OR shutdown
        cv_not_full_.wait(lock, [this]() { 
            return queue_.size() < max_size_ || shutdown_; 
        });
        
        if (shutdown_) return false;
        
        queue_.push(std::move(item));
        cv_not_empty_.notify_one(); // wake one consumer
        return true;
    }
    
    // CONSUMER: Pop with blocking when empty
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // Wait until queue is not empty OR shutdown
        cv_not_empty_.wait(lock, [this]() {
            return !queue_.empty() || shutdown_;
        });
        
        if (shutdown_ && queue_.empty()) return false;
        
        item = std::move(queue_.front());
        queue_.pop();
        cv_not_full_.notify_one(); // wake one producer
        return true;
    }
    
    // CONSUMER: Pop with timeout
    bool try_pop(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        bool success = cv_not_empty_.wait_for(lock, timeout, [this]() {
            return !queue_.empty() || shutdown_;
        });
        
        if (!success || (shutdown_ && queue_.empty())) return false;
        
        item = std::move(queue_.front());
        queue_.pop();
        cv_not_full_.notify_one();
        return true;
    }
    
    // Non-blocking try
    bool try_push(T item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.size() >= max_size_) return false;
        queue_.push(std::move(item));
        cv_not_empty_.notify_one();
        return true;
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx_);
        shutdown_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
};

// ===== USAGE =====
void producer_consumer_demo() {
    ThreadSafeQueue<int> queue(10);
    
    // Producers
    auto producer = [&](int id) {
        for (int i = 0; i < 20; i++) {
            queue.push(id * 100 + i);
            std::cout << "P" << id << " produced " << (id * 100 + i) << "\n";
        }
    };
    
    // Consumers
    auto consumer = [&](int id) {
        int item;
        while (queue.pop(item)) {
            std::cout << "C" << id << " consumed " << item << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    
    std::thread p1(producer, 1), p2(producer, 2);
    std::thread c1(consumer, 1), c2(consumer, 2);
    
    p1.join();
    p2.join();
    
    // After producers done, signal shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    queue.shutdown();
    
    c1.join();
    c2.join();
}

/*
 * CONDITION VARIABLE PITFALLS:
 * 
 * 1. SPURIOUS WAKEUPS: Always use predicate version of wait()
 *    BAD:  cv.wait(lock);
 *    GOOD: cv.wait(lock, []{ return condition; });
 * 
 * 2. LOST NOTIFICATIONS: If notify called before wait, signal is lost
 *    Always check the condition before waiting
 * 
 * 3. Must hold the mutex when modifying shared state
 *    notify_one/all can be called with or without lock held
 *    (releasing lock before notify can improve performance)
 */
```

### 2.6 Atomics

```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>

// ===== ATOMIC BASICS =====
std::atomic<int> atomicCounter{0};

void atomicIncrement(int iterations) {
    for (int i = 0; i < iterations; i++) {
        atomicCounter.fetch_add(1, std::memory_order_relaxed);
        // Or simply: atomicCounter++;
    }
}

// ===== ATOMIC OPERATIONS =====
void atomic_operations_demo() {
    std::atomic<int> x{0};
    
    x.store(42);                    // atomic write
    int val = x.load();             // atomic read
    int old = x.exchange(100);      // atomic read-modify-write
    
    // Compare-and-swap (CAS) — THE fundamental lock-free primitive
    int expected = 100;
    bool success = x.compare_exchange_strong(expected, 200);
    // If x == expected: x = 200, returns true
    // If x != expected: expected = x, returns false
    
    // compare_exchange_weak: Can spuriously fail (use in loops)
    expected = 200;
    while (!x.compare_exchange_weak(expected, 300)) {
        // expected is updated to current value on failure
    }
    
    // Fetch operations
    x.fetch_add(1);    // atomic increment, returns old value
    x.fetch_sub(1);    // atomic decrement
    x.fetch_and(0xFF); // atomic bitwise AND
    x.fetch_or(0x01);  // atomic bitwise OR
    x.fetch_xor(0x01); // atomic bitwise XOR
}

// ===== MEMORY ORDERS (from weakest to strongest) =====
/*
 * memory_order_relaxed:
 *   - Only atomicity guaranteed, no ordering
 *   - Good for counters, statistics
 *
 * memory_order_acquire (loads):
 *   - No reads/writes in current thread can be reordered BEFORE this load
 *   - "I want to see everything the other thread did before its release"
 *
 * memory_order_release (stores):
 *   - No reads/writes in current thread can be reordered AFTER this store
 *   - "Everything I did before this is visible to the acquiring thread"
 *
 * memory_order_acq_rel:
 *   - Both acquire and release semantics (for read-modify-write ops)
 *
 * memory_order_seq_cst (DEFAULT):
 *   - Strongest: total global ordering of all seq_cst operations
 *   - Most intuitive but potentially slowest
 */

// ===== ACQUIRE-RELEASE EXAMPLE =====
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;                                          // (1)
    ready.store(true, std::memory_order_release);       // (2)
    // Release ensures (1) happens before (2) is visible
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}   // (3)
    // Acquire ensures everything before the release store
    // is visible after this load returns true
    assert(data == 42);  // GUARANTEED
}

// ===== SPINLOCK USING ATOMICS =====
class SpinLock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin — busy wait
            // Optional: add backoff strategy
            #if defined(__cpp_lib_atomic_flag_test)
            while (flag_.test(std::memory_order_relaxed)) {
                std::this_thread::yield(); // TTAS optimization
            }
            #endif
        }
    }
    
    void unlock() {
        flag_.clear(std::memory_order_release);
    }
    
    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

// ===== LOCK-FREE STACK (Classic Interview Question) =====
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };
    
    std::atomic<Node*> head_{nullptr};
    
public:
    void push(T val) {
        Node* newNode = new Node(std::move(val));
        newNode->next = head_.load(std::memory_order_relaxed);
        
        // CAS loop: keep trying until we successfully update head
        while (!head_.compare_exchange_weak(
            newNode->next,  // expected (updated on failure)
            newNode,        // desired
            std::memory_order_release,
            std::memory_order_relaxed
        )) {
            // newNode->next is automatically updated to current head
        }
    }
    
    bool pop(T& result) {
        Node* oldHead = head_.load(std::memory_order_relaxed);
        
        while (oldHead && !head_.compare_exchange_weak(
            oldHead,
            oldHead->next,
            std::memory_order_acquire,
            std::memory_order_relaxed
        )) {
            // oldHead is automatically updated
        }
        
        if (!oldHead) return false;
        
        result = std::move(oldHead->data);
        delete oldHead; // WARNING: ABA problem possible in production
        return true;
    }
    
    ~LockFreeStack() {
        T dummy;
        while (pop(dummy)) {}
    }
};

/*
 * ABA PROBLEM:
 * Thread 1: reads head = A
 * Thread 1: suspended
 * Thread 2: pops A, pops B, pushes A back (different A or reused memory)
 * Thread 1: CAS succeeds (head is still A) but the list structure changed!
 * 
 * Solutions:
 * 1. Tagged pointers (pack version counter with pointer)
 * 2. Hazard pointers
 * 3. Epoch-based reclamation
 * 4. std::shared_ptr atomic operations
 */
```

### 2.7 Semaphores (C++20)

```cpp
#include <semaphore>
#include <thread>
#include <iostream>
#include <vector>

// ===== COUNTING SEMAPHORE =====
// Limits concurrent access to N threads
class ConnectionPool {
    std::counting_semaphore<10> semaphore_{10}; // max 10 concurrent connections
    
public:
    void useConnection(int id) {
        semaphore_.acquire(); // decrement, blocks if 0
        
        std::cout << "Thread " << id << " got connection\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Thread " << id << " releasing connection\n";
        
        semaphore_.release(); // increment
    }
};

// ===== BINARY SEMAPHORE (C++20) =====
// Like a mutex but can be released by a different thread
std::binary_semaphore 
    smphSignalMainToThread{0},
    smphSignalThreadToMain{0};

void threadFunc() {
    smphSignalMainToThread.acquire(); // wait for main
    std::cout << "Thread got signal from main\n";
    smphSignalThreadToMain.release(); // signal main
}

// ===== IMPLEMENTING SEMAPHORE PRE-C++20 =====
class Semaphore {
    std::mutex mtx_;
    std::condition_variable cv_;
    int count_;
    
public:
    explicit Semaphore(int count = 0) : count_(count) {}
    
    void acquire() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        --count_;
    }
    
    void release() {
        std::lock_guard<std::mutex> lock(mtx_);
        ++count_;
        cv_.notify_one();
    }
    
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (count_ > 0) {
            --count_;
            return true;
        }
        return false;
    }
};
```

### 2.8 Futures, Promises, and Async

```cpp
#include <future>
#include <thread>
#include <iostream>
#include <numeric>
#include <vector>

// ===== std::async — High-level thread dispatch =====
int computeSum(const std::vector<int>& data) {
    return std::accumulate(data.begin(), data.end(), 0);
}

void async_demo() {
    std::vector<int> data(1000000, 1);
    
    // Launch policy:
    // std::launch::async — guaranteed new thread
    // std::launch::deferred — lazy evaluation on get()
    // std::launch::async | std::launch::deferred — implementation decides
    
    auto future = std::async(std::launch::async, computeSum, std::ref(data));
    
    // Do other work while sum is being computed...
    std::cout << "Computing...\n";
    
    int result = future.get(); // blocks until ready
    std::cout << "Sum: " << result << "\n";
}

// ===== std::promise / std::future — Manual channel =====
void workerWithPromise(std::promise<int> promise) {
    try {
        int result = 42; // expensive computation
        promise.set_value(result);
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

// ===== std::packaged_task — Wraps callable for async execution =====
void packaged_task_demo() {
    std::packaged_task<int(int, int)> task([](int a, int b) {
        return a + b;
    });
    
    auto future = task.get_future();
    
    std::thread t(std::move(task), 2, 3); // run on separate thread
    
    std::cout << "Result: " << future.get() << "\n"; // 5
    t.join();
}

// ===== PARALLEL ACCUMULATE (Interview Classic) =====
template<typename Iterator, typename T>
T parallel_accumulate(Iterator begin, Iterator end, T init) {
    const long length = std::distance(begin, end);
    if (length < 10000) {
        return std::accumulate(begin, end, init);
    }
    
    const long num_threads = std::min(
        (long)std::thread::hardware_concurrency(),
        (length + 9999) / 10000
    );
    const long block_size = length / num_threads;
    
    std::vector<std::future<T>> futures;
    Iterator block_start = begin;
    
    for (long i = 0; i < num_threads - 1; ++i) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        
        futures.push_back(std::async(std::launch::async,
            [](Iterator b, Iterator e) {
                return std::accumulate(b, e, T{});
            }, block_start, block_end));
        
        block_start = block_end;
    }
    
    // Last block (may be slightly larger)
    T last_result = std::accumulate(block_start, end, T{});
    
    T result = init + last_result;
    for (auto& f : futures) {
        result += f.get();
    }
    
    return result;
}
```

### 2.9 Latches and Barriers (C++20)

```cpp
#include <latch>
#include <barrier>
#include <thread>
#include <iostream>
#include <vector>

// ===== std::latch — Single-use countdown synchronization =====
// Each thread counts down; threads can wait for count to reach zero
void latch_demo() {
    const int NUM_WORKERS = 5;
    std::latch workDone(NUM_WORKERS);
    std::latch startSignal(1);
    
    auto worker = [&](int id) {
        startSignal.wait();         // wait for start signal
        std::cout << "Worker " << id << " working\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * id));
        std::cout << "Worker " << id << " done\n";
        workDone.count_down();      // signal completion
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.emplace_back(worker, i);
    }
    
    startSignal.count_down();       // start all workers
    workDone.wait();                // wait for all to finish
    
    std::cout << "All workers done!\n";
    
    for (auto& t : threads) t.join();
}

// ===== std::barrier — Reusable synchronization point =====
// Threads synchronize at the barrier repeatedly (phases)
void barrier_demo() {
    const int NUM_THREADS = 4;
    int phase = 0;
    
    auto on_completion = [&]() noexcept {
        // Called by one thread when all arrive
        std::cout << "=== Phase " << phase++ << " complete ===\n";
    };
    
    std::barrier sync_point(NUM_THREADS, on_completion);
    
    auto worker = [&](int id) {
        for (int iter = 0; iter < 3; iter++) {
            std::cout << "Thread " << id << " phase " << iter << "\n";
            sync_point.arrive_and_wait(); // synchronize
        }
    };
    
    std::vector<std::jthread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }
}
```

---

## PHASE 3: CLASSIC CONCURRENCY PATTERNS

### 3.1 Reader-Writer Lock from Scratch

```cpp
#include <mutex>
#include <condition_variable>
#include <cassert>

// ===== READER-WRITER LOCK (Interview: Implement from scratch) =====
class ReadWriteLock {
    std::mutex mtx_;
    std::condition_variable cv_;
    int activeReaders_ = 0;
    int waitingWriters_ = 0;
    bool activeWriter_ = false;
    
public:
    void lockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        // Wait if there's an active writer OR waiting writers (writer priority)
        cv_.wait(lock, [this]() {
            return !activeWriter_ && waitingWriters_ == 0;
        });
        ++activeReaders_;
    }
    
    void unlockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        --activeReaders_;
        if (activeReaders_ == 0) {
            cv_.notify_all(); // wake waiting writers
        }
    }
    
    void lockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        ++waitingWriters_;
        cv_.wait(lock, [this]() {
            return !activeWriter_ && activeReaders_ == 0;
        });
        --waitingWriters_;
        activeWriter_ = true;
    }
    
    void unlockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        activeWriter_ = false;
        cv_.notify_all(); // wake all waiting readers and writers
    }
};

// ===== RAII WRAPPERS =====
class ReadLockGuard {
    ReadWriteLock& rw_;
public:
    explicit ReadLockGuard(ReadWriteLock& rw) : rw_(rw) { rw_.lockRead(); }
    ~ReadLockGuard() { rw_.unlockRead(); }
};

class WriteLockGuard {
    ReadWriteLock& rw_;
public:
    explicit WriteLockGuard(ReadWriteLock& rw) : rw_(rw) { rw_.lockWrite(); }
    ~WriteLockGuard() { rw_.unlockWrite(); }
};

// ===== VARIANT: Reader-priority (can starve writers) =====
class ReaderPriorityRWLock {
    std::mutex mtx_;
    std::condition_variable cv_;
    int readers_ = 0;
    bool writing_ = false;
    
public:
    void lockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !writing_; });
        ++readers_;
    }
    
    void unlockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        if (--readers_ == 0) cv_.notify_one();
    }
    
    void lockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !writing_ && readers_ == 0; });
        writing_ = true;
    }
    
    void unlockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        writing_ = false;
        cv_.notify_all();
    }
};
```

### 3.2 Thread Pool

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <type_traits>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
    
public:
    explicit ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this]() {
                            return stop_ || !tasks_.empty();
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task(); // execute outside lock
                }
            });
        }
    }
    
    // Submit task and get future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> 
    {
        using ReturnType = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("Submit on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        cv_.notify_one();
        return result;
    }
    
    // Simple fire-and-forget version
    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) throw std::runtime_error("Enqueue on stopped pool");
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }
    
    size_t pendingTasks() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        return tasks_.size();
    }
    
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};

// ===== USAGE =====
void threadpool_demo() {
    ThreadPool pool(4);
    
    // Submit with return value
    auto f1 = pool.submit([](int a, int b) { return a + b; }, 2, 3);
    auto f2 = pool.submit([]() { return std::string("hello"); });
    
    std::cout << f1.get() << "\n"; // 5
    std::cout << f2.get() << "\n"; // "hello"
    
    // Submit many tasks
    std::vector<std::future<int>> results;
    for (int i = 0; i < 100; i++) {
        results.push_back(pool.submit([i]() { return i * i; }));
    }
    
    for (auto& r : results) {
        std::cout << r.get() << " ";
    }
}
```

### 3.3 Dining Philosophers

```cpp
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>

// ===== DINING PHILOSOPHERS (Classic Concurrency Problem) =====
class DiningPhilosophers {
    static constexpr int N = 5;
    std::mutex forks_[N];
    
public:
    // SOLUTION 1: Resource ordering (pick lower-numbered fork first)
    void dine_ordered(int id) {
        int left = id;
        int right = (id + 1) % N;
        
        // Always lock lower-numbered fork first
        int first = std::min(left, right);
        int second = std::max(left, right);
        
        for (int i = 0; i < 3; i++) {
            // Think
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Pick up forks
            std::lock_guard<std::mutex> lock1(forks_[first]);
            std::lock_guard<std::mutex> lock2(forks_[second]);
            
            // Eat
            std::cout << "Philosopher " << id << " eating\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // SOLUTION 2: std::lock (locks both atomically)
    void dine_stdlock(int id) {
        int left = id;
        int right = (id + 1) % N;
        
        for (int i = 0; i < 3; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::scoped_lock lock(forks_[left], forks_[right]);
            std::cout << "Philosopher " << id << " eating\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // SOLUTION 3: Limit concurrency (only N-1 can try at once)
    // Uses a semaphore to limit to 4 philosophers trying at once
    
    void start() {
        std::vector<std::thread> threads;
        for (int i = 0; i < N; i++) {
            threads.emplace_back(&DiningPhilosophers::dine_ordered, this, i);
        }
        for (auto& t : threads) t.join();
    }
};
```

### 3.4 Print in Order / FizzBuzz Multithreaded

```cpp
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <iostream>

// ===== LEETCODE 1114: Print in Order =====
class PrintInOrder {
    std::mutex mtx_;
    std::condition_variable cv_;
    int step_ = 1;
    
public:
    void first(std::function<void()> printFirst) {
        std::unique_lock<std::mutex> lock(mtx_);
        printFirst();
        step_ = 2;
        cv_.notify_all();
    }
    
    void second(std::function<void()> printSecond) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return step_ >= 2; });
        printSecond();
        step_ = 3;
        cv_.notify_all();
    }
    
    void third(std::function<void()> printThird) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return step_ >= 3; });
        printThird();
    }
};

// ===== LEETCODE 1195: FizzBuzz Multithreaded =====
class FizzBuzz {
    int n_;
    int current_ = 1;
    std::mutex mtx_;
    std::condition_variable cv_;
    
public:
    FizzBuzz(int n) : n_(n) {}
    
    void fizz(std::function<void()> printFizz) {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { 
                return current_ > n_ || (current_ % 3 == 0 && current_ % 5 != 0); 
            });
            if (current_ > n_) return;
            printFizz();
            ++current_;
            cv_.notify_all();
        }
    }
    
    void buzz(std::function<void()> printBuzz) {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { 
                return current_ > n_ || (current_ % 5 == 0 && current_ % 3 != 0); 
            });
            if (current_ > n_) return;
            printBuzz();
            ++current_;
            cv_.notify_all();
        }
    }
    
    void fizzbuzz(std::function<void()> printFizzBuzz) {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { 
                return current_ > n_ || (current_ % 15 == 0); 
            });
            if (current_ > n_) return;
            printFizzBuzz();
            ++current_;
            cv_.notify_all();
        }
    }
    
    void number(std::function<void(int)> printNumber) {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { 
                return current_ > n_ || (current_ % 3 != 0 && current_ % 5 != 0); 
            });
            if (current_ > n_) return;
            printNumber(current_);
            ++current_;
            cv_.notify_all();
        }
    }
};

// ===== LEETCODE 1226: Print Zero Even Odd =====
class ZeroEvenOdd {
    int n_;
    int current_ = 1;
    int state_ = 0; // 0: print zero, 1: print odd, 2: print even
    std::mutex mtx_;
    std::condition_variable cv_;
    
public:
    ZeroEvenOdd(int n) : n_(n) {}
    
    void zero(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n_; i++) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return state_ == 0; });
            printNumber(0);
            state_ = (i % 2 == 1) ? 1 : 2;
            cv_.notify_all();
        }
    }
    
    void odd(std::function<void(int)> printNumber) {
        for (int i = 1; i <= n_; i += 2) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return state_ == 1; });
            printNumber(i);
            state_ = 0;
            cv_.notify_all();
        }
    }
    
    void even(std::function<void(int)> printNumber) {
        for (int i = 2; i <= n_; i += 2) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return state_ == 2; });
            printNumber(i);
            state_ = 0;
            cv_.notify_all();
        }
    }
};
```

---

## PHASE 4: STREAM PROCESSING & QUEUES

### 4.1 Stream Processing Pipeline

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <optional>
#include <iostream>
#include <sstream>

// ===== MULTI-STAGE PIPELINE =====
// Stage 1: Read raw data
// Stage 2: Parse/Transform
// Stage 3: Aggregate/Output

template<typename T>
class StreamQueue {
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool closed_ = false;
    size_t maxSize_;
    std::condition_variable cv_full_;
    
public:
    explicit StreamQueue(size_t maxSize = 1000) : maxSize_(maxSize) {}
    
    // Returns false if queue is closed
    bool enqueue(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_full_.wait(lock, [this]() { 
            return queue_.size() < maxSize_ || closed_; 
        });
        if (closed_) return false;
        queue_.push(std::move(item));
        cv_.notify_one();
        return true;
    }
    
    // Returns nullopt if queue is closed and empty
    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        cv_full_.notify_one();
        return item;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();
        cv_full_.notify_all();
    }
    
    bool isClosed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_ && queue_.empty();
    }
};

// ===== PIPELINE STAGE =====
template<typename In, typename Out>
class PipelineStage {
    StreamQueue<In>& input_;
    StreamQueue<Out>& output_;
    std::function<Out(In)> transform_;
    std::vector<std::thread> workers_;
    
public:
    PipelineStage(
        StreamQueue<In>& input,
        StreamQueue<Out>& output,
        std::function<Out(In)> transform,
        int numWorkers = 1
    ) : input_(input), output_(output), transform_(transform) {
        for (int i = 0; i < numWorkers; i++) {
            workers_.emplace_back([this]() {
                while (auto item = input_.dequeue()) {
                    Out result = transform_(std::move(*item));
                    if (!output_.enqueue(std::move(result))) break;
                }
                output_.close(); // propagate close signal
            });
        }
    }
    
    ~PipelineStage() {
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }
};

// ===== FULL PIPELINE EXAMPLE =====
// Log processing: raw string -> parsed record -> aggregated stats

struct LogRecord {
    std::string timestamp;
    std::string level;
    std::string message;
    int responseTime;
};

struct Stats {
    int count;
    double avgResponseTime;
    std::string summary;
};

void stream_pipeline_demo() {
    StreamQueue<std::string> rawQueue(100);
    StreamQueue<LogRecord> parsedQueue(100);
    StreamQueue<Stats> statsQueue(100);
    
    // Stage 1: Parse raw strings -> LogRecord (4 workers)
    PipelineStage<std::string, LogRecord> parser(
        rawQueue, parsedQueue,
        [](std::string raw) -> LogRecord {
            // Simulate parsing
            return {"2024-01-01", "INFO", raw, (int)raw.length()};
        },
        4 // parallel parsers
    );
    
    // Stage 2: Aggregate LogRecords -> Stats (2 workers)
    PipelineStage<LogRecord, Stats> aggregator(
        parsedQueue, statsQueue,
        [](LogRecord record) -> Stats {
            return {1, (double)record.responseTime, record.message};
        },
        2
    );
    
    // Producer: Generate raw log lines
    std::thread producer([&rawQueue]() {
        for (int i = 0; i < 1000; i++) {
            rawQueue.enqueue("Log message " + std::to_string(i));
        }
        rawQueue.close();
    });
    
    // Consumer: Read aggregated stats
    std::thread consumer([&statsQueue]() {
        int total = 0;
        while (auto stat = statsQueue.dequeue()) {
            total += stat->count;
        }
        std::cout << "Total processed: " << total << "\n";
    });
    
    producer.join();
    consumer.join();
}
```

### 4.2 Bounded Buffer / Ring Buffer

```cpp
#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <atomic>

// ===== LOCK-BASED RING BUFFER =====
template<typename T>
class RingBuffer {
    std::vector<T> buffer_;
    size_t head_ = 0;      // write position
    size_t tail_ = 0;      // read position
    size_t count_ = 0;
    size_t capacity_;
    
    mutable std::mutex mtx_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    bool closed_ = false;
    
public:
    explicit RingBuffer(size_t capacity) 
        : buffer_(capacity), capacity_(capacity) {}
    
    bool push(const T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        notFull_.wait(lock, [this]() { 
            return count_ < capacity_ || closed_; 
        });
        if (closed_) return false;
        
        buffer_[head_] = item;
        head_ = (head_ + 1) % capacity_;
        ++count_;
        notEmpty_.notify_one();
        return true;
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        notEmpty_.wait(lock, [this]() { 
            return count_ > 0 || closed_; 
        });
        if (count_ == 0) return false;
        
        item = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) % capacity_;
        --count_;
        notFull_.notify_one();
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }
};

// ===== LOCK-FREE SPSC (Single Producer Single Consumer) QUEUE =====
// This is a very common interview question at Google
template<typename T>
class SPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
    };
    
    // Separate cache lines to avoid false sharing
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    
public:
    SPSCQueue() {
        Node* dummy = new Node();
        head_.store(dummy);
        tail_.store(dummy);
    }
    
    // Only called by producer thread
    void push(T item) {
        Node* newNode = new Node();
        newNode->data = std::move(item);
        
        Node* oldTail = tail_.load(std::memory_order_relaxed);
        oldTail->next.store(newNode, std::memory_order_release);
        tail_.store(newNode, std::memory_order_relaxed);
    }
    
    // Only called by consumer thread
    bool pop(T& item) {
        Node* oldHead = head_.load(std::memory_order_relaxed);
        Node* next = oldHead->next.load(std::memory_order_acquire);
        
        if (next == nullptr) return false; // empty
        
        item = std::move(next->data);
        head_.store(next, std::memory_order_relaxed);
        delete oldHead;
        return true;
    }
    
    ~SPSCQueue() {
        T dummy;
        while (pop(dummy)) {}
        delete head_.load();
    }
};

// ===== MPMC (Multi-Producer Multi-Consumer) Lock-Free Queue =====
// Simplified version using atomic operations
template<typename T>
class MPMCBoundedQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
    
    std::vector<Cell> buffer_;
    size_t bufferMask_;
    alignas(64) std::atomic<size_t> enqueuePos_{0};
    alignas(64) std::atomic<size_t> dequeuePos_{0};
    
public:
    // capacity must be power of 2
    explicit MPMCBoundedQueue(size_t capacity) 
        : buffer_(capacity), bufferMask_(capacity - 1) {
        for (size_t i = 0; i < capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    bool try_push(T item) {
        Cell* cell;
        size_t pos = enqueuePos_.load(std::memory_order_relaxed);
        
        while (true) {
            cell = &buffer_[pos & bufferMask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            
            if (diff == 0) {
                if (enqueuePos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // full
            } else {
                pos = enqueuePos_.load(std::memory_order_relaxed);
            }
        }
        
        cell->data = std::move(item);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }
    
    bool try_pop(T& item) {
        Cell* cell;
        size_t pos = dequeuePos_.load(std::memory_order_relaxed);
        
        while (true) {
            cell = &buffer_[pos & bufferMask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
            
            if (diff == 0) {
                if (dequeuePos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false; // empty
            } else {
                pos = dequeuePos_.load(std::memory_order_relaxed);
            }
        }
        
        item = std::move(cell->data);
        cell->sequence.store(
            pos + bufferMask_ + 1, std::memory_order_release);
        return true;
    }
};
```

### 4.3 Stream Processing Patterns

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <chrono>

// ===== PATTERN 1: Sliding Window Counter =====
class SlidingWindowCounter {
    mutable std::mutex mtx_;
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
    std::chrono::seconds windowSize_;
    
public:
    explicit SlidingWindowCounter(std::chrono::seconds window) 
        : windowSize_(window) {}
    
    void recordEvent() {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::steady_clock::now();
        timestamps_.push_back(now);
        evict(now);
    }
    
    size_t count() const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::steady_clock::now();
        // Remove old entries
        while (!timestamps_.empty() && 
               now - timestamps_.front() > windowSize_) {
            timestamps_.pop_front();
        }
        return timestamps_.size();
    }
    
private:
    void evict(std::chrono::steady_clock::time_point now) {
        while (!timestamps_.empty() && 
               now - timestamps_.front() > windowSize_) {
            timestamps_.pop_front();
        }
    }
};

// ===== PATTERN 2: Rate Limiter (Token Bucket) =====
class TokenBucketRateLimiter {
    mutable std::mutex mtx_;
    double tokens_;
    double maxTokens_;
    double refillRate_; // tokens per second
    std::chrono::steady_clock::time_point lastRefill_;
    
public:
    TokenBucketRateLimiter(double maxTokens, double refillRate)
        : tokens_(maxTokens), maxTokens_(maxTokens), 
          refillRate_(refillRate),
          lastRefill_(std::chrono::steady_clock::now()) {}
    
    bool tryConsume(double tokens = 1.0) {
        std::lock_guard<std::mutex> lock(mtx_);
        refill();
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }
    
    // Blocking version
    void consume(double tokens = 1.0) {
        while (!tryConsume(tokens)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(
            now - lastRefill_).count();
        tokens_ = std::min(maxTokens_, tokens_ + elapsed * refillRate_);
        lastRefill_ = now;
    }
};

// ===== PATTERN 3: Fan-Out/Fan-In =====
template<typename Input, typename Output>
class FanOutFanIn {
    using TaskFunc = std::function<Output(Input)>;
    
    StreamQueue<Input>& inputQueue_;
    StreamQueue<Output>& outputQueue_;
    std::vector<std::thread> workers_;
    TaskFunc processor_;
    
public:
    FanOutFanIn(
        StreamQueue<Input>& input,
        StreamQueue<Output>& output,
        TaskFunc processor,
        int numWorkers
    ) : inputQueue_(input), outputQueue_(output), processor_(processor) {
        for (int i = 0; i < numWorkers; i++) {
            workers_.emplace_back([this]() {
                while (auto item = inputQueue_.dequeue()) {
                    Output result = processor_(std::move(*item));
                    outputQueue_.enqueue(std::move(result));
                }
            });
        }
    }
    
    void waitForCompletion() {
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        outputQueue_.close();
    }
    
    ~FanOutFanIn() {
        waitForCompletion();
    }
};

// ===== PATTERN 4: Event/Message Bus =====
class EventBus {
    using Handler = std::function<void(const std::string&)>;
    
    std::mutex mtx_;
    std::unordered_map<std::string, std::vector<Handler>> handlers_;
    
    // Async dispatch queue
    StreamQueue<std::pair<std::string, std::string>> eventQueue_{1000};
    std::thread dispatchThread_;
    
public:
    EventBus() {
        dispatchThread_ = std::thread([this]() {
            while (auto event = eventQueue_.dequeue()) {
                auto& [topic, data] = *event;
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = handlers_.find(topic);
                if (it != handlers_.end()) {
                    for (auto& handler : it->second) {
                        handler(data);
                    }
                }
            }
        });
    }
    
    void subscribe(const std::string& topic, Handler handler) {
        std::lock_guard<std::mutex> lock(mtx_);
        handlers_[topic].push_back(std::move(handler));
    }
    
    void publish(const std::string& topic, const std::string& data) {
        eventQueue_.enqueue({topic, data});
    }
    
    ~EventBus() {
        eventQueue_.close();
        if (dispatchThread_.joinable()) dispatchThread_.join();
    }
};

// ===== PATTERN 5: MapReduce =====
template<typename Input, typename MapOut, typename ReduceOut>
class MapReduce {
public:
    using MapFunc = std::function<std::vector<std::pair<std::string, MapOut>>(const Input&)>;
    using ReduceFunc = std::function<ReduceOut(const std::string&, const std::vector<MapOut>&)>;
    
    std::unordered_map<std::string, ReduceOut> execute(
        const std::vector<Input>& inputs,
        MapFunc mapper,
        ReduceFunc reducer,
        int numMappers = 4
    ) {
        // MAP PHASE
        std::mutex mapMtx;
        std::unordered_map<std::string, std::vector<MapOut>> intermediate;
        
        {
            std::vector<std::thread> mappers_threads;
            std::atomic<size_t> index{0};
            
            for (int i = 0; i < numMappers; i++) {
                mappers_threads.emplace_back([&]() {
                    size_t idx;
                    while ((idx = index.fetch_add(1)) < inputs.size()) {
                        auto mapped = mapper(inputs[idx]);
                        std::lock_guard<std::mutex> lock(mapMtx);
                        for (auto& [key, value] : mapped) {
                            intermediate[key].push_back(std::move(value));
                        }
                    }
                });
            }
            for (auto& t : mappers_threads) t.join();
        }
        
        // REDUCE PHASE
        std::mutex reduceMtx;
        std::unordered_map<std::string, ReduceOut> results;
        
        {
            std::vector<std::thread> reducer_threads;
            auto it = intermediate.begin();
            std::mutex itMtx;
            
            int numReducers = std::min(numMappers, (int)intermediate.size());
            for (int i = 0; i < numReducers; i++) {
                reducer_threads.emplace_back([&]() {
                    while (true) {
                        std::string key;
                        std::vector<MapOut> values;
                        {
                            std::lock_guard<std::mutex> lock(itMtx);
                            if (it == intermediate.end()) return;
                            key = it->first;
                            values = std::move(it->second);
                            ++it;
                        }
                        auto result = reducer(key, values);
                        std::lock_guard<std::mutex> lock(reduceMtx);
                        results[key] = std::move(result);
                    }
                });
            }
            for (auto& t : reducer_threads) t.join();
        }
        
        return results;
    }
};

// ===== USAGE: Word Count MapReduce =====
void mapreduce_demo() {
    MapReduce<std::string, int, int> mr;
    
    std::vector<std::string> documents = {
        "hello world hello",
        "world foo bar",
        "hello bar baz foo"
    };
    
    auto results = mr.execute(
        documents,
        // Map: document -> [(word, 1), ...]
        [](const std::string& doc) -> std::vector<std::pair<std::string, int>> {
            std::vector<std::pair<std::string, int>> result;
            std::istringstream iss(doc);
            std::string word;
            while (iss >> word) {
                result.emplace_back(word, 1);
            }
            return result;
        },
        // Reduce: (word, [1,1,1,...]) -> count
        [](const std::string& key, const std::vector<int>& values) -> int {
            return std::accumulate(values.begin(), values.end(), 0);
        },
        4 // num mappers
    );
    
    for (auto& [word, count] : results) {
        std::cout << word << ": " << count << "\n";
    }
}
```

### 4.4 Concurrent Data Structures

```cpp
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <list>
#include <optional>
#include <functional>
#include <vector>

// ===== CONCURRENT HASH MAP (Sharded/Striped) =====
template<typename K, typename V, size_t NumShards = 16>
class ConcurrentHashMap {
    struct Shard {
        mutable std::shared_mutex mtx;
        std::unordered_map<K, V> map;
    };
    
    std::array<Shard, NumShards> shards_;
    
    size_t getShard(const K& key) const {
        return std::hash<K>{}(key) % NumShards;
    }
    
public:
    void insert(const K& key, const V& value) {
        auto& shard = shards_[getShard(key)];
        std::unique_lock lock(shard.mtx);
        shard.map[key] = value;
    }
    
    std::optional<V> get(const K& key) const {
        auto& shard = shards_[getShard(key)];
        std::shared_lock lock(shard.mtx);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) return it->second;
        return std::nullopt;
    }
    
    bool erase(const K& key) {
        auto& shard = shards_[getShard(key)];
        std::unique_lock lock(shard.mtx);
        return shard.map.erase(key) > 0;
    }
    
    // Compute if absent (like Java's computeIfAbsent)
    V getOrCompute(const K& key, std::function<V(const K&)> factory) {
        auto& shard = shards_[getShard(key)];
        
        // Try read first
        {
            std::shared_lock lock(shard.mtx);
            auto it = shard.map.find(key);
            if (it != shard.map.end()) return it->second;
        }
        
        // Upgrade to write
        std::unique_lock lock(shard.mtx);
        // Double-check
        auto [it, inserted] = shard.map.try_emplace(key);
        if (inserted) {
            it->second = factory(key);
        }
        return it->second;
    }
    
    void forEach(std::function<void(const K&, const V&)> fn) const {
        for (auto& shard : shards_) {
            std::shared_lock lock(shard.mtx);
            for (auto& [k, v] : shard.map) {
                fn(k, v);
            }
        }
    }
    
    size_t size() const {
        size_t total = 0;
        for (auto& shard : shards_) {
            std::shared_lock lock(shard.mtx);
            total += shard.map.size();
        }
        return total;
    }
};

// ===== THREAD-SAFE LRU CACHE (Google Interview Favorite) =====
template<typename K, typename V>
class ThreadSafeLRUCache {
    size_t capacity_;
    std::list<std::pair<K, V>> lruList_; // front = most recent
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
    mutable std::mutex mtx_;
    
public:
    explicit ThreadSafeLRUCache(size_t capacity) : capacity_(capacity) {}
    
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        
        // Move to front (most recently used)
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        return it->second->second;
    }
    
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = map_.find(key);
        
        if (it != map_.end()) {
            // Update existing
            it->second->second = value;
            lruList_.splice(lruList_.begin(), lruList_, it->second);
            return;
        }
        
        // Evict if at capacity
        if (map_.size() >= capacity_) {
            auto& lru = lruList_.back();
            map_.erase(lru.first);
            lruList_.pop_back();
        }
        
        // Insert new
        lruList_.emplace_front(key, value);
        map_[key] = lruList_.begin();
    }
    
    bool remove(const K& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        lruList_.erase(it->second);
        map_.erase(it);
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.size();
    }
};
```

---

## PHASE 5: GOOGLE-STYLE INTERVIEW PROBLEMS

### 5.1 Web Crawler (Multithreaded)

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>

class WebCrawler {
    using GetUrlsFn = std::function<std::vector<std::string>(const std::string&)>;
    
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::string> urlQueue_;
    std::unordered_set<std::string> visited_;
    int activeWorkers_ = 0;
    GetUrlsFn getUrls_;
    std::vector<std::string> results_;
    
public:
    std::vector<std::string> crawl(
        const std::string& startUrl, 
        GetUrlsFn getUrls,
        int numThreads = 8
    ) {
        getUrls_ = getUrls;
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            urlQueue_.push(startUrl);
            visited_.insert(startUrl);
        }
        
        std::vector<std::thread> workers;
        for (int i = 0; i < numThreads; i++) {
            workers.emplace_back(&WebCrawler::worker, this);
        }
        
        for (auto& w : workers) w.join();
        
        return results_;
    }
    
private:
    std::string getHostname(const std::string& url) {
        // Extract hostname from URL
        auto pos = url.find("//");
        if (pos == std::string::npos) return url;
        pos += 2;
        auto end = url.find('/', pos);
        return url.substr(pos, end - pos);
    }
    
    void worker() {
        while (true) {
            std::string url;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this]() {
                    return !urlQueue_.empty() || 
                           (activeWorkers_ == 0 && urlQueue_.empty());
                });
                
                // Termination: no URLs and no active workers
                if (urlQueue_.empty() && activeWorkers_ == 0) {
                    cv_.notify_all();
                    return;
                }
                
                if (urlQueue_.empty()) continue;
                
                url = urlQueue_.front();
                urlQueue_.pop();
                ++activeWorkers_;
            }
            
            // Fetch URLs (outside lock!)
            auto newUrls = getUrls_(url);
            
            {
                std::lock_guard<std::mutex> lock(mtx_);
                results_.push_back(url);
                
                std::string hostname = getHostname(url);
                for (auto& newUrl : newUrls) {
                    if (getHostname(newUrl) == hostname && 
                        visited_.find(newUrl) == visited_.end()) {
                        visited_.insert(newUrl);
                        urlQueue_.push(newUrl);
                    }
                }
                
                --activeWorkers_;
            }
            cv_.notify_all();
        }
    }
};
```

### 5.2 Blocking Queue Variants

```cpp
// ===== PRIORITY BLOCKING QUEUE =====
template<typename T, typename Compare = std::less<T>>
class PriorityBlockingQueue {
    std::priority_queue<T, std::vector<T>, Compare> pq_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    size_t maxSize_;
    std::condition_variable cvFull_;
    bool closed_ = false;
    
public:
    explicit PriorityBlockingQueue(size_t maxSize = SIZE_MAX) 
        : maxSize_(maxSize) {}
    
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cvFull_.wait(lock, [this]() { 
            return pq_.size() < maxSize_ || closed_; 
        });
        if (closed_) return false;
        pq_.push(std::move(item));
        cv_.notify_one();
        return true;
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !pq_.empty() || closed_; });
        if (pq_.empty()) return false;
        item = std::move(const_cast<T&>(pq_.top()));
        pq_.pop();
        cvFull_.notify_one();
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();
        cvFull_.notify_all();
    }
};

// ===== DELAY QUEUE (Execute tasks at scheduled time) =====
#include <chrono>

template<typename T>
class DelayQueue {
    struct DelayedItem {
        T data;
        std::chrono::steady_clock::time_point deadline;
        
        bool operator>(const DelayedItem& other) const {
            return deadline > other.deadline;
        }
    };
    
    std::priority_queue<DelayedItem, std::vector<DelayedItem>, 
                        std::greater<DelayedItem>> pq_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool closed_ = false;
    
public:
    void push(T item, std::chrono::milliseconds delay) {
        std::lock_guard<std::mutex> lock(mtx_);
        pq_.push({std::move(item), 
                  std::chrono::steady_clock::now() + delay});
        cv_.notify_one();
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        while (true) {
            if (closed_ && pq_.empty()) return false;
            
            if (pq_.empty()) {
                cv_.wait(lock);
                continue;
            }
            
            auto deadline = pq_.top().deadline;
            auto now = std::chrono::steady_clock::now();
            
            if (now >= deadline) {
                item = std::move(const_cast<DelayedItem&>(pq_.top()).data);
                pq_.pop();
                return true;
            }
            
            cv_.wait_until(lock, deadline);
        }
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();
    }
};
```

### 5.3 Read-Write Lock with Write Preference and Fairness

```cpp
// ===== FAIR READ-WRITE LOCK (No starvation) =====
// Uses a ticket system: requests served in order
class FairReadWriteLock {
    std::mutex mtx_;
    std::condition_variable cv_;
    
    int activeReaders_ = 0;
    bool activeWriter_ = false;
    
    // Ticket system for fairness
    unsigned long long nextTicket_ = 0;
    unsigned long long servingTicket_ = 0;
    
    struct Waiter {
        unsigned long long ticket;
        bool isWriter;
        bool ready = false;
    };
    
    std::queue<Waiter*> waitQueue_;
    
public:
    void lockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        
        if (!activeWriter_ && waitQueue_.empty()) {
            ++activeReaders_;
            return;
        }
        
        Waiter w{nextTicket_++, false};
        waitQueue_.push(&w);
        
        cv_.wait(lock, [&w]() { return w.ready; });
        
        ++activeReaders_;
    }
    
    void unlockRead() {
        std::unique_lock<std::mutex> lock(mtx_);
        --activeReaders_;
        
        if (activeReaders_ == 0) {
            wakeNext(lock);
        }
    }
    
    void lockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        
        if (!activeWriter_ && activeReaders_ == 0 && waitQueue_.empty()) {
            activeWriter_ = true;
            return;
        }
        
        Waiter w{nextTicket_++, true};
        waitQueue_.push(&w);
        
        cv_.wait(lock, [&w]() { return w.ready; });
        
        activeWriter_ = true;
    }
    
    void unlockWrite() {
        std::unique_lock<std::mutex> lock(mtx_);
        activeWriter_ = false;
        wakeNext(lock);
    }
    
private:
    void wakeNext(std::unique_lock<std::mutex>& lock) {
        if (waitQueue_.empty()) return;
        
        auto* front = waitQueue_.front();
        
        if (front->isWriter) {
            // Wake single writer
            front->ready = true;
            waitQueue_.pop();
            cv_.notify_all();
        } else {
            // Wake all consecutive readers
            while (!waitQueue_.empty() && !waitQueue_.front()->isWriter) {
                waitQueue_.front()->ready = true;
                waitQueue_.pop();
            }
            cv_.notify_all();
        }
    }
};
```

### 5.4 Async Task Scheduler

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>

class TaskScheduler {
    struct ScheduledTask {
        std::function<void()> task;
        std::chrono::steady_clock::time_point executeAt;
        std::chrono::milliseconds interval{0}; // 0 = one-shot
        bool recurring;
        int id;
        
        bool operator>(const ScheduledTask& other) const {
            return executeAt > other.executeAt;
        }
    };
    
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>,
                        std::greater<ScheduledTask>> taskQueue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    bool stopped_ = false;
    std::atomic<int> nextId_{0};
    std::unordered_set<int> cancelledTasks_;
    
public:
    explicit TaskScheduler(int numWorkers = 2) {
        for (int i = 0; i < numWorkers; i++) {
            workers_.emplace_back(&TaskScheduler::workerLoop, this);
        }
    }
    
    // Schedule one-shot task after delay
    int schedule(std::function<void()> task, 
                 std::chrono::milliseconds delay) {
        int id = nextId_++;
        std::lock_guard<std::mutex> lock(mtx_);
        taskQueue_.push({
            std::move(task),
            std::chrono::steady_clock::now() + delay,
            std::chrono::milliseconds{0},
            false,
            id
        });
        cv_.notify_one();
        return id;
    }
    
    // Schedule recurring task
    int scheduleRecurring(std::function<void()> task,
                          std::chrono::milliseconds interval,
                          std::chrono::milliseconds initialDelay = 
                              std::chrono::milliseconds{0}) {
        int id = nextId_++;
        std::lock_guard<std::mutex> lock(mtx_);
        taskQueue_.push({
            std::move(task),
            std::chrono::steady_clock::now() + initialDelay,
            interval,
            true,
            id
        });
        cv_.notify_one();
        return id;
    }
    
    void cancel(int taskId) {
        std::lock_guard<std::mutex> lock(mtx_);
        cancelledTasks_.insert(taskId);
    }
    
    ~TaskScheduler() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }
    
private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(mtx_);
                
                while (true) {
                    if (stopped_) return;
                    if (taskQueue_.empty()) {
                        cv_.wait(lock);
                        continue;
                    }
                    
                    auto& top = taskQueue_.top();
                    auto now = std::chrono::steady_clock::now();
                    
                    if (now >= top.executeAt) {
                        auto scheduledTask = taskQueue_.top();
                        taskQueue_.pop();
                        
                        // Check if cancelled
                        if (cancelledTasks_.count(scheduledTask.id)) {
                            cancelledTasks_.erase(scheduledTask.id);
                            continue;
                        }
                        
                        task = scheduledTask.task;
                        
                        // Re-schedule if recurring
                        if (scheduledTask.recurring) {
                            scheduledTask.executeAt = now + scheduledTask.interval;
                            taskQueue_.push(std::move(scheduledTask));
                        }
                        break;
                    } else {
                        cv_.wait_until(lock, top.executeAt);
                    }
                }
            }
            
            // Execute outside lock
            if (task) task();
        }
    }
};
```

### 5.5 Concurrent Merge K Sorted Streams

```cpp
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <optional>

// ===== MERGE K SORTED STREAMS CONCURRENTLY =====
template<typename T>
class SortedStreamMerger {
    struct StreamEntry {
        T value;
        int streamIndex;
        bool operator>(const StreamEntry& other) const {
            return value > other.value;
        }
    };
    
public:
    using StreamReader = std::function<std::optional<T>()>;
    
    // Merges K sorted streams into one sorted output stream
    void merge(
        std::vector<StreamReader>& streams,
        StreamQueue<T>& output,
        int prefetchSize = 10
    ) {
        std::priority_queue<StreamEntry, std::vector<StreamEntry>,
                            std::greater<StreamEntry>> minHeap;
        
        // Prefetch buffers - one per stream, filled by background threads
        std::vector<StreamQueue<T>> prefetchQueues(streams.size());
        
        // Launch prefetch threads
        std::vector<std::thread> prefetchers;
        for (size_t i = 0; i < streams.size(); i++) {
            prefetchers.emplace_back([&streams, &prefetchQueues, i, prefetchSize]() {
                while (auto val = streams[i]()) {
                    prefetchQueues[i].enqueue(std::move(*val));
                }
                prefetchQueues[i].close();
            });
        }
        
        // Initialize heap with first element from each stream
        for (size_t i = 0; i < prefetchQueues.size(); i++) {
            T val;
            if (auto opt = prefetchQueues[i].dequeue()) {
                minHeap.push({std::move(*opt), (int)i});
            }
        }
        
        // Merge
        while (!minHeap.empty()) {
            auto [value, idx] = minHeap.top();
            minHeap.pop();
            
            output.enqueue(std::move(value));
            
            if (auto opt = prefetchQueues[idx].dequeue()) {
                minHeap.push({std::move(*opt), idx});
            }
        }
        
        output.close();
        
        for (auto& t : prefetchers) t.join();
    }
};
```

---

## PHASE 6: CHEAT SHEET & INTERVIEW TIPS

### Quick Reference

```
┌─────────────────────────────────────────────────────────────────┐
│                    LOCK TYPES CHEAT SHEET                       │
├─────────────────────┬───────────────────────────────────────────┤
│ lock_guard          │ Simple RAII, no flexibility               │
│ unique_lock         │ Flexible: defer, try, timed, moveable    │
│ scoped_lock (C++17) │ Multi-mutex, deadlock-free                │
│ shared_lock         │ Reader lock for shared_mutex              │
├─────────────────────┼───────────────────────────────────────────┤
│ mutex               │ Basic exclusive lock                      │
│ timed_mutex         │ + try_lock_for/until                      │
│ recursive_mutex     │ Same thread can lock multiple times       │
│ shared_mutex        │ Reader-writer lock                        │
├─────────────────────┼───────────────────────────────────────────┤
│ condition_variable  │ Works with unique_lock<mutex>             │
│ condition_var_any   │ Works with any lock type                  │
├─────────────────────┼───────────────────────────────────────────┤
│ atomic<T>           │ Lock-free for fundamental types           │
│ atomic_flag         │ Guaranteed lock-free, test_and_set        │
├─────────────────────┼───────────────────────────────────────────┤
│ counting_semaphore  │ N concurrent accesses (C++20)             │
│ binary_semaphore    │ Cross-thread signaling (C++20)            │
│ latch               │ Single-use countdown (C++20)              │
│ barrier             │ Reusable sync point (C++20)               │
├─────────────────────┼───────────────────────────────────────────┤
│ future/promise      │ One-shot value channel                    │
│ async               │ High-level task dispatch                  │
│ packaged_task       │ Wraps callable for async use              │
│ jthread (C++20)     │ Auto-joining, stoppable                   │
└─────────────────────┴───────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    MEMORY ORDERS                                │
├──────────────────────┬──────────────────────────────────────────┤
│ relaxed              │ Atomicity only, no ordering              │
│ acquire (loads)      │ See all writes before matching release   │
│ release (stores)     │ Make all prior writes visible            │
│ acq_rel             │ Both (for RMW operations)                 │
│ seq_cst (default)   │ Total global order, most intuitive        │
└──────────────────────┴──────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│               COMMON PATTERNS                                   │
├─────────────────────────────────────────────────────────────────┤
│ 1. Producer-Consumer    → Bounded queue + condition variables   │
│ 2. Reader-Writer        → shared_mutex or custom RW lock       │
│ 3. Thread Pool          → Queue + worker threads + futures      │
│ 4. Pipeline             → Chain of queues between stages       │
│ 5. Fan-Out/Fan-In       → Distribute work, collect results     │
│ 6. Barrier Sync         → All threads wait at checkpoint       │
│ 7. Double-Checked Lock  → Singleton pattern optimization       │
│ 8. Monitor Pattern      → Mutex + condition vars encapsulated  │
└─────────────────────────────────────────────────────────────────┘
```

### Interview Strategy

```
WHEN YOU GET A CONCURRENCY QUESTION:

1. CLARIFY
   - How many readers vs writers?
   - Is ordering important?
   - What consistency model is needed?
   - Bounded or unbounded?
   - Graceful shutdown needed?

2. IDENTIFY THE PATTERN
   - Mutual exclusion → mutex/lock_guard
   - Multiple readers → shared_mutex
   - Producer-consumer → queue + condition variable
   - Pipeline → chain of queues
   - Barrier sync → barrier/latch
   - Rate limiting → token bucket
   - Cache → LRU with mutex

3. DESIGN FIRST, CODE SECOND
   - Draw the data flow
   - Identify shared mutable state
   - Decide lock granularity
   - Consider deadlock scenarios

4. WATCH FOR
   - Data races (protect ALL shared state)
   - Deadlock (lock ordering, scoped_lock)
   - Starvation (fairness in RW locks)
   - Spurious wakeups (always use predicate wait)
   - Exception safety (RAII everything)
   - False sharing (alignas(64) for atomics)
   - Lost notifications (check condition before wait)

5. OPTIMIZE
   - Reduce lock scope
   - Use reader-writer locks for read-heavy
   - Lock striping for hash maps
   - Lock-free for hot paths
   - Batch operations to amortize lock cost
```

### Practice Problems (Ordered by Difficulty)

```
EASY:
□ Print in Order (LC 1114)
□ Print FooBar Alternately (LC 1115)
□ Building H2O (LC 1117)
□ Thread-safe counter

MEDIUM:
□ Print Zero Even Odd (LC 1116)
□ FizzBuzz Multithreaded (LC 1195)
□ Bounded Blocking Queue (LC 1188)
□ Thread-safe LRU Cache
□ Producer-Consumer with multiple producers/consumers
□ Reader-Writer Lock from scratch
□ Thread Pool implementation

HARD:
□ Web Crawler Multithreaded (LC 1242)
□ Dining Philosophers (LC 1226)
□ Traffic Light (LC 1279)
□ Design Bounded Blocking Queue with priority
□ Lock-free stack/queue
□ Concurrent merge of K sorted streams
□ MapReduce framework
□ Task scheduler with recurring tasks
□ Fair reader-writer lock (no starvation)
□ Design a concurrent rate limiter
```

This guide covers everything from fundamentals to Google-level problems. Work through each phase sequentially, implement each data structure from scratch, and practice explaining your design decisions out loud — that's what interviewers want to hear.
