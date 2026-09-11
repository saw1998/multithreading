
#include <iostream>
#include <thread>
#include <vector>
using namespace std;

// basic thread creation
void simpleFunction(int id){
    cout<<"Thread "<<id<<" running on thread: "<<this_thread::get_id()<<endl;
}

// thread with reference parameters
void modifyValue(int& val){
    val += 100;
}

// thread with class (callable object)
class Worker{
public:
    void operator()(int id) const {
        cout<<"Worker "<<id<<" executing"<<endl;
    }
};

int main(){
    //Method 1: function pointer
    thread t1(simpleFunction, 1);

    // Method 2: Lambda
    thread t2([](int id){
        cout<<"Lambda thread "<<id<<"\n";
    }, 2);

    // Method 3: Callable object
    Worker worker;
    thread t3(worker, 3);

    // Method 4: pass by reference (Must use std::ref)
    int value = 42;
    thread t4(modifyValue, std::ref(value));

    //Method 5: Member function
    //std::thread t5(&MyClass::memberFunc, &obj, arg...);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    cout<<"Modified value: "<<value<<"\n";

    //Spanning multiple thread
    vector<thread> threads;
    for(int i=0;i<10;i++){
        threads.emplace_back(simpleFunction,i);
    }

    for(auto& t:threads){
        t.join();
    }

    return 0;
}
