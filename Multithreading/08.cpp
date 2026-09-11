//race condition demo

#include <iostream>
#include <thread>
using namespace std;

void increment(int& val){
    val++;
}

int main(){
    int val = 1;
    thread t1(increment, ref(val));
    thread t2(increment, ref(val));
    cout<<val<<endl;

    t1.join();
    t2.join();

}
