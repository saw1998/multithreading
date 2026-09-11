#include <chrono>
#include <exception>
#include<iostream>
#include <stdexcept>
#include <thread>
#include <utility>
using namespace std;

void shortTask(int id, int durationMs){
    cout<<"Thread "<<id<<" Starting"<<endl;
    this_thread::sleep_for(chrono::milliseconds(durationMs));
    cout<<"Thread "<<id<<" Finishing"<<endl;
}

void lifecycle_demo(){
    //create
    thread t(shortTask, 1, 500);

    cout<<"Thread created. Joinable: "<<t.joinable()<<endl; //true
    cout<<"Thread ID: "<<t.get_id()<<endl;

    //waiting
    cout<<"Main thread doing other work...\n";

    //join
    t.join(); //block until thread finishes

    cout<<"After join. Joinable: "<<t.joinable()<<endl; //false

    //Detach
    thread bg(shortTask, 2, 1000);
    bg.detach(); //fire and forget
    cout<<"Detached. Joinable: "<<bg.joinable()<<endl; //0 false

    //bg in now independent - but be careful
    // if main exists before bg finishes -> undefined behaviour!
    this_thread::sleep_for(chrono::milliseconds(1500)); //gives bg time
}

class ManagedThread{
    thread thread_;

public:
    enum class JoinPolicy{JOIN, DETACH};

    JoinPolicy policy_;

    template<typename Fn, typename... Args>
    explicit ManagedThread(JoinPolicy policy, Fn&& fn, Args&&... args)
        : thread_(std::forward<Fn>(fn), forward<Args>(args)...)
        , policy_(policy) {}

    ~ManagedThread(){
        if(thread_.joinable()){
            if(policy_ == JoinPolicy::JOIN){
                cout<<"joining thread"<<endl;
                thread_.join();
            } else {
                cout<<"detaching thread"<<endl;
                thread_.detach();
            }
        }
    }

    //prevent copying (threads are not copyable)
    ManagedThread(const ManagedThread&) = delete;
    ManagedThread& operator=(const ManagedThread&) = delete;

    //Allow moving (transfer ownership)
    ManagedThread(ManagedThread&&) = default;
    ManagedThread& operator=(ManagedThread&&) = default;

    thread::id get_id() const {
        return thread_.get_id();
    }
    bool joinable() const {
        return thread_.joinable();
    }
};

void safe_usage(){
    try{
        ManagedThread t(ManagedThread::JoinPolicy::JOIN, shortTask, 1, 5000);

        throw runtime_error("Something went wrong");
    } catch (std::exception &e){
        cout<<"Caught: "<<e.what()<<endl;
    }
}

int main(){
    // lifecycle_demo();
    safe_usage();
}
