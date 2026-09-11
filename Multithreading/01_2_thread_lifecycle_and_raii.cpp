
// problem : Exception safety
// if exception thrown between thread creation and join(),
// the program calls std::terminate()
//
//
// solution : RAII thread guard
#include <thread>

class ThreadGuard{
    std::thread& t_;

public:

}
