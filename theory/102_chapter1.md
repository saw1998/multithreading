# Deep Dive: Server Latency & Filtered Sliding Windows

## 🎓 BACKGROUND FOR NON-CS MAJORS: BUILDING THE FOUNDATION

---

## PART 1: WHY DOES THIS PROBLEM EXIST? (Real-World Context)

```
Imagine you work at Google and you're monitoring server health.
Every millisecond, a server reports how long it took to respond
to a request (called "latency" - measured in milliseconds).

THE BUSINESS PROBLEM:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Server latency stream: [45, 23, 67, 89, 12, 156, 34, 78, ...]
                         ↑
                    new reading every millisecond

You want to know: "What is the AVERAGE latency of the last K readings?"

But there's a catch:
Sometimes there are temporary spikes (network hiccup, garbage collection)
that would DISTORT our average. We want to IGNORE the top X highest values.

WHY IGNORE TOP X?
- Top X = outliers/spikes that don't represent real performance
- The remaining K values = the "true" performance picture
- This is called a "trimmed mean" in statistics

EXAMPLE:
Window of last K=5 + X=2 = 7 readings: [45, 23, 67, 156, 12, 89, 34]
After ignoring top X=2 highest (156, 89):
Remaining: [45, 23, 67, 12, 34]
Average = (45+23+67+12+34) / 5 = 181/5 = 36.2ms
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

The ALGORITHMIC CHALLENGE:
New reading arrives every millisecond.
We can't afford to sort all K+X values from scratch each time.
We need to maintain a DYNAMIC sorted structure.
```

---

## PART 2: DATA STRUCTURES YOU NEED TO UNDERSTAND FIRST

### 2.1 The Array (Baseline - Why It's Too Slow)

```
MENTAL MODEL: Think of an array as a row of numbered boxes.
┌───┬───┬───┬───┬───┐
│ 45│ 23│ 67│156│ 12│
└───┴───┴───┴───┴───┘
  0   1   2   3   4

FINDING the maximum: Look at EVERY box → O(N) time
(If N=1000, you check 1000 boxes)

SORTING: Rearrange all boxes → O(N log N) time
(If 1 million readings per second, this kills performance)

For our problem: New reading arrives, old one leaves.
Array approach: Re-sort everything = O((K+X) log(K+X)) per reading
If K=1000, X=100, that's 1100 * log(1100) ≈ 11,000 operations
Per millisecond = 11 BILLION operations per second → IMPOSSIBLE
```

### 2.2 The Deque (Double-Ended Queue)

```
MENTAL MODEL: A line of people where you can add/remove
from BOTH the front and the back.

FRONT ←[12][23][34][45][67]→ BACK

push_back()  → add to right end    O(1)
push_front() → add to left end     O(1)
pop_back()   → remove from right   O(1)
pop_front()  → remove from left    O(1)

WHY IT'S USEFUL: Tracks which elements are "in the window"
in their ARRIVAL ORDER.

LIMITATION: Cannot find the kth-largest element efficiently.
           Cannot remove an arbitrary element efficiently.
```

### 2.3 The Heap (Priority Queue) — The Natural But Flawed Choice

```
MENTAL MODEL: A tree where EVERY parent is larger than its children.
(For Max-Heap)

           156        ← always at the top (maximum)
          /    \
        89      67
       /  \    /  \
      45  23  12   34

OPERATIONS:
push(val) → Add to bottom, "bubble up"     O(log N)
top()     → Peek at maximum                O(1)
pop()     → Remove maximum, "sink down"    O(log N)

THE FATAL FLAW FOR OUR PROBLEM:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Window slides: [45, 23, 67, 156, 12, 89, 34]
New element 78 arrives. Old element 45 leaves.

We need to REMOVE 45 from the middle of the heap.
But we don't know WHERE 45 is in the heap tree!

The heap only guarantees the ROOT is maximum.
To find 45, we'd have to scan ALL elements → O(N) search!

This is called "arbitrary deletion" and standard heaps
cannot do it efficiently.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 2.4 The Self-Balancing BST (std::multiset) — The Solution

```
MENTAL MODEL: A sorted tree that AUTOMATICALLY stays balanced.

std::multiset in C++ is a Red-Black Tree internally.
It keeps ALL elements sorted at ALL times.

Key insight: Unlike heap, BST maintains FULL sorted order.

Example with values [12, 23, 34, 45, 67, 89, 156]:

              45
            /    \
          23      89
         /  \    /  \
        12  34  67  156

OPERATIONS (ALL O(log N)):
insert(val)     → Add val, re-balance
erase(iterator) → Remove specific element, re-balance  ← THIS IS KEY!
begin()         → Iterator to SMALLEST element
rbegin()        → Iterator to LARGEST element

WHY THIS SOLVES OUR PROBLEM:
- We can remove ANY specific element in O(log N)
  (We just need its iterator, which we can store)
- We always know which elements are in "top X" vs "bottom K"
- We maintain a running sum for O(1) average computation

TWO std::multiset APPROACH:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
low_set  = {12, 23, 34, 45, 67}  ← bottom K values (used in average)
high_set = {89, 156}              ← top X values (IGNORED)

Invariant: ALL elements in high_set > ALL elements in low_set
           |high_set| == X  (always exactly X elements)
           |low_set| == K   (always exactly K elements)

When window slides:
1. New element arrives → add to correct set
2. Old element leaves → remove from correct set
3. Rebalance if invariant violated (at most 1 element moves)
4. Average = low_sum / K
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## PART 3: COMPLETE SOLUTION WITH DETAILED EXPLANATION

