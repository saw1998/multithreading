#include<iostream>
#include <set>
#include <stdexcept>

class FilteredLatencyWindow{
private:
    int K_; //number of values used in avg
    int X_; //number of values ignored
    int windowSize_; //K+X = total window size

    std::multiset<double> high_set_;
    std::multiset<double> low_set_;

    double low_sum_; //running sum of low set
    int total_count_; //how many element currently in both sets

    std::deque<double> window_;

public:
    FilteredLatencyWindow(int K, int X) : K_(K), X_(X), windowSize_(X+K), low_sum_(0.0), total_count_(0){
        if(K <= 0) throw std::invalid_argument("K must be positive");
        if(X < 0) throw std::invalid_argument("X must be non-negative");
    }

    void addReading(double latency){
        if(total_count_ == windowSize_){
            double oldest = window_.front();
            window_.pop_front();

            removeFromSets(oldest);
            total_count_--;
        }
        window_.push_back(latency);

        if(!high_set_.empty() && latency >= *high_set_.begin()){
            high_set_.insert(latency);
        } else {
            low_set_.insert(latency);
            low_sum_ += latency;
        }

        total_count_++;

        rebalance();
    }

    double getAverage() const {
        if(low_set_.empty()){
            throw std::runtime_error("No data in window");
        }
        return low_sum_ / (double)low_set_.size();
    }

    void removeFromSets(double val){
        if(high_set_.find(val) != high_set_.end()){
            high_set_.erase(val);
        } else {
            low_set_.erase(val);
            low_sum_ -= val;
        }
    }

    void rebalance(){
        if(high_set_.size() > X_){
            auto it = high_set_.begin();
            double val = *it;

            low_set_.insert(val);
            low_sum_ += val;
            high_set_.erase(it);
        } else if(high_set_.size() < X_ && !low_set_.empty()){

        }
    }


};
