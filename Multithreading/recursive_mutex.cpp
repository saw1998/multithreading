#include<mutex>
#include <vector>

class TreeNode{
    std::recursive_mutex mtx_;
    int value_;
    std::vector<TreeNode*> children_;

public:
    void process(){
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        //do work with value_
        for(auto &child : children_){
            child->process();
        }
    }

    void updateAndProcess(int newVal){
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        value_ = newVal;
        process();
    }
};