```cpp
#include <iostream>
#include <vector>
#include <deque>
#include <set>          // std::multiset lives here
#include <stdexcept>
#include <cassert>
#include <iomanip>
#include <sstream>

/*
 * ═══════════════════════════════════════════════════════════════
 * CLASS: FilteredLatencyWindow
 *
 * WHAT IT DOES:
 * Maintains a sliding window of the last (K + X) server latency
 * readings. Returns the average of the bottom K values (i.e.,
 * after discarding the top X highest values).
 *
 * WHY THIS DESIGN:
 * We use TWO std::multiset containers to partition the window:
 *   - high_set: Contains the X largest values (to be IGNORED)
 *   - low_set:  Contains the K smallest values (used in average)
 *
 * We maintain a running sum (low_sum) to compute average in O(1).
 * Every insert/remove is O(log K) due to multiset operations.
 *
 * DATA FLOW (mental model):
 *
 * Stream: [45, 23, 67, 156, 12, 89, 34, 78, ...]
 *                                 ↑
 *                           new arrival
 *
 *   high_set (top X=2): {89, 156}   ← don't count these
 *   low_set  (bot K=5): {12,23,34,45,67}  ← average these
 *                              ↓
 *                    average = 181/5 = 36.2
 *
 * INVARIANT (must hold at all times after initialization):
 *   1. |high_set| == X
 *   2. |low_set|  == K  (or < K if stream has fewer than K values)
 *   3. max(low_set) <= min(high_set)
 *      (Every low element is ≤ every high element)
 * ═══════════════════════════════════════════════════════════════
 */
class FilteredLatencyWindow {
private:
    int K_;          // Number of values used in average
    int X_;          // Number of top values to IGNORE
    int windowSize_; // K + X = total window capacity

    /*
     * WHY std::multiset instead of std::set?
     *
     * std::set: Only stores UNIQUE values.
     *           If two readings are both 89ms, it would only store one!
     *
     * std::multiset: Stores DUPLICATE values.
     *                [89, 89, 89] are stored as three separate entries.
     *
     * Internally: Both are Red-Black Trees (self-balancing BST).
     * All operations: O(log N) where N = number of elements.
     */
    std::multiset<double> high_set_; // Top X values (excluded from avg)
    std::multiset<double> low_set_;  // Bottom K values (included in avg)

    double low_sum_;     // Running sum of low_set_ elements
    int    total_count_; // How many elements currently in both sets

    /*
     * DEQUE: Tracks arrival ORDER of elements.
     *
     * WHY DO WE NEED THIS?
     * multiset is sorted by VALUE, not by arrival time.
     * When window slides, we need to know WHICH element to remove.
     * That's the element that arrived FIRST (oldest element).
     *
     * The deque maintains insertion order:
     * Front = oldest element (next to be evicted)
     * Back  = newest element (just arrived)
     *
     * Example:
     * Arrivals: 45, 23, 67, 156, 12, 89, 34
     * Deque:  [45, 23, 67, 156, 12, 89, 34]
     *           ↑                           ↑
     *         oldest                     newest
     *
     * When 78 arrives (window full at 7):
     * Remove 45 (front of deque) from its multiset
     * Add 78 to deque back
     * Deque: [23, 67, 156, 12, 89, 34, 78]
     */
    std::deque<double> window_; // Tracks arrival order

public:
    /*
     * CONSTRUCTOR
     *
     * K: How many values to average (after filtering)
     * X: How many top values to exclude
     *
     * Validation: K and X must be positive.
     * We don't initialize the sets here — they start empty
     * and fill up as readings arrive.
     */
    FilteredLatencyWindow(int K, int X)
        : K_(K), X_(X), windowSize_(K + X), 
          low_sum_(0.0), total_count_(0) 
    {
        if (K <= 0) throw std::invalid_argument("K must be positive");
        if (X < 0)  throw std::invalid_argument("X must be non-negative");
    }

    /*
     * ═══════════════════════════════════════════════════════
     * METHOD: addReading(double latency)
     *
     * This is the CORE of our solution.
     * Called every time a new latency reading arrives.
     *
     * ALGORITHM (4 steps):
     * Step 1: If window is full, REMOVE the oldest element
     * Step 2: ADD the new element to the correct set
     * Step 3: REBALANCE the two sets to restore invariant
     * Step 4: UPDATE low_sum accordingly
     *
     * TIME COMPLEXITY: O(log K) per call
     *   - Each multiset operation (insert, erase, find) is O(log N)
     *   - N is bounded by K+X, so O(log(K+X)) = O(log K) asymptotically
     * ═══════════════════════════════════════════════════════
     */
    void addReading(double latency) {

        // ─────────────────────────────────────────────────────
        // STEP 1: EVICT oldest element if window is full
        // ─────────────────────────────────────────────────────
        /*
         * WHEN: total elements in both sets == windowSize_ (K + X)
         *
         * WHAT: Remove the front of the deque (oldest reading).
         *       Remove that same value from whichever set contains it.
         *
         * HOW TO FIND WHICH SET?
         * The value is in high_set_ if it's ≥ min(high_set_).
         * More precisely: check if high_set_ contains it.
         *
         * SUBTLE ISSUE with duplicates:
         * If both sets contain the same value (e.g., both have 89),
         * we need to remove from the CORRECT set (not just any set).
         *
         * SOLUTION: Check high_set_ first. If the oldest value exists
         * there, remove one copy from high_set_. Otherwise, remove
         * from low_set_.
         *
         * WHY CHECK high_set_ FIRST?
         * If a value exists in BOTH sets (possible with duplicates),
         * we should preferentially remove from high_set_ when the
         * value is in high range. The correct check is:
         *
         * "Is this value ≥ smallest element in high_set_?"
         * If yes → it belongs to high_set_
         * If no  → it belongs to low_set_
         *
         * Actually the cleanest approach: since we INSERT into the
         * correct set, the oldest element was inserted into exactly
         * one set. We can determine which by value comparison:
         * If value >= *high_set_.begin() AND high_set has it, 
         * it's in high.
         */
        if (total_count_ == windowSize_) {
            double oldest = window_.front();
            window_.pop_front();

            removeFromSets(oldest);
            total_count_--;
        }

        // ─────────────────────────────────────────────────────
        // STEP 2: ADD the new element
        // ─────────────────────────────────────────────────────
        /*
         * WHERE DO WE INSERT?
         *
         * Case A: high_set_ not yet full (|high_set_| < X_)
         *   → Insert into high_set_ (it gets excluded from average)
         *   → We'll rebalance afterward if needed
         *
         * Case B: high_set_ is full (|high_set_| == X_)
         *   Compare new value to minimum of high_set_:
         *
         *   If new_val >= min(high_set_):
         *     → new value BELONGS in high (it's one of top X)
         *     → Insert into high_set_
         *   
         *   Else (new_val < min(high_set_)):
         *     → new value BELONGS in low (it's not among top X)
         *     → Insert into low_set_, update low_sum_
         *
         * WHY THIS WORKS:
         * After inserting, we may need to move ONE element between
         * sets to restore the invariant. This is the "rebalance" step.
         */
        window_.push_back(latency);

        if (!high_set_.empty() && latency >= *high_set_.begin()) {
            // New value is large enough to be in "top X" territory
            high_set_.insert(latency);
        } else {
            // New value goes into "bottom K" territory
            low_set_.insert(latency);
            low_sum_ += latency;
        }

        total_count_++;

        // ─────────────────────────────────────────────────────
        // STEP 3: REBALANCE
        // ─────────────────────────────────────────────────────
        /*
         * WHY REBALANCE?
         *
         * After insertion or deletion, the sizes might be wrong:
         *
         * PROBLEM 1: high_set_ has too many elements (> X_)
         * This happens when we insert a large value into high_set_
         * and high_set_ was already full.
         *
         * SOLUTION: Move the SMALLEST element from high_set_ to low_set_.
         * (The smallest element of high_set_ is the "borderline" element—
         * it should be in low_set_ if high_set_ is overfull)
         *
         * Example:
         * Before: high={89,156,200} (size=3, but X=2 → too many!)
         *         low ={12,23,34,45,67}
         * Move 89 from high to low:
         * After:  high={156,200} (size=2 = X ✓)
         *         low ={12,23,34,45,67,89}
         *
         * BUT WAIT: Now low has K+1 elements. That means we evicted
         * too few. Actually, after eviction (step 1), both sets total
         * K+X-1 elements. After insertion, they total K+X. The sizes
         * should be exactly K and X. Rebalance fixes any discrepancy.
         *
         * PROBLEM 2: high_set_ has too few elements (< X_)
         * This happens when we insert a small value into low_set_
         * and high_set_ was already at capacity (didn't lose an element)
         * OR when a large element was evicted from high_set_.
         *
         * SOLUTION: Move the LARGEST element from low_set_ to high_set_.
         *
         * IMPORTANT: Rebalance happens AT MOST ONCE per operation.
         * Only one element ever needs to move.
         */
        rebalance();
    }

    /*
     * ═══════════════════════════════════════════════════════
     * METHOD: getAverage()
     *
     * Returns average of bottom K values.
     *
     * Since we maintain low_sum_ as a running sum, this is O(1).
     *
     * EDGE CASES:
     * - If fewer than K elements in stream: average of all in low_set_
     * - If stream is empty: throw or return sentinel
     * ═══════════════════════════════════════════════════════
     */
    double getAverage() const {
        if (low_set_.empty()) {
            throw std::runtime_error("No data in window");
        }
        return low_sum_ / static_cast<double>(low_set_.size());
    }

    /*
     * Returns current window contents in arrival order (for debugging)
     */
    std::vector<double> getWindow() const {
        return std::vector<double>(window_.begin(), window_.end());
    }

    /*
     * Returns sorted low_set contents (for debugging/verification)
     */
    std::vector<double> getLowSet() const {
        return std::vector<double>(low_set_.begin(), low_set_.end());
    }

    /*
     * Returns sorted high_set contents (for debugging/verification)
     */
    std::vector<double> getHighSet() const {
        return std::vector<double>(high_set_.begin(), high_set_.end());
    }

    int getK() const { return K_; }
    int getX() const { return X_; }
    int getWindowSize() const { return windowSize_; }
    int getTotalCount() const { return total_count_; }

private:
    /*
     * ═══════════════════════════════════════════════════════
     * HELPER: removeFromSets(double val)
     *
     * Removes ONE COPY of val from whichever set contains it.
     *
     * THE TRICKY DUPLICATE CASE:
     * Suppose val = 89 and both sets have 89 in them.
     * How do we know which set the OLDEST 89 came from?
     *
     * APPROACH:
     * When we inserted the element, it went into high_set_ if it was
     * >= min(high_set_) at THAT TIME, or low_set_ otherwise.
     *
     * Reconstruction: If val is >= the current minimum of high_set_,
     * it's more likely in high_set_. But this can be wrong with duplicates.
     *
     * SAFER APPROACH: Check if high_set_ contains val. If yes, remove
     * from there (preference for high_set_ when ambiguous). If no,
     * remove from low_set_.
     *
     * This maintains correct set sizes because our insertion logic
     * mirrors this logic.
     *
     * WHY IS THIS CORRECT?
     * The invariant is about SET SIZES (|high_set_| == X) and
     * VALUE ORDERING (all high > all low). If we remove from the
     * "wrong" set for a duplicate, the rebalance step corrects it.
     * ═══════════════════════════════════════════════════════
     */
    void removeFromSets(double val) {
        auto it_high = high_set_.find(val);
        if (it_high != high_set_.end()) {
            // val is in high_set_ → remove it from there
            high_set_.erase(it_high);
            // No change to low_sum_ (high elements aren't counted)
        } else {
            // val must be in low_set_
            auto it_low = low_set_.find(val);
            // In correct usage, this should always be found
            if (it_low == low_set_.end()) {
                throw std::logic_error("Element not found in either set");
            }
            low_set_.erase(it_low);
            low_sum_ -= val;
        }
    }

    /*
     * ═══════════════════════════════════════════════════════
     * HELPER: rebalance()
     *
     * Restores the invariant after insert or remove.
     *
     * CASE 1: high_set_ is too BIG (size > X_)
     * ───────────────────────────────────────────
     * Move the MINIMUM of high_set_ to low_set_.
     *
     * WHY THE MINIMUM?
     * high_set_ stores the TOP X values.
     * If it has X+1 values, the (X+1)th largest should NOT be excluded.
     * The (X+1)th largest is the MINIMUM of high_set_.
     *
     * Visual:
     * high = {67, 89, 156}  ← too many! X=2
     * Move 67 (minimum of high):
     * high = {89, 156}      ← correct
     * low  = {12, 23, 34, 45, 67}  ← 67 now counted in average
     *
     * CASE 2: high_set_ is too SMALL (size < X_) AND low_set_ not empty
     * ───────────────────────────────────────────────────────────────────
     * Move the MAXIMUM of low_set_ to high_set_.
     *
     * WHY THE MAXIMUM?
     * We need one more element in high_set_.
     * The element that SHOULD be in high (to be excluded) is the
     * LARGEST element in low_set_ (because high contains the top X).
     *
     * Visual:
     * high = {156}          ← too few! X=2
     * low  = {12, 23, 34, 45, 67, 89}
     * Move 89 (maximum of low):
     * high = {89, 156}      ← correct
     * low  = {12, 23, 34, 45, 67}  ← 89 no longer in average
     * ═══════════════════════════════════════════════════════
     */
    void rebalance() {
        if ((int)high_set_.size() > X_) {
            // high_set_ overfull → move its minimum to low_set_
            /*
             * high_set_.begin() → iterator to the SMALLEST element
             * (multiset is sorted ascending by default)
             *
             *   high = {89, 156}
             *           ↑
             *         begin() points here
             */
            auto it = high_set_.begin();
            double val = *it;

            low_set_.insert(val);
            low_sum_ += val;
            high_set_.erase(it);

        } else if ((int)high_set_.size() < X_ && !low_set_.empty()) {
            // high_set_ underfull → move maximum of low_set_ to high_set_
            /*
             * low_set_.end() → one past the end (not valid)
             * std::prev(low_set_.end()) → iterator to LAST element
             *                            = MAXIMUM element
             *
             *   low = {12, 23, 34, 45, 67}
             *                          ↑
             *              prev(end()) points here
             */
            auto it = std::prev(low_set_.end());
            double val = *it;

            high_set_.insert(val);
            low_sum_ -= val;
            low_set_.erase(it);
        }
        // If sizes are correct, do nothing (most common case)
    }

    /*
     * INVARIANT CHECKER (for testing/debugging only)
     * Verifies all invariants hold after each operation.
     */
    bool checkInvariant() const {
        // Size checks
        if ((int)high_set_.size() > X_) return false;
        if (total_count_ == windowSize_) {
            if ((int)high_set_.size() != X_) return false;
            if ((int)low_set_.size() != K_) return false;
        }

        // Ordering check: max(low) <= min(high)
        if (!low_set_.empty() && !high_set_.empty()) {
            double max_low  = *std::prev(low_set_.end());
            double min_high = *high_set_.begin();
            if (max_low > min_high) return false;
        }

        // Sum check
        double computed_sum = 0;
        for (double v : low_set_) computed_sum += v;
        if (std::abs(computed_sum - low_sum_) > 1e-9) return false;

        // Window check: total elements match
        if ((int)window_.size() != total_count_) return false;

        return true;
    }
};
```

