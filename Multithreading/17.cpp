#include <atomic>

//basic atomics
void basic_automics(){
    std::atomic<int> x{10};
    //load
    int val = x.load(std::memory_order_seq_cst);

    //store
    x.store(42, std::__1::memory_order_seq_cst);

    //exchange
    int old = x.exchange(100);

    //fetch add
    int before = x.fetch_add(5);
    int after = x.load();

    int expected = 105;
    bool success = x.compare_exchange_strong(expected, 999);



}