---

## PART 4: STEP-BY-STEP TRACE (DRY RUN)

```cpp
/*
 * Let's trace through K=3, X=2 (window=5)
 * with stream: [50, 20, 80, 10, 60, 30, 90, 40]
 *
 * At any point:
 * - high_set_ must have exactly 2 elements (top 2, excluded)
 * - low_set_  must have exactly 3 elements (bottom 3, averaged)
 * - Invariant: max(low) <= min(high)
 *
 * ═══ INITIAL STATE ═══
 * high_set_ = {}
 * low_set_  = {}
 * low_sum_  = 0
 * window_   = []
 * total     = 0
 *
 * ─── addReading(50) ───────────────────────────────────────────
 * Step 1 (Evict): total=0 < 5, no eviction
 * Step 2 (Insert): high_set_ is empty AND latency(50) not >= min(high)
 *                  Wait, high is empty, so condition:
 *                  !high_set_.empty() → FALSE
 *                  So we go to ELSE: insert into low_set_
 *   low_set_  = {50}
 *   low_sum_  = 50
 * Step 3 (Rebalance):
 *   high_set_.size() = 0 < X_=2 AND low_set_ not empty
 *   → Move max(low) to high:  max(low) = 50
 *   high_set_ = {50}, low_set_ = {}, low_sum_ = 0
 *
 * State: high={50}, low={}, sum=0, window=[50], total=1
 *
 * ─── addReading(20) ───────────────────────────────────────────
 * Step 1: total=1 < 5, no eviction
 * Step 2: !high_set_.empty() → TRUE
 *         20 >= min(high=50)? 20 >= 50? NO
 *         → Insert into low_set_
 *   low_set_ = {20}, low_sum_ = 20
 * Step 3: high_set_.size() = 1 < X_=2 AND low not empty
 *         → Move max(low)=20 to high
 *   high_set_ = {20, 50}, low_set_ = {}, low_sum_ = 0
 *
 * State: high={20,50}, low={}, sum=0, window=[50,20], total=2
 *
 * ─── addReading(80) ───────────────────────────────────────────
 * Step 1: total=2 < 5, no eviction
 * Step 2: !high_set_.empty() → TRUE
 *         80 >= min(high=20)? 80 >= 20? YES
 *         → Insert into high_set_
 *   high_set_ = {20, 50, 80}
 * Step 3: high_set_.size() = 3 > X_=2
 *         → Move min(high)=20 to low
 *   high_set_ = {50, 80}
 *   low_set_  = {20}, low_sum_ = 20
 *
 * State: high={50,80}, low={20}, sum=20, window=[50,20,80], total=3
 *
 * VERIFY: max(low)=20 <= min(high)=50 ✓
 *
 * ─── addReading(10) ───────────────────────────────────────────
 * Step 1: total=3 < 5, no eviction
 * Step 2: 10 >= min(high=50)? NO → Insert into low
 *   low_set_ = {10, 20}, low_sum_ = 30
 * Step 3: high.size()=2 == X_=2, low.size()=2 < K_=3 → no rebalance
 *         (Neither condition triggers)
 *
 * State: high={50,80}, low={10,20}, sum=30, window=[50,20,80,10], total=4
 *
 * ─── addReading(60) ───────────────────────────────────────────
 * Step 1: total=4 < 5, no eviction
 * Step 2: 60 >= min(high=50)? YES → Insert into high
 *   high_set_ = {50, 60, 80}
 * Step 3: high.size()=3 > X_=2
 *         → Move min(high)=50 to low
 *   high_set_ = {60, 80}
 *   low_set_  = {10, 20, 50}, low_sum_ = 80
 *
 * State: high={60,80}, low={10,20,50}, sum=80, window=[50,20,80,10,60], total=5
 *
 * VERIFY: max(low)=50 <= min(high)=60 ✓
 * WINDOW FULL! Average = 80/3 = 26.67
 *
 * ─── addReading(30) ───────────────────────────────────────────
 * Step 1: total=5 == windowSize_=5 → EVICT oldest!
 *         oldest = window_.front() = 50
 *         window_.pop_front() → window=[20,80,10,60]
 *         removeFromSets(50):
 *           high_set_.find(50)? high={60,80} → NOT FOUND
 *           low_set_.find(50)?  low={10,20,50} → FOUND at iterator
 *           low_set_.erase(iterator) → low={10,20}
 *           low_sum_ -= 50 → low_sum_ = 30
 *         total_count_ = 4
 *
 * Step 2: Insert 30
 *         30 >= min(high=60)? NO → Insert into low
 *   low_set_ = {10, 20, 30}, low_sum_ = 60
 * Step 3: high.size()=2 == X_, low.size()=3 == K_ → NO rebalance needed
 *
 * total_count_ = 5
 * State: high={60,80}, low={10,20,30}, sum=60, window=[20,80,10,60,30]
 *
 * VERIFY: Window contains [20,80,10,60,30]
 *         Top 2 = {60, 80} ✓ matches high_set_
 *         Bottom 3 = {10, 20, 30} ✓ matches low_set_
 *         Average = 60/3 = 20.0 ✓
 *
 * ─── addReading(90) ───────────────────────────────────────────
 * Step 1: Evict oldest=20 from window=[80,10,60,30]
 *         removeFromSets(20):
 *           high.find(20)? {60,80} → NOT FOUND
 *           low.find(20)?  {10,20,30} → FOUND
 *           low.erase → low={10,30}, low_sum_=40
 *         total=4
 *
 * Step 2: Insert 90
 *         90 >= min(high=60)? YES → Insert into high
 *   high_set_ = {60, 80, 90}
 * Step 3: high.size()=3 > X_=2
 *         Move min(high)=60 to low:
 *   high={80,90}, low={10,30,60}, low_sum_=100
 *
 * State: high={80,90}, low={10,30,60}, sum=100, window=[80,10,60,30,90]
 * Average = 100/3 = 33.33
 *
 * VERIFY: Window [80,10,60,30,90]
 *         Top 2 = {80, 90} ✓
 *         Bottom 3 = {10, 30, 60} ✓
 *         Average = 100/3 = 33.33 ✓
 */
```

---

## PART 5: COMPLETE TEST SUITE

```cpp
#include <iostream>
#include <vector>
#include <deque>
#include <set>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <random>
#include <chrono>

// [FilteredLatencyWindow class from above goes here]

/*
 * ═══════════════════════════════════════════════════════════════
 * BRUTE FORCE VERIFIER
 *
 * This is NOT efficient (O(N log N) per query) but is CORRECT.
 * We use it to verify our O(log K) solution produces same answers.
 *
 * Strategy:
 * 1. Keep a window_ of last K+X values (deque)
 * 2. When queried, sort the window_ and take mean of bottom K
 *
 * This gives us ground truth for comparison testing.
 * ═══════════════════════════════════════════════════════════════
 */
class BruteForceVerifier {
    int K_, X_, windowSize_;
    std::deque<double> window_;

public:
    BruteForceVerifier(int K, int X) 
        : K_(K), X_(X), windowSize_(K + X) {}

    void addReading(double val) {
        if ((int)window_.size() == windowSize_) {
            window_.pop_front();
        }
        window_.push_back(val);
    }

    double getAverage() const {
        if (window_.empty()) throw std::runtime_error("Empty");
        
        // Copy and sort
        std::vector<double> sorted_vals(window_.begin(), window_.end());
        std::sort(sorted_vals.begin(), sorted_vals.end());
        
        // Take bottom min(K_, actual_size) elements
        int count = std::min(K_, (int)sorted_vals.size());
        // But also ensure we remove top X if we have enough elements
        int total = (int)sorted_vals.size();
        int to_exclude = std::max(0, total - K_);
        
        double sum = 0;
        int used = 0;
        for (int i = 0; i < total - to_exclude; i++) {
            sum += sorted_vals[i];
            used++;
        }
        
        return used > 0 ? sum / used : 0.0;
    }
};

// ═══════════════════════════════════════════════════════════════
// UTILITY: Print state of FilteredLatencyWindow
// ═══════════════════════════════════════════════════════════════
void printState(const FilteredLatencyWindow& fw, const std::string& label = "") {
    if (!label.empty()) std::cout << "\n" << label << "\n";
    
    auto window = fw.getWindow();
    auto low    = fw.getLowSet();
    auto high   = fw.getHighSet();
    
    std::cout << "  Window (arrival order): [";
    for (size_t i = 0; i < window.size(); i++) {
        std::cout << window[i];
        if (i + 1 < window.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    
    std::cout << "  Low set  (bottom K=" << fw.getK() << "): [";
    for (size_t i = 0; i < low.size(); i++) {
        std::cout << low[i];
        if (i + 1 < low.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    
    std::cout << "  High set (top   X=" << fw.getX() << "): [";
    for (size_t i = 0; i < high.size(); i++) {
        std::cout << high[i];
        if (i + 1 < high.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    
    if (!low.empty()) {
        std::cout << "  Average: " << std::fixed << std::setprecision(4) 
                  << fw.getAverage() << "\n";
    }
    std::cout << "  Total elements: " << fw.getTotalCount() << "/" 
              << fw.getWindowSize() << "\n";
}

// ═══════════════════════════════════════════════════════════════
// TEST 1: Basic Functionality (from our trace above)
// ═══════════════════════════════════════════════════════════════
void test_basic() {
    std::cout << "═══ TEST 1: Basic Functionality (K=3, X=2) ═══\n";
    
    FilteredLatencyWindow fw(3, 2);
    BruteForceVerifier    bfv(3, 2);
    
    std::vector<double> stream = {50, 20, 80, 10, 60, 30, 90, 40};
    
    for (double reading : stream) {
        fw.addReading(reading);
        bfv.addReading(reading);
        
        std::string label = "After adding " + std::to_string((int)reading) + ":";
        printState(fw, label);
        
        // Verify against brute force
        if (!fw.getLowSet().empty()) {
            double fw_avg  = fw.getAverage();
            double bfv_avg = bfv.getAverage();
            
            bool match = std::abs(fw_avg - bfv_avg) < 1e-9;
            std::cout << "  Brute force avg: " << std::fixed << std::setprecision(4) 
                      << bfv_avg << "\n";
            std::cout << "  Match: " << (match ? "✓ YES" : "✗ NO") << "\n";
            
            if (!match) {
                std::cerr << "  MISMATCH: fw=" << fw_avg << " bfv=" << bfv_avg << "\n";
            }
        }
        std::cout << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════
// TEST 2: X=0 (No Filtering — should be simple moving average)
// ═══════════════════════════════════════════════════════════════
void test_no_filtering() {
    std::cout << "═══ TEST 2: X=0 (No Filtering, K=3) ═══\n";
    
    FilteredLatencyWindow fw(3, 0);
    BruteForceVerifier    bfv(3, 0);
    
    // With X=0: window size = 3, all 3 values are averaged
    std::vector<double> stream = {10, 20, 30, 40, 50};
    // Expected averages: 10, 15, 20, 30, 40
    
    for (double val : stream) {
        fw.addReading(val);
        bfv.addReading(val);
        
        std::cout << "  Added " << val 
                  << " | Avg = " << std::fixed << std::setprecision(2) 
                  << fw.getAverage()
                  << " | BFV = " << bfv.getAverage();
        
        bool match = std::abs(fw.getAverage() - bfv.getAverage()) < 1e-9;
        std::cout << " | " << (match ? "✓" : "✗") << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════
// TEST 3: All Same Values (Stress Test)
// ═══════════════════════════════════════════════════════════════
void test_all_same() {
    std::cout << "\n═══ TEST 3: All Same Values (K=3, X=2) ═══\n";
    
    FilteredLatencyWindow fw(3, 2);
    BruteForceVerifier    bfv(3, 2);
    
    // All 100ms — average should always be 100
    for (int i = 0; i < 10; i++) {
        fw.addReading(100.0);
        bfv.addReading(100.0);
        
        std::cout << "  After reading " << (i+1) 
                  << " | Avg = " << fw.getAverage()
                  << " | BFV = " << bfv.getAverage() << "\n";
        
        assert(std::abs(fw.getAverage() - 100.0) < 1e-9);
    }
    std::cout << "  All same values test: ✓\n";
}

// ═══════════════════════════════════════════════════════════════
// TEST 4: Ascending Stream (Tests eviction of small values)
// ═══════════════════════════════════════════════════════════════
void test_ascending() {
    std::cout << "\n═══ TEST 4: Ascending Stream (K=2, X=1) ═══\n";
    /*
     * K=2, X=1, window=3
     * Stream: 1, 2, 3, 4, 5, 6, 7
     *
     * At window=3, K=2 (average), X=1 (exclude highest):
     * Window [1,2,3]: exclude 3, avg(1,2)=1.5
     * Window [2,3,4]: exclude 4, avg(2,3)=2.5
     * Window [3,4,5]: exclude 5, avg(3,4)=3.5
     * ...pattern: window bottom K is {n, n+1}, avg = n + 0.5
     */
    FilteredLatencyWindow fw(2, 1);
    BruteForceVerifier    bfv(2, 1);
    
    for (int i = 1; i <= 10; i++) {
        fw.addReading(i);
        bfv.addReading(i);
        
        std::cout << "  Added " << i 
                  << " | Window: " << std::setw(20);
        
        auto w = fw.getWindow();
        std::ostringstream oss;
        oss << "[";
        for (size_t j = 0; j < w.size(); j++) {
            oss << w[j];
            if (j+1 < w.size()) oss << ",";
        }
        oss << "]";
        std::cout << oss.str();
        
        std::cout << " | Avg=" << std::fixed << std::setprecision(1) 
                  << fw.getAverage()
                  << " | BFV=" << bfv.getAverage();
        
        bool match = std::abs(fw.getAverage() - bfv.getAverage()) < 1e-9;
        std::cout << " " << (match ? "✓" : "✗") << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════
// TEST 5: Spike Pattern (Most Realistic Latency Test)
// ═══════════════════════════════════════════════════════════════
void test_spike_pattern() {
    std::cout << "\n═══ TEST 5: Spike Pattern (K=5, X=2) ═══\n";
    std::cout << "  Scenario: Normal latency ~50ms with occasional 500ms spikes\n";
    
    FilteredLatencyWindow fw(5, 2);
    BruteForceVerifier    bfv(5, 2);
    
    // Normal: 50ms, Spike: 500ms
    std::vector<double> stream = {
        50, 50, 50, 500, 50,   // spike in middle
        50, 500, 50, 50, 500,   // multiple spikes
        50, 50, 50, 50, 50,     // back to normal
    };
    
    for (double val : stream) {
        fw.addReading(val);
        bfv.addReading(val);
        
        std::string type = (val > 100) ? "SPIKE" : "normal";
        std::cout << "  Added " << std::setw(3) << val << " (" << type << ")"
                  << " | Filtered avg=" << std::fixed << std::setprecision(2) 
                  << fw.getAverage()
                  << " | Unfiltered check=" << bfv.getAverage();
        
        bool match = std::abs(fw.getAverage() - bfv.getAverage()) < 1e-9;
        std::cout << " " << (match ? "✓" : "✗") << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════
// TEST 6: Duplicate Values (Edge Case)
// ═══════════════════════════════════════════════════════════════
void test_duplicates() {
    std::cout << "\n═══ TEST 6: Duplicate Values (K=2, X=2) ═══\n";
    
    FilteredLatencyWindow fw(2, 2);
    BruteForceVerifier    bfv(2, 2);
    
    // Many duplicates to stress test multiset handling
    std::vector<double> stream = {
        50, 50, 50, 50,    // all same
        30, 50, 50, 70,    // mix with duplicates
        100, 100, 50, 50   // duplicate spikes
    };
    
    int i = 0;
    for (double val : stream) {
        fw.addReading(val);
        bfv.addReading(val);
        
        double fw_avg  = fw.getAverage();
        double bfv_avg = bfv.getAverage();
        bool match = std::abs(fw_avg - bfv_avg) < 1e-9;
        
        std::cout << "  [" << ++i << "] Added " << val 
                  << " | Low=" << std::setw(15);
        
        auto low = fw.getLowSet();
        std::ostringstream oss;
        oss << "{";
        for (size_t j = 0; j < low.size(); j++) {
            oss << low[j];
            if (j+1 < low.size()) oss << ",";
        }
        oss << "}";
        std::cout << oss.str();
        
        std::cout << " | High=";
        auto high = fw.getHighSet();
        oss.str("");
        oss << "{";
        for (size_t j = 0; j < high.size(); j++) {
            oss << high[j];
            if (j+1 < high.size()) oss << ",";
        }
        oss << "}";
        std::cout << oss.str();
        
        std::cout << " | Avg=" << fw_avg 
                  << " " << (match ? "✓" : "✗") << "\n";
        
        if (!match) {
            std::cerr << "    MISMATCH! fw=" << fw_avg << " bfv=" << bfv_avg << "\n";
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// TEST 7: Random Stress Test (1000 readings, verify every step)
// ═══════════════════════════════════════════════════════════════
void test_random_stress() {
    std::cout << "\n═══ TEST 7: Random Stress Test (1000 readings) ═══\n";
    
    const int K = 10, X = 5;
    FilteredLatencyWindow fw(K, X);
    BruteForceVerifier    bfv(K, X);
    
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(1.0, 1000.0);
    
    int mismatches = 0;
    const int NUM_READINGS = 1000;
    
    for (int i = 0; i < NUM_READINGS; i++) {
        double val = dist(rng);
        fw.addReading(val);
        bfv.addReading(val);
        
        double fw_avg  = fw.getAverage();
        double bfv_avg = bfv.getAverage();
        
        if (std::abs(fw_avg - bfv_avg) > 1e-6) {
            mismatches++;
            std::cerr << "  Mismatch at reading " << i 
                      << ": fw=" << fw_avg << " bfv=" << bfv_avg << "\n";
        }
    }
    
    std::cout << "  Processed " << NUM_READINGS << " random readings\n";
    std::cout << "  Mismatches: " << mismatches << "\n";
    std::cout << "  Result: " << (mismatches == 0 ? "✓ ALL PASSED" : "✗ FAILED") << "\n";
}

// ═══════════════════════════════════════════════════════════════
// TEST 8: Performance Benchmark
// ═══════════════════════════════════════════════════════════════
void test_performance() {
    std::cout << "\n═══ TEST 8: Performance Benchmark ═══\n";
    
    const int K = 1000, X = 100;
    const int NUM_OPERATIONS = 1000000;
    
    FilteredLatencyWindow fw(K, X);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(1.0, 1000.0);
    
    // Pre-generate values to avoid measuring random number generation
    std::vector<double> values(NUM_OPERATIONS);
    for (auto& v : values) v = dist(rng);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        fw.addReading(values[i]);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  K=" << K << ", X=" << X << "\n";
    std::cout << "  Operations: " << NUM_OPERATIONS << "\n";
    std::cout << "  Total time: " << duration.count() << " microseconds\n";
    std::cout << "  Per operation: " << (double)duration.count() / NUM_OPERATIONS 
              << " microseconds\n";
    std::cout << "  Final average: " << fw.getAverage() << "ms\n";
}

// ═══════════════════════════════════════════════════════════════
// TEST 9: Edge Cases
// ═══════════════════════════════════════════════════════════════
void test_edge_cases() {
    std::cout << "\n═══ TEST 9: Edge Cases ═══\n";
    
    // Edge case 1: K=1, X=1 (window=2, always report the smaller of 2)
    {
        std::cout << "  Case A: K=1, X=1 (always keep smaller of last 2)\n";
        FilteredLatencyWindow fw(1, 1);
        BruteForceVerifier    bfv(1, 1);
        
        std::vector<double> vals = {100, 50, 200, 30, 150};
        for (double v : vals) {
            fw.addReading(v);
            bfv.addReading(v);
            double fw_a = fw.getAverage(), bfv_a = bfv.getAverage();
            bool match = std::abs(fw_a - bfv_a) < 1e-9;
            std::cout << "    Added " << v << " | min of window = " << fw_a
                      << " " << (match ? "✓" : "✗") << "\n";
        }
    }
    
    // Edge case 2: Very large X (X = K+1, meaning exclude almost everything)
    {
        std::cout << "\n  Case B: K=1, X=9 (exclude 9, keep only minimum of 10)\n";
        FilteredLatencyWindow fw(1, 9);
        BruteForceVerifier    bfv(1, 9);
        
        // After window fills with [1,2,3,4,5,6,7,8,9,10]:
        // Exclude top 9: {2,3,4,5,6,7,8,9,10}
        // Keep bottom 1: {1}
        // Average = 1
        for (int i = 1; i <= 10; i++) {
            fw.addReading(i);
            bfv.addReading(i);
        }
        double fw_a = fw.getAverage(), bfv_a = bfv.getAverage();
        std::cout << "    After adding 1..10: Avg=" << fw_a 
                  << " BFV=" << bfv_a << "\n";
        std::cout << "    Expected: 1.0 " 
                  << (std::abs(fw_a - 1.0) < 1e-9 ? "✓" : "✗") << "\n";
    }
    
    // Edge case 3: Stream smaller than window
    {
        std::cout << "\n  Case C: Only 2 readings but window size = 10\n";
        FilteredLatencyWindow fw(5, 5);
        
        fw.addReading(100);
        fw.addReading(50);
        
        // Only 2 elements, neither set is at capacity
        // Brute force: sort [100,50] = [50,100], exclude min(0, 2-5)=0 top
        // Actually: we have 2 elements, want to exclude top X=5 but only 2 exist
        // Our brute force handles this: take all elements if fewer than K+X
        double avg = fw.getAverage();
        std::cout << "    Reading 100 then 50: Avg=" << avg << "\n";
        std::cout << "    (Should be average of bottom elements in sparse window)\n";
    }
    
    // Edge case 4: Monotonically decreasing stream
    {
        std::cout << "\n  Case D: Decreasing stream (K=3, X=1)\n";
        FilteredLatencyWindow fw(3, 1);
        BruteForceVerifier    bfv(3, 1);
        
        for (int v : {100, 90, 80, 70, 60, 50}) {
            fw.addReading(v);
            bfv.addReading(v);
            double fw_a = fw.getAverage(), bfv_a = bfv.getAverage();
            bool match = std::abs(fw_a - bfv_a) < 1e-9;
            std::cout << "    Added " << v << " | Avg=" << fw_a 
                      << " " << (match ? "✓" : "✗") << "\n";
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << std::fixed << std::setprecision(4);
    
    test_basic();
    test_no_filtering();
    test_all_same();
    test_ascending();
    test_spike_pattern();
    test_duplicates();
    test_random_stress();
    test_performance();
    test_edge_cases();
    
    std::cout << "\n═══ ALL TESTS COMPLETE ═══\n";
    return 0;
}
```

---

## PART 6: THE THREE RELATED PROBLEMS (From Your Table)

### Problem 1: Message Rate Limiter — Sliding Window

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * PROBLEM: Message Rate Limiter
 * 
 * REAL-WORLD: "Allow max N requests per user per time window"
 * Example: Gmail limits to 500 emails/hour, Twitter to 300 tweets/hour
 *
 * NAIVE: Store array of ALL timestamps, scan to count in window → O(N)
 *
 * OPTIMAL: Hash Map + Deque Eviction → O(1) amortized
 *   - Hash Map: user_id → deque of timestamps
 *   - Deque: only stores timestamps within the current window
 *   - When checking: evict expired timestamps from front of deque
 *   - Count = deque size (all within window by invariant)
 *
 * WHY O(1) AMORTIZED?
 * Each timestamp is added ONCE and removed ONCE.
 * Total work = 2N operations for N events = O(N) total = O(1) per event.
 *
 * ═══════════════════════════════════════════════════════════════
 */
#include <unordered_map>
#include <deque>
#include <string>
#include <chrono>
#include <mutex>

class MessageRateLimiter {
    /*
     * STORAGE:
     * userTimestamps_: maps user_id → deque of past event timestamps
     *
     * Why deque?
     * - New events go to BACK  (most recent)
     * - Old events evicted from FRONT (least recent)
     * - This is a FIFO queue — exactly what deque does efficiently
     *
     * Memory bound: Each user stores at most maxMessages_ timestamps.
     * Cannot grow unboundedly even under attack.
     */
    std::unordered_map<std::string, std::deque<long long>> userTimestamps_;
    int maxMessages_;               // Max messages allowed in window
    long long windowDurationMs_;    // Window size in milliseconds
    mutable std::mutex mtx_;        // Thread safety
    
    long long nowMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
    
public:
    MessageRateLimiter(int maxMessages, long long windowMs)
        : maxMessages_(maxMessages), windowDurationMs_(windowMs) {}
    
    /*
     * CORE OPERATION: isAllowed(userId)
     *
     * ALGORITHM:
     * 1. Get current time
     * 2. Find (or create) the deque for this user
     * 3. EVICT all timestamps older than (now - windowDurationMs_)
     *    from the FRONT of the deque (they're outside the window)
     * 4. CHECK if deque size < maxMessages_
     *    If yes: add current timestamp to back, return true (allowed)
     *    If no:  return false (rate limited)
     *
     * WHY THIS WORKS:
     * After eviction, deque contains ONLY timestamps within the window.
     * deque.size() = number of events in the current window.
     *
     * EXAMPLE (maxMessages=3, windowMs=60000 i.e. 60 seconds):
     *
     * Time 0:    User A sends message 1
     *   deque: [0]         size=1 < 3 → ALLOWED
     *
     * Time 10s:  User A sends message 2
     *   deque: [0, 10000]  size=2 < 3 → ALLOWED
     *
     * Time 20s:  User A sends message 3
     *   deque: [0,10000,20000]  size=3 = 3 → ALLOWED (last one)
     *
     * Time 30s:  User A tries message 4
     *   evict: nothing expired yet (oldest is 0, window is 60s, now=30s)
     *   deque: [0,10000,20000]  size=3 = 3 → DENIED
     *
     * Time 65s:  User A tries message 5
     *   evict: 0ms expired (65000 - 0 > 60000), 10000ms expired
     *   deque after evict: [20000]  size=1 < 3 → ALLOWED
     *   deque: [20000, 65000]
     */
    bool isAllowed(const std::string& userId) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        long long now = nowMs();
        auto& timestamps = userTimestamps_[userId];
        
        // Evict expired timestamps from front
        long long cutoff = now - windowDurationMs_;
        while (!timestamps.empty() && timestamps.front() <= cutoff) {
            timestamps.pop_front();
        }
        
        // Check if within limit
        if ((int)timestamps.size() < maxMessages_) {
            timestamps.push_back(now);
            return true;
        }
        
        return false;
    }
    
    /*
     * HOW MANY MESSAGES CAN THIS USER STILL SEND?
     * (Good for returning Retry-After headers in APIs)
     */
    int remainingQuota(const std::string& userId) {
        std::lock_guard<std::mutex> lock(mtx_);
        long long now = nowMs();
        auto it = userTimestamps_.find(userId);
        if (it == userTimestamps_.end()) return maxMessages_;
        
        auto& timestamps = it->second;
        long long cutoff = now - windowDurationMs_;
        
        // Count non-expired
        int active = 0;
        for (auto ts : timestamps) {
            if (ts > cutoff) active++;
        }
        
        return std::max(0, maxMessages_ - active);
    }
    
    /*
     * MEMORY CLEANUP: Remove users with no recent activity
     * Call periodically to prevent map from growing unboundedly.
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mtx_);
        long long now = nowMs();
        long long cutoff = now - windowDurationMs_;
        
        for (auto it = userTimestamps_.begin(); it != userTimestamps_.end(); ) {
            auto& deq = it->second;
            while (!deq.empty() && deq.front() <= cutoff) {
                deq.pop_front();
            }
            if (deq.empty()) {
                it = userTimestamps_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// TEST: Rate Limiter
// ═══════════════════════════════════════════════════════════════
void test_rate_limiter() {
    std::cout << "\n═══ RATE LIMITER TEST (3 messages per 100ms) ═══\n";
    
    // 3 messages per 100ms window
    MessageRateLimiter limiter(3, 100);
    
    auto testUser = [&](const std::string& user, const std::string& scenario) {
        std::cout << "\n  Scenario: " << scenario << " [user=" << user << "]\n";
        for (int i = 0; i < 5; i++) {
            bool allowed = limiter.isAllowed(user);
            std::cout << "    Request " << (i+1) << ": " 
                      << (allowed ? "ALLOWED" : "DENIED") << "\n";
            // Small sleep between requests
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    
    testUser("alice", "5 rapid requests (should deny after 3)");
    
    std::cout << "\n  Waiting 110ms for window to expire...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    
    testUser("alice", "5 requests after window reset (should deny after 3 again)");
}
```

### Problem 2: Stream Triplets with Difference ≤ D

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * PROBLEM: Stream Triplets (≤ D)
 *
 * DEFINITION:
 * Given a stream of numbers, at each point count the number of
 * triplets (i, j, k) where i < j < k (indices) and
 * nums[k] - nums[i] <= D (the range of the triplet ≤ D)
 *
 * REAL-WORLD CONTEXT:
 * "How many triples of network packets have their timestamp range ≤ D?"
 * Used in network anomaly detection — bursts of 3+ events in time window D.
 *
 * NAIVE: For each new element, try all pairs → O(N²) per insertion
 *
 * OPTIMAL: Self-Balancing BST (std::multiset) → O(log N) per insertion
 *
 * KEY INSIGHT:
 * When element nums[k] arrives:
 * Count triplets with nums[k] as the RIGHTMOST element.
 * We need pairs (i, j) where:
 *   - i < j < k (already in stream before k)
 *   - nums[k] - max(nums[i], nums[j]) <= D
 *   - Equivalently: all elements in pair >= nums[k] - D
 *
 * So: count elements in stream so far that are >= nums[k] - D.
 * Call this count M.
 * New triplets with k as rightmost = C(M, 2) = M*(M-1)/2
 * (Choose any 2 of those M elements as i and j)
 *
 * IMPLEMENTATION:
 * Use std::multiset (sorted BST) to maintain stream elements.
 * lower_bound(nums[k] - D) gives iterator to first element >= nums[k] - D.
 * Distance from that iterator to end = M.
 *
 * But std::distance on multiset iterators is O(N)!
 * We use ORDER STATISTICS TREE instead (or track count differently).
 *
 * SIMPLIFIED APPROACH: Use sorted structure with rank queries.
 *
 * ═══════════════════════════════════════════════════════════════
 */
#include <set>
#include <vector>
#include <iostream>

class StreamTriplets {
    /*
     * WHY std::multiset?
     * - Keeps elements sorted (BST property)
     * - Allows lower_bound() to find elements >= threshold in O(log N)
     * - Allows insert in O(log N)
     * - Allows duplicates
     *
     * LIMITATION:
     * std::multiset::distance is O(N) (iterators aren't random access).
     * For competitive programming with N large, use an Order Statistics Tree
     * or a Fenwick Tree (Binary Indexed Tree).
     *
     * For interviews, std::multiset with careful counting is acceptable.
     */
    std::multiset<int> seen_;   // All elements seen so far
    long long totalTriplets_;   // Running total
    
public:
    StreamTriplets() : totalTriplets_(0) {}
    
    /*
     * ALGORITHM for addElement(val, D):
     *
     * 1. Count M = number of elements in 'seen' that are >= val - D
     *    (These are elements that could form the "start" of a triplet ending at val)
     *    Note: Actually we want elements in range [val-D, val] since
     *    we want max-min <= D, so max <= min + D.
     *    If val is the new element, elements forming triplets with val:
     *    need nums[i] >= val - D (so val - nums[i] <= D)
     *    AND nums[i] <= val (since val >= all elements if stream ascending,
     *    but not in general)
     *
     *    CORRECTED: We want count of elements x in seen where
     *    val - x <= D AND x <= val
     *    So x in range [val - D, val]
     *
     * 2. M elements are already in seen_.
     *    Any 2 of them can be (i, j) for a triplet (i, j, k) where k is val.
     *    New triplets = C(M, 2) = M * (M-1) / 2
     *
     * 3. Insert val into seen_.
     *
     * 4. totalTriplets_ += new triplets
     *
     * EXAMPLE: Stream = [1, 2, 3, 4], D = 2
     * 
     * Add 1: seen={}, M=0, new_triplets=0, seen={1}, total=0
     * Add 2: seen={1}
     *   range=[2-2,2]=[0,2], elements in range: {1} (just 1)
     *   M=1, new_triplets=C(1,2)=0, seen={1,2}, total=0
     * Add 3: seen={1,2}
     *   range=[3-2,3]=[1,3], elements in range: {1,2} (both!)
     *   M=2, new_triplets=C(2,2)=1, seen={1,2,3}, total=1
     *   Triplet: (1,2,3) ✓ (3-1=2 <= D=2)
     * Add 4: seen={1,2,3}
     *   range=[4-2,4]=[2,4], elements in range: {2,3}
     *   M=2, new_triplets=C(2,2)=1, seen={1,2,3,4}, total=2
     *   Triplet: (2,3,4) ✓ (4-2=2 <= D=2)
     *   Note: (1,3,4) has range 4-1=3 > D=2 ✗
     *   Note: (1,2,4) has range 4-1=3 > D=2 ✗
     *
     * Total triplets after [1,2,3,4] with D=2: 2 ✓
     */
    void addElement(int val, int D) {
        // Find elements in range [val-D, val]
        auto lo = seen_.lower_bound(val - D); // first element >= val-D
        auto hi = seen_.upper_bound(val);      // first element >  val
        
        // Count elements in range (this is O(N) for multiset — see note below)
        long long M = std::distance(lo, hi);
        
        // New triplets with val as the rightmost
        long long newTriplets = M * (M - 1) / 2;
        totalTriplets_ += newTriplets;
        
        seen_.insert(val);
    }
    
    long long getTotalTriplets() const { return totalTriplets_; }
};

/*
 * O(log N) VERSION using Fenwick Tree (Binary Indexed Tree)
 * This is the "optimal" solution mentioned in your table.
 *
 * Fenwick Tree gives us O(log N) range count queries.
 * We use coordinate compression to handle arbitrary values.
 */
class FenwickTree {
    std::vector<int> tree_;
    int n_;
    
public:
    explicit FenwickTree(int n) : tree_(n + 1, 0), n_(n) {}
    
    // Add 1 at position i
    void update(int i, int delta = 1) {
        for (++i; i <= n_; i += i & (-i))
            tree_[i] += delta;
    }
    
    // Count elements in range [0, i]
    int query(int i) {
        int sum = 0;
        for (++i; i > 0; i -= i & (-i))
            sum += tree_[i];
        return sum;
    }
    
    // Count elements in range [l, r]
    int query(int l, int r) {
        return l > r ? 0 : query(r) - (l > 0 ? query(l - 1) : 0);
    }
};

/*
 * OPTIMAL: O(log N) per insertion with coordinate compression
 */
class StreamTripletsOptimal {
    FenwickTree* fenwick_;
    std::vector<int> sorted_vals_; // for coordinate compression
    long long total_;
    
public:
    // Pre-process: requires knowing all values upfront (offline version)
    // For online: use dynamic segment tree or skip list
    void solve(const std::vector<int>& stream, int D) {
        // Coordinate compression
        sorted_vals_ = stream;
        std::sort(sorted_vals_.begin(), sorted_vals_.end());
        sorted_vals_.erase(
            std::unique(sorted_vals_.begin(), sorted_vals_.end()),
            sorted_vals_.end()
        );
        
        int n = sorted_vals_.size();
        FenwickTree ft(n);
        total_ = 0;
        
        for (int val : stream) {
            // Find compressed index of val
            int idx = std::lower_bound(sorted_vals_.begin(), sorted_vals_.end(), val) 
                      - sorted_vals_.begin();
            
            // Find compressed index of val - D
            int lo_idx = std::lower_bound(sorted_vals_.begin(), sorted_vals_.end(), val - D)
                         - sorted_vals_.begin();
            
            // Count elements in range [val-D, val] = compressed [lo_idx, idx]
            long long M = ft.query(lo_idx, idx);
            total_ += M * (M - 1) / 2;
            
            ft.update(idx);
        }
    }
    
    long long getTotal() const { return total_; }
};

// ═══════════════════════════════════════════════════════════════
// TEST: Stream Triplets
// ═══════════════════════════════════════════════════════════════
void test_stream_triplets() {
    std::cout << "\n═══ STREAM TRIPLETS TEST ═══\n";
    
    // Test 1: Manual example from explanation
    {
        StreamTriplets st;
        std::vector<int> stream = {1, 2, 3, 4};
        int D = 2;
        
        std::cout << "  Stream: [1,2,3,4], D=" << D << "\n";
        for (int val : stream) {
            st.addElement(val, D);
            std::cout << "    After " << val 
                      << ": total triplets = " << st.getTotalTriplets() << "\n";
        }
        // Expected: 0, 0, 1, 2
        std::cout << "  Final: " << st.getTotalTriplets() << " (expected 2)\n";
    }
    
    // Test 2: All same elements
    {
        StreamTriplets st;
        std::vector<int> stream = {5, 5, 5, 5, 5};
        int D = 0;
        std::cout << "\n  Stream: [5,5,5,5,5], D=0\n";
        for (int val : stream) {
            st.addElement(val, D);
        }
        // C(5,3) = 10 triplets
        std::cout << "  Total: " << st.getTotalTriplets() 
                  << " (expected 10 = C(5,3))\n";
    }
    
    // Test 3: No triplets possible
    {
        StreamTriplets st;
        std::vector<int> stream = {1, 100, 200, 300};
        int D = 5;
        std::cout << "\n  Stream: [1,100,200,300], D=5 (no triplets possible)\n";
        for (int val : stream) {
            st.addElement(val, D);
        }
        std::cout << "  Total: " << st.getTotalTriplets() << " (expected 0)\n";
    }
}

// ═══════════════════════════════════════════════════════════════
// MAIN: Run all tests
// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   FILTERED SLIDING WINDOW - COMPLETE TEST SUITE ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    
    test_basic();
    test_no_filtering();
    test_all_same();
    test_ascending();
    test_spike_pattern();
    test_duplicates();
    test_random_stress();
    test_performance();
    test_edge_cases();
    test_rate_limiter();
    test_stream_triplets();
    
    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║              ALL TESTS COMPLETE                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    
    return 0;
}
```

---

## PART 7: COMPLEXITY ANALYSIS (With Intuition)

```
╔════════════════════════════════════════════════════════════════════╗
║              COMPLEXITY ANALYSIS                                   ║
╠════════════════╦══════════════╦═════════════════╦══════════════════╣
║ Operation      ║ Naive Array  ║ Our BST Solution║ Why BST Wins     ║
╠════════════════╬══════════════╬═════════════════╬══════════════════╣
║ addReading()   ║ O((K+X)logN) ║ O(log K)        ║ BST insert       ║
║ getAverage()   ║ O(K+X)       ║ O(1)            ║ running sum      ║
║ evict element  ║ O(K+X)       ║ O(log K)        ║ BST delete       ║
║ rebalance      ║ O((K+X)logN) ║ O(log K)        ║ single move      ║
╠════════════════╬══════════════╬═════════════════╬══════════════════╣
║ Space          ║ O(K+X)       ║ O(K+X)          ║ Same             ║
╚════════════════╩══════════════╩═════════════════╩══════════════════╝

INTUITION FOR O(log K):

std::multiset is a Red-Black Tree with height = O(log N).
Any operation (find, insert, erase, begin, end, prev, next)
travels at most O(log N) nodes from root to leaf.

With K+X elements total:
  height = O(log(K+X)) = O(log K) (since X < K in practice)

Per addReading():
  - 1 deque operation:       O(1)
  - 1 removeFromSets call:   O(log K)  [find + erase in multiset]
  - 1 insert into multiset:  O(log K)
  - 1 rebalance:             O(log K)  [begin/end/insert/erase]
  Total: O(log K)

For 1 MILLION readings per second with K=1000:
  O(log 1000) ≈ 10 operations per reading
  10 million operations/second = FEASIBLE ✓

Vs. naive O(K log K) = 1000*10 = 10,000 operations per reading
  10 billion operations/second = NOT FEASIBLE ✗

══════════════════════════════════════════════════════════════════════

INTERVIEW FOLLOW-UP QUESTIONS AND ANSWERS:

Q: "Can you do this without std::multiset?"
A: Yes. Use a Hash-Heap:
   - Heap augmented with HashMap<value, set<heap_position>>
   - When evicting, look up position in O(1), delete from heap in O(log K)
   - More complex to implement but same complexity

Q: "What if K and X are very large (millions)?"
A: The BST approach still works. O(log N) with N=10^6 is only ~20 ops.
   For extreme scale, use Fenwick Tree or Segment Tree for O(log N) range
   sum queries.

Q: "What about floating point precision in low_sum_?"
A: Use Kahan summation algorithm for numerical stability:
   (compensated summation that corrects floating-point rounding errors)
   Or use integer arithmetic (store latencies as integer microseconds).

Q: "Can this handle concurrent updates?"
A: Add a std::shared_mutex:
   - Readers (getAverage) use shared_lock
   - Writers (addReading) use unique_lock
   This was covered in the multi-threading section.
```

This complete solution gives you:
1. **Deep intuition** for WHY each data structure is chosen
2. **Complete traced example** showing every state transition  
3. **All edge cases** systematically tested
4. **Brute-force verifier** to validate correctness
5. **Three related problems** (Rate Limiter, Triplets, Filtered Window) with the same paradigm
6. **Complexity analysis** with real-world performance intuition
