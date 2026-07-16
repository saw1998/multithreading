# Deep Dive: Advanced Graph Theory & State Space Modeling

## 🎓 BACKGROUND: GRAPH THEORY FOR NON-CS MAJORS

```
WHAT IS A GRAPH?
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Think of a city map:
- NODES (vertices) = intersections
- EDGES = roads between intersections
- WEIGHT = distance/cost of traveling a road

Real-world examples:
- Google Maps: nodes=locations, edges=roads, weight=distance
- Internet: nodes=routers, edges=cables, weight=latency
- Social network: nodes=people, edges=friendships

WHY GRAPH PROBLEMS ARE HARD:
Graphs can have millions of nodes.
Brute force (try all paths) = exponential time.
Smart algorithms (BFS, Dijkstra) = polynomial time.
Google-level = O(V+E) or better.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## PROBLEM 1: CAB SHARING — MULTI-SOURCE BFS INTERSECTION

### Understanding the Problem First

```
SCENARIO:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Alice starts at node A.
Bob   starts at node B.
Both need to reach node DEST.
Every edge costs 1 unit.

WITHOUT sharing: Alice pays dist(A→DEST), Bob pays dist(B→DEST)
WITH sharing: They meet at some intermediate node M.
  Alice pays dist(A→M)
  Bob   pays dist(B→M)
  They SHARE a cab from M to DEST, so they SPLIT the cost.
  Shared cost = dist(M→DEST)  [one cab, not two]

Total cost = dist(A→M) + dist(B→M) + dist(M→DEST)

GOAL: Find M that MINIMIZES this total cost.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

EXAMPLE GRAPH:
     A
    / \
   1   2
  /     \
 3---4---5---DEST
      \
       B

dist(A→3) = 1, dist(A→4) = 2, dist(A→5) = 3
dist(B→4) = 1, dist(B→3) = 2, dist(B→5) = 2
dist(DEST→5)=1, dist(DEST→4)=2, dist(DEST→3)=3

F(3) = 1 + 2 + 3 = 6
F(4) = 2 + 1 + 2 = 5  ← MINIMUM
F(5) = 3 + 2 + 1 = 6

Answer: Meet at node 4, cost = 5
```

### Why Three BFS Runs?

```
NAIVE APPROACH (exponential):
Try all possible meeting points M.
For each M, find shortest path A→M and B→M and M→DEST.
This seems like 3 Dijkstra runs per node = O(V * E log V) → TOO SLOW.

INSIGHT:
BFS from a SINGLE SOURCE computes shortest distance to ALL nodes.
One BFS from A  → gives dist(A→v) for ALL v in O(V+E)
One BFS from B  → gives dist(B→v) for ALL v in O(V+E)
One BFS from DEST → gives dist(DEST→v) for ALL v in O(V+E)

Total: 3 * O(V+E) = O(V+E)

Then scan all nodes once:
  For each v: compute F(v) = dist_A[v] + dist_B[v] + dist_dest[v]
  Return min F(v)

This is the KEY INSIGHT: precompute everything, then scan once.
```

```cpp
#include <iostream>
#include <vector>
#include <queue>
#include <climits>
#include <algorithm>
#include <cassert>
#include <string>
#include <sstream>

/*
 * ═══════════════════════════════════════════════════════════════
 * CLASS: CabSharing
 *
 * PROBLEM:
 * Alice at node 'alice', Bob at node 'bob', destination 'dest'.
 * Each edge costs 1. They can meet at any node M and share a
 * single cab from M to dest (paying once instead of twice).
 *
 * OBJECTIVE:
 * Minimize: dist(alice→M) + dist(bob→M) + dist(M→dest)
 *
 * ALGORITHM:
 * 1. BFS from alice → dist_alice[v] for all v
 * 2. BFS from bob   → dist_bob[v]   for all v
 * 3. BFS from dest  → dist_dest[v]  for all v
 *    (BFS on undirected graph = same as reverse BFS)
 * 4. For each node v: F(v) = sum of 3 distances
 * 5. Return min F(v)
 *
 * WHY UNDIRECTED BFS FROM DEST WORKS:
 * dist(v → dest) = dist(dest → v) on undirected graphs.
 * So BFS from dest gives us shortest path to dest from any node.
 *
 * TIME:  O(V + E) — three BFS runs, one scan
 * SPACE: O(V)     — three distance arrays
 * ═══════════════════════════════════════════════════════════════
 */
class CabSharing {
public:
    /*
     * GRAPH REPRESENTATION:
     * adjacency list: graph[u] = list of neighbors of u
     * Using vector<vector<int>> for unweighted undirected graph.
     *
     * WHY ADJACENCY LIST over MATRIX?
     * Matrix: O(V²) space, bad for sparse graphs
     * List: O(V+E) space, efficient for traversal
     * Most real-world graphs are sparse (E << V²)
     */
    using Graph = std::vector<std::vector<int>>;

    /*
     * BFS: Breadth-First Search
     *
     * MENTAL MODEL:
     * Imagine dropping a stone in water — ripples expand outward.
     * BFS expands level by level from the source.
     *
     * Level 0: source itself
     * Level 1: all nodes 1 hop away
     * Level 2: all nodes 2 hops away
     * ...
     *
     * Since each edge costs 1, the LEVEL = SHORTEST DISTANCE.
     *
     * DATA STRUCTURE: Queue (FIFO)
     * Why queue? Process nodes in ORDER of discovery.
     * First discovered = closest to source.
     * Queue ensures we process all level-k nodes before level-(k+1).
     *
     * VISITED ARRAY:
     * Prevents revisiting nodes (would cause infinite loops in cycles
     * and incorrect distances).
     * dist[v] = -1 means "not yet visited"
     * dist[v] = k means "k hops from source"
     */
    static std::vector<int> bfs(const Graph& graph, int source) {
        int n = graph.size();
        std::vector<int> dist(n, -1); // -1 = not reached

        std::queue<int> queue;
        dist[source] = 0;
        queue.push(source);

        /*
         * BFS LOOP:
         *
         * Each iteration: Take front node u, examine all neighbors.
         * If neighbor v not visited (dist[v] == -1):
         *   - dist[v] = dist[u] + 1  (one more hop)
         *   - Push v to queue (will be processed later)
         *
         * WHY THIS GIVES SHORTEST PATHS:
         * When we FIRST reach a node v, it's via the shortest path.
         * Why? Because BFS processes nodes in non-decreasing distance order.
         * Any later path to v is >= current dist[v].
         *
         * TERMINATION: When queue is empty, all reachable nodes processed.
         */
        while (!queue.empty()) {
            int u = queue.front();
            queue.pop();

            for (int v : graph[u]) {
                if (dist[v] == -1) {        // not yet visited
                    dist[v] = dist[u] + 1;  // one more hop
                    queue.push(v);
                }
            }
        }

        return dist;
        // dist[v] = shortest hops from source to v
        // dist[v] = -1 if v is unreachable
    }

    /*
     * MAIN SOLVER: findMinCost
     *
     * Returns the minimum total cost for cab sharing.
     * Also optionally returns the best meeting node.
     */
    static int findMinCost(
        const Graph& graph,
        int alice,
        int bob,
        int dest,
        int* bestMeetingNode = nullptr
    ) {
        int n = graph.size();

        // Three BFS runs — each O(V+E)
        std::vector<int> dist_alice = bfs(graph, alice);
        std::vector<int> dist_bob   = bfs(graph, bob);
        std::vector<int> dist_dest  = bfs(graph, dest);

        /*
         * Now scan all nodes to find minimum F(v).
         *
         * F(v) = dist_alice[v] + dist_bob[v] + dist_dest[v]
         *
         * EDGE CASE: If any distance is -1 (unreachable),
         * skip this node (cannot be a valid meeting point).
         *
         * Why dist_dest[v] = dist(v → dest)?
         * On undirected graphs, path v→dest = path dest→v reversed.
         * BFS from dest gives dist(dest → v) = dist(v → dest).
         */
        int minCost = INT_MAX;
        int bestNode = -1;

        for (int v = 0; v < n; v++) {
            // Skip unreachable nodes
            if (dist_alice[v] == -1 || dist_bob[v] == -1 || dist_dest[v] == -1) {
                continue;
            }

            int cost = dist_alice[v] + dist_bob[v] + dist_dest[v];

            if (cost < minCost) {
                minCost = cost;
                bestNode = v;
            }
        }

        if (bestMeetingNode) *bestMeetingNode = bestNode;
        return minCost;
    }

    /*
     * HELPER: Build undirected graph from edge list
     */
    static Graph buildGraph(int n, const std::vector<std::pair<int,int>>& edges) {
        Graph g(n);
        for (auto [u, v] : edges) {
            g[u].push_back(v);
            g[v].push_back(u); // undirected
        }
        return g;
    }
};

// ═══════════════════════════════════════════════════════════════
// TESTS WITH STEP-BY-STEP TRACE
// ═══════════════════════════════════════════════════════════════

void test_cab_sharing() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   PROBLEM 1: CAB SHARING             ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";

    // ─── Test 1: LeetCode 2976 (Minimum Cost to Convert String) variant ───
    /*
     * GRAPH:
     *
     *  0 --- 1 --- 2
     *  |           |
     *  3 --- 4 --- 5
     *
     * Alice = 0, Bob = 5, Dest = 2
     *
     * dist_alice: from node 0
     *   0→0=0, 0→1=1, 0→2=2, 0→3=1, 0→4=2, 0→5=3
     *
     * dist_bob: from node 5
     *   5→5=0, 5→4=1, 5→3=2, 5→2=1, 5→1=2, 5→0=3
     *
     * dist_dest: from node 2
     *   2→2=0, 2→1=1, 2→0=2, 2→5=1, 2→4=2, 2→3=3
     *
     * F(0) = 0 + 3 + 2 = 5
     * F(1) = 1 + 2 + 1 = 4
     * F(2) = 2 + 1 + 0 = 3  ← but wait, if dest=2, meeting at dest means
     *                          dist_dest[2]=0, so they both travel to dest
     *                          separately... actually this means:
     *                          Alice travels to 2 (cost 2)
     *                          Bob travels to 2 (cost 1)
     *                          They share from 2 to 2 (cost 0)
     *                          Total = 3 (but they both paid full price!)
     * F(3) = 1 + 2 + 3 = 6
     * F(4) = 2 + 1 + 2 = 5
     * F(5) = 3 + 0 + 1 = 4
     *
     * Minimum F = F(2) = 3 at node 2 (but this is both traveling to dest)
     * or F(1) = 4 at node 1, or F(5) = 4 at node 5
     *
     * WAIT: F(2) = 3 means Alice pays 2, Bob pays 1, sharing pays 0.
     * That IS the minimum! They meet at destination.
     */
    {
        std::cout << "Test 1: Grid-like graph\n";
        std::cout << "  Graph: 0-1-2 with 0-3-4-5 and 2-5\n";
        std::cout << "  Alice=0, Bob=5, Dest=2\n\n";

        int n = 6;
        std::vector<std::pair<int,int>> edges = {
            {0,1},{1,2},{0,3},{3,4},{4,5},{2,5}
        };
        auto g = CabSharing::buildGraph(n, edges);

        int bestNode;
        int result = CabSharing::findMinCost(g, 0, 5, 2, &bestNode);

        // Print distance tables
        auto da = CabSharing::bfs(g, 0);
        auto db = CabSharing::bfs(g, 5);
        auto dd = CabSharing::bfs(g, 2);

        std::cout << "  ┌──────┬────────────┬──────────┬──────────┬──────────┬────────┐\n";
        std::cout << "  │ Node │ dist_alice │ dist_bob │ dist_dest│  F(v)    │ Best?  │\n";
        std::cout << "  ├──────┼────────────┼──────────┼──────────┼──────────┼────────┤\n";

        for (int v = 0; v < n; v++) {
            int fv = (da[v]==-1||db[v]==-1||dd[v]==-1) ? -1
                     : da[v]+db[v]+dd[v];
            std::cout << "  │  " << v << "   │     "
                      << (da[v]==-1?-1:da[v]) << "      │    "
                      << (db[v]==-1?-1:db[v]) << "     │    "
                      << (dd[v]==-1?-1:dd[v]) << "     │    "
                      << fv << "     │  "
                      << (v==bestNode?"← MIN ":"       ") << "│\n";
        }
        std::cout << "  └──────┴────────────┴──────────┴──────────┴──────────┴────────┘\n";
        std::cout << "  \n  Best meeting node: " << bestNode
                  << ", Minimum cost: " << result << "\n\n";
    }

    // ─── Test 2: Linear path ───
    {
        std::cout << "Test 2: Linear path 0-1-2-3-4\n";
        std::cout << "  Alice=0, Bob=4, Dest=4\n";
        std::cout << "  (Bob is already at dest, optimal meeting = dest)\n\n";

        int n = 5;
        std::vector<std::pair<int,int>> edges = {{0,1},{1,2},{2,3},{3,4}};
        auto g = CabSharing::buildGraph(n, edges);

        int bestNode;
        int result = CabSharing::findMinCost(g, 0, 4, 4, &bestNode);
        std::cout << "  Minimum cost: " << result
                  << ", Meeting at: " << bestNode << "\n\n";
    }

    // ─── Test 3: Star graph ───
    {
        std::cout << "Test 3: Star graph (hub=0, leaves=1,2,3,4)\n";
        std::cout << "  Alice=1, Bob=2, Dest=3\n";
        std::cout << "  Best meeting: hub (node 0) since all paths go through it\n\n";

        int n = 5;
        std::vector<std::pair<int,int>> edges = {{0,1},{0,2},{0,3},{0,4}};
        auto g = CabSharing::buildGraph(n, edges);

        int bestNode;
        int result = CabSharing::findMinCost(g, 1, 2, 3, &bestNode);

        auto da = CabSharing::bfs(g, 1);
        auto db = CabSharing::bfs(g, 2);
        auto dd = CabSharing::bfs(g, 3);

        for (int v = 0; v < n; v++) {
            if (da[v]==-1||db[v]==-1||dd[v]==-1) continue;
            std::cout << "  F(" << v << ") = " << da[v] << "+" << db[v]
                      << "+" << dd[v] << " = " << (da[v]+db[v]+dd[v])
                      << (v==bestNode?" ← MIN":"") << "\n";
        }
        std::cout << "  Min cost: " << result << " at node " << bestNode << "\n\n";
    }
}
```

---

## PROBLEM 2: 0-1 BFS — TELEPORTER NETWORK

### The Core Insight: Why Not Dijkstra?

```
PROBLEM SETUP:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Nodes = teleporters
Edges = connections between teleporters

Two types of edges:
  WORKING teleporter → WORKING teleporter: cost 0 (instantaneous)
  PARTIAL teleporter → anything:          cost 1 (takes 1 day)

Goal: Find path A→B with MINIMUM total cost (days wasted).
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

WHY DIJKSTRA IS OVERKILL:
Dijkstra uses a priority queue (binary heap).
Every push/pop = O(log V).
For V nodes, E edges: O(E log V).

BUT: our edge weights are ONLY 0 or 1.
This special structure allows a much better algorithm.

0-1 BFS KEY INSIGHT:
Think of the frontier of nodes we're exploring.
All nodes at current cost K form a "current level."
  - Cost-0 edges: neighbor has SAME cost → stays in current level
  - Cost-1 edges: neighbor has cost K+1 → goes to next level

Regular BFS queue processes nodes level by level.
But when cost-0 edges exist, new nodes should be processed
BEFORE nodes with higher cost.

SOLUTION: Use a DEQUE (double-ended queue)
  - Cost-0 edge neighbor → push to FRONT (process soon)
  - Cost-1 edge neighbor → push to BACK  (process later)

This maintains the invariant: deque is always sorted by cost!
O(V+E) total — same as BFS.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

VISUAL COMPARISON:
                              DIJKSTRA        0-1 BFS
Edge weight processing:       Priority Queue  Deque
Per operation cost:           O(log V)        O(1)
Total complexity:             O(E log V)      O(V+E)
When to use:                  Any weights     Only 0 or 1 weights
```

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * CLASS: TeleporterNetwork
 *
 * GRAPH: Nodes are teleporters.
 *        Edge type determines cost:
 *        - From WORKING node: cost 0 (free travel)
 *        - From PARTIAL node: cost 1 (1 day penalty)
 *
 * ALGORITHM: 0-1 BFS with Deque
 *
 * WHY DEQUE WORKS:
 * Invariant: All nodes in deque are in non-decreasing cost order.
 * - Push to FRONT: maintains order for same-cost nodes
 * - Push to BACK: correctly places higher-cost nodes after
 *
 * This means when we process a node (from front), its cost
 * is MINIMUM possible — same guarantee as Dijkstra but O(V+E).
 *
 * PROOF OF CORRECTNESS:
 * At any point, deque contains nodes with at most 2 distinct costs:
 * {k} and {k+1} for the current minimum cost k.
 * When we pop from front, we always get a node with cost k.
 * When we process its edges:
 *   - 0-cost edge: neighbor gets cost k+0=k → push front (stays in k group)
 *   - 1-cost edge: neighbor gets cost k+1   → push back (goes after k group)
 * After all k-cost nodes processed, front is k+1 → correctly advances.
 * ═══════════════════════════════════════════════════════════════
 */
class TeleporterNetwork {
public:
    /*
     * Node types:
     * WORKING: travel FROM here costs 0
     * PARTIAL: travel FROM here costs 1
     * BROKEN:  cannot travel through (treat as absent)
     */
    enum NodeType { WORKING = 0, PARTIAL = 1, BROKEN = -1 };

    /*
     * Edge representation: {neighbor, cost}
     * Cost is determined by the SOURCE node's type.
     */
    using Edge = std::pair<int, int>; // {neighbor, cost}
    using Graph = std::vector<std::vector<Edge>>;

    /*
     * BUILD GRAPH:
     * Given adjacency list and node types, build weighted graph.
     * Edge cost = type of SOURCE node (where you're traveling FROM).
     *
     * WHY SOURCE NODE DETERMINES COST?
     * "Traveling FROM a partial teleporter takes 1 day."
     * So the cost is on departure, not arrival.
     */
    static Graph buildGraph(
        int n,
        const std::vector<std::pair<int,int>>& rawEdges,
        const std::vector<NodeType>& nodeTypes
    ) {
        Graph g(n);
        for (auto [u, v] : rawEdges) {
            if (nodeTypes[u] == BROKEN || nodeTypes[v] == BROKEN) continue;

            int costUtoV = (nodeTypes[u] == PARTIAL) ? 1 : 0;
            int costVtoU = (nodeTypes[v] == PARTIAL) ? 1 : 0;

            g[u].push_back({v, costUtoV});
            g[v].push_back({u, costVtoU});
        }
        return g;
    }

    /*
     * 0-1 BFS CORE ALGORITHM:
     *
     * STATE: dist[v] = minimum cost to reach v from source
     *
     * DEQUE INVARIANT:
     * Front = nodes with current minimum cost
     * Back  = nodes with current minimum cost + 1
     *
     * STEP BY STEP:
     * 1. Initialize: dist[source] = 0, push source to deque
     * 2. Pop from FRONT (gives minimum cost node)
     * 3. For each edge (u → v, weight w):
     *    - new_cost = dist[u] + w
     *    - If new_cost < dist[v]:  (found better path)
     *      * dist[v] = new_cost
     *      * If w == 0: push_FRONT(v)  [same level, process soon]
     *      * If w == 1: push_BACK(v)   [next level, process later]
     * 4. Repeat until deque empty
     *
     * WHY WE DON'T NEED VISITED ARRAY:
     * A node CAN be re-added to the deque if we find a better path.
     * The dist[v] check ensures we only process improvements.
     * A node is "finally settled" when its dist is minimum — 
     * subsequent processing will find new_cost >= dist[v] and skip.
     */
    static std::vector<int> zeroOneBFS(const Graph& graph, int source) {
        int n = graph.size();
        std::vector<int> dist(n, INT_MAX);

        std::deque<int> dq;
        dist[source] = 0;
        dq.push_back(source);

        while (!dq.empty()) {
            int u = dq.front();
            dq.pop_front();

            for (auto [v, weight] : graph[u]) {
                int newCost = dist[u] + weight;

                if (newCost < dist[v]) {
                    dist[v] = newCost;

                    if (weight == 0) {
                        /*
                         * FREE EDGE: neighbor v has same cost as u.
                         * Push to FRONT so it's processed at current level.
                         *
                         * WHY FRONT?
                         * v has cost = dist[u] + 0 = dist[u].
                         * Other nodes at back have cost = dist[u] + 1.
                         * v should be processed BEFORE those higher-cost nodes.
                         * FRONT ensures this.
                         */
                        dq.push_front(v);
                    } else {
                        /*
                         * COSTLY EDGE: neighbor v has cost dist[u] + 1.
                         * Push to BACK so it's processed at next level.
                         *
                         * WHY BACK?
                         * After all dist[u]-cost nodes processed,
                         * dist[u]+1 cost nodes should be next.
                         * BACK ensures FIFO ordering within same cost level.
                         */
                        dq.push_back(v);
                    }
                }
            }
        }

        return dist;
    }

    /*
     * FIND SHORTEST PATH (returns cost and path nodes)
     */
    static std::pair<int, std::vector<int>> findShortestPath(
        const Graph& graph,
        int source,
        int dest
    ) {
        int n = graph.size();
        std::vector<int> dist(n, INT_MAX);
        std::vector<int> parent(n, -1); // for path reconstruction

        std::deque<int> dq;
        dist[source] = 0;
        dq.push_back(source);

        while (!dq.empty()) {
            int u = dq.front();
            dq.pop_front();

            // Early termination: once dest is settled, stop
            if (u == dest) break;

            for (auto [v, weight] : graph[u]) {
                int newCost = dist[u] + weight;
                if (newCost < dist[v]) {
                    dist[v] = newCost;
                    parent[v] = u; // track how we reached v

                    if (weight == 0) dq.push_front(v);
                    else             dq.push_back(v);
                }
            }
        }

        // Reconstruct path
        std::vector<int> path;
        if (dist[dest] == INT_MAX) return {-1, {}}; // unreachable

        for (int v = dest; v != -1; v = parent[v]) {
            path.push_back(v);
        }
        std::reverse(path.begin(), path.end());

        return {dist[dest], path};
    }
};

// ═══════════════════════════════════════════════════════════════
// TESTS: 0-1 BFS
// ═══════════════════════════════════════════════════════════════

void test_01_bfs() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   PROBLEM 2: 0-1 BFS TELEPORTER      ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";

    using NT = TeleporterNetwork::NodeType;

    // ─── Test 1: Simple linear path ───
    {
        std::cout << "Test 1: Linear path with mixed node types\n";
        std::cout << "  Nodes: 0(W) - 1(W) - 2(P) - 3(W) - 4(W)\n";
        std::cout << "  W=Working(cost 0), P=Partial(cost 1)\n";
        std::cout << "  Source=0, Dest=4\n\n";

        /*
         * MANUAL TRACE:
         * Edge costs (based on SOURCE node type):
         *   0→1: cost 0 (node 0 is WORKING)
         *   1→2: cost 0 (node 1 is WORKING)
         *   2→3: cost 1 (node 2 is PARTIAL)
         *   3→4: cost 0 (node 3 is WORKING)
         *
         * Deque trace:
         * Initial: dist=[0,∞,∞,∞,∞], deque=[0]
         *
         * Pop 0: neighbors {1, cost=0}
         *   dist[1] = 0, push_FRONT(1)
         *   deque=[1]
         *
         * Pop 1: neighbors {0,cost=0},{2,cost=0}
         *   dist[0]=0, skip (not better)
         *   dist[2] = 0, push_FRONT(2)
         *   deque=[2]
         *
         * Pop 2: neighbors {1,cost=1},{3,cost=1}
         *   dist[1]=0, 0+1=1 > 0, skip
         *   dist[3] = 1, push_BACK(3)
         *   deque=[3]
         *
         * Pop 3: neighbors {2,cost=0},{4,cost=0}
         *   dist[2]=0, 1+0=1 > 0, skip
         *   dist[4] = 1, push_FRONT(4)
         *   deque=[4]
         *
         * Pop 4: dest reached!
         *
         * Result: dist[4] = 1 (used partial teleporter once)
         * Path: 0→1→2→3→4
         */

        int n = 5;
        std::vector<std::pair<int,int>> edges = {{0,1},{1,2},{2,3},{3,4}};
        std::vector<NT> types = {NT::WORKING, NT::WORKING, NT::PARTIAL,
                                  NT::WORKING, NT::WORKING};

        auto g = TeleporterNetwork::buildGraph(n, edges, types);
        auto [cost, path] = TeleporterNetwork::findShortestPath(g, 0, 4);

        std::cout << "  Minimum cost (days): " << cost << "\n";
        std::cout << "  Path: ";
        for (size_t i = 0; i < path.size(); i++) {
            std::cout << path[i];
            if (i+1 < path.size()) std::cout << " → ";
        }
        std::cout << "\n  Expected cost: 1\n\n";
    }

    // ─── Test 2: Multiple paths with different costs ───
    {
        std::cout << "Test 2: Diamond graph with shortcut\n";
        std::cout << "  Graph:\n";
        std::cout << "    0(W) → 1(W) → 3(W)    path A: cost 0\n";
        std::cout << "    0(W) → 2(P) → 3(W)    path B: cost 1\n";
        std::cout << "  Source=0, Dest=3\n\n";

        int n = 4;
        std::vector<std::pair<int,int>> edges = {{0,1},{1,3},{0,2},{2,3}};
        std::vector<NT> types = {NT::WORKING, NT::WORKING,
                                  NT::PARTIAL,  NT::WORKING};

        auto g = TeleporterNetwork::buildGraph(n, edges, types);
        auto [cost, path] = TeleporterNetwork::findShortestPath(g, 0, 3);

        std::cout << "  Minimum cost: " << cost << "\n";
        std::cout << "  Path (via all-working nodes): ";
        for (size_t i = 0; i < path.size(); i++) {
            std::cout << path[i];
            if (i+1<path.size()) std::cout << " → ";
        }
        std::cout << "\n  Expected cost: 0 (path 0→1→3)\n\n";
    }

    // ─── Test 3: Forced through partial nodes ───
    {
        std::cout << "Test 3: All intermediate nodes are partial\n";

        int n = 4;
        std::vector<std::pair<int,int>> edges = {{0,1},{1,2},{2,3}};
        std::vector<NT> types = {NT::WORKING, NT::PARTIAL,
                                  NT::PARTIAL,  NT::WORKING};

        auto g = TeleporterNetwork::buildGraph(n, edges, types);
        auto [cost, path] = TeleporterNetwork::findShortestPath(g, 0, 3);

        std::cout << "  Path 0(W)→1(P)→2(P)→3(W)\n";
        std::cout << "  Edge costs: 0→1 free, 1→2 costs 1, 2→3 costs 1\n";
        std::cout << "  Minimum cost: " << cost << " (expected: 2)\n\n";
    }

    // ─── Test 4: Verify 0-1 BFS vs Dijkstra give same answer ───
    {
        std::cout << "Test 4: Comparing 0-1 BFS vs Dijkstra (validation)\n";

        // Build larger random graph
        int n = 7;
        std::vector<std::pair<int,int>> edges = {
            {0,1},{0,2},{1,3},{1,4},{2,4},{2,5},{3,6},{4,6},{5,6}
        };
        std::vector<NT> types = {
            NT::WORKING, NT::PARTIAL, NT::WORKING,
            NT::WORKING, NT::PARTIAL, NT::PARTIAL, NT::WORKING
        };

        auto g = TeleporterNetwork::buildGraph(n, edges, types);
        auto [cost01, path01] = TeleporterNetwork::findShortestPath(g, 0, 6);

        // Simple Dijkstra for comparison
        std::vector<int> dist_dijkstra(n, INT_MAX);
        using PII = std::pair<int,int>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;
        dist_dijkstra[0] = 0;
        pq.push({0, 0});

        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist_dijkstra[u]) continue;
            for (auto [v, w] : g[u]) {
                if (dist_dijkstra[u] + w < dist_dijkstra[v]) {
                    dist_dijkstra[v] = dist_dijkstra[u] + w;
                    pq.push({dist_dijkstra[v], v});
                }
            }
        }

        std::cout << "  0-1 BFS cost:  " << cost01 << "\n";
        std::cout << "  Dijkstra cost: " << dist_dijkstra[6] << "\n";
        std::cout << "  Match: " << (cost01 == dist_dijkstra[6] ? "✓" : "✗") << "\n\n";
    }
}
```

---

## PROBLEM 3: BIPARTITE VERIFICATION & COLORED BINARY TREE

### Understanding Coloring Constraints

```
PROBLEM:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Given: Undirected, acyclic, connected graph where every node
       has degree ≤ 3 (max 3 neighbors).
       (This means the graph is a TREE or close to it.)

Nodes are colored: R, B, W in a cyclic pattern.

Tree layers (top to bottom):
  Layer 0: R  (root)
  Layer 1: B  (children of root)
  Layer 2: W  (grandchildren)
  Layer 3: R  (repeat)
  ...pattern: R→B→W→R→B→W...

CONSTRAINT:
  R node: parent must be W, children must be B
  B node: parent must be R, children must be W
  W node: parent must be B, children must be R

FIND: A valid root node R such that:
  1. Choosing R as root creates a tree following the RBW pattern
  2. R is a valid root = it has NO NEIGHBOR of its "mandatory parent color"
     (Since the root has no parent, if R is chosen as root, it must not
      be adjacent to any node of the color that would be its parent)

Example:
  Root is R: its parent (if existed) would be W.
  If R has a W neighbor → choosing R as root is INVALID
  (because that W neighbor can only be a child of B, not a child of R)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

COLOR CYCLE: R → B → W → R → B → W → ...
  R's parent color = W (predecessor in cycle)
  B's parent color = R
  W's parent color = B

VALID ROOT: node v such that NO neighbor of v has v's parent color.
```

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * CLASS: ColoredTreeRooting
 *
 * GIVEN:
 * - Undirected acyclic connected graph (tree structure)
 * - Each node colored R, B, or W
 * - Color sequence follows R→B→W→R→B→W... layer by layer
 *
 * FIND: Valid root node(s) for this coloring scheme.
 *
 * KEY INVARIANTS:
 * 1. In a valid tree with RBW layering:
 *    - R nodes at depth 0, 3, 6, ... (depth % 3 == 0)
 *    - B nodes at depth 1, 4, 7, ... (depth % 3 == 1)
 *    - W nodes at depth 2, 5, 8, ... (depth % 3 == 2)
 *
 * 2. Parent-child color relationships:
 *    R(parent) → B(child): R is at even depth, B is R's child
 *    B(parent) → W(child): B is at odd depth
 *    W(parent) → R(child): W completes the cycle
 *
 * 3. "Parent color" of each color:
 *    R's parent must be W
 *    B's parent must be R
 *    W's parent must be B
 *
 * ALGORITHM TO FIND VALID ROOT (O(V)):
 * For each node v with color C:
 *   parentColor = predecessorInCycle(C)  // W→R→B→W going backward
 *   If v has NO neighbor with color == parentColor:
 *     v is a VALID ROOT candidate
 *
 * WHY THIS WORKS:
 * If v is a valid root:
 *   - v has no parent (it's the root)
 *   - Any neighbor of v is a CHILD of v
 *   - Children of v must have the CHILD color (successor in cycle)
 *   - If v has a neighbor with the PARENT color → contradiction
 *     (that neighbor can't be v's child, and v can't be its child either
 *      without breaking the cycle)
 *
 * UNIQUENESS:
 * In a valid tree, EXACTLY ONE node can be the root.
 * If 0 valid roots: graph cannot form valid colored tree.
 * If 2+ valid roots: ambiguous, also invalid.
 * ═══════════════════════════════════════════════════════════════
 */
class ColoredTreeRooting {
public:
    enum Color { R = 0, B = 1, W = 2, NONE = -1 };

    static Color charToColor(char c) {
        if (c == 'R') return R;
        if (c == 'B') return B;
        if (c == 'W') return W;
        return NONE;
    }

    static char colorToChar(Color c) {
        if (c == R) return 'R';
        if (c == B) return 'B';
        if (c == W) return 'W';
        return '?';
    }

    /*
     * PARENT COLOR MAPPING:
     * Given a node's color, what color must its parent be?
     *
     * Cycle: R → B → W → R → B → W
     * To find parent: go BACKWARD in cycle
     *
     * R's parent = W (because W → R in forward direction)
     * B's parent = R (because R → B)
     * W's parent = B (because B → W)
     *
     * For a root node: it has NO parent.
     * So a root with color C must have NO neighbor of color parentOf(C).
     */
    static Color parentColor(Color c) {
        // Going backward in R→B→W→R cycle
        if (c == R) return W; // parent of R is W
        if (c == B) return R; // parent of B is R
        if (c == W) return B; // parent of W is B
        return NONE;
    }

    /*
     * CHILD COLOR: what color must children of this node be?
     */
    static Color childColor(Color c) {
        // Going forward in R→B→W→R cycle
        if (c == R) return B; // children of R are B
        if (c == B) return W; // children of B are W
        if (c == W) return R; // children of W are R
        return NONE;
    }

    /*
     * FIND VALID ROOTS:
     *
     * Algorithm:
     * For each node v:
     *   Get v's color C
     *   Get parentColor(C) = P
     *   Check if any neighbor of v has color P
     *   If NO neighbor has color P → v is a valid root
     *
     * STEP BY STEP for each node:
     * 1. Look up color of node v
     * 2. Compute required parent color (what would v's parent be if exists)
     * 3. Scan v's neighbors
     * 4. If ANY neighbor has that parent color → v CANNOT be root
     *    (Because that neighbor would have to be v's parent, but we're
     *     choosing v as root — contradiction)
     */
    static std::vector<int> findValidRoots(
        int n,
        const std::vector<std::vector<int>>& graph,
        const std::vector<Color>& colors
    ) {
        std::vector<int> validRoots;

        for (int v = 0; v < n; v++) {
            Color myColor = colors[v];
            Color myParentColor = parentColor(myColor);

            bool hasParentColorNeighbor = false;

            for (int neighbor : graph[v]) {
                if (colors[neighbor] == myParentColor) {
                    /*
                     * Found a neighbor with v's parent color.
                     * This means v CANNOT be the root.
                     *
                     * WHY?
                     * If v were root, every neighbor is a child.
                     * Children must have childColor(myColor), NOT parentColor.
                     * But this neighbor has parentColor → v can't be root.
                     *
                     * Equivalently: if v is not root, this neighbor would be v's parent.
                     * But then this neighbor has wrong color
                     * (should be parentColor, which it IS — so this neighbor IS v's parent
                     *  in any valid tree containing this edge).
                     * So v is not the root.
                     */
                    hasParentColorNeighbor = true;
                    break;
                }
            }

            if (!hasParentColorNeighbor) {
                validRoots.push_back(v);
            }
        }

        return validRoots;
    }

    /*
     * VERIFY COLORING:
     * Given a root, do BFS to verify all colors match expected pattern.
     * Returns true if coloring is valid.
     *
     * This is O(V+E) and used to VERIFY our root selection.
     */
    static bool verifyColoring(
        int n,
        const std::vector<std::vector<int>>& graph,
        const std::vector<Color>& colors,
        int root
    ) {
        std::vector<int> depth(n, -1);
        std::queue<int> q;
        depth[root] = 0;
        q.push(root);

        while (!q.empty()) {
            int u = q.front(); q.pop();

            // Expected color at this depth
            Color expectedColor = static_cast<Color>(depth[u] % 3);

            if (colors[u] != expectedColor) {
                return false; // Color doesn't match expected
            }

            // Check degree constraint (max 3 for intermediate, max 2 for non-root)
            // For root: max children = 3 (degree ≤ 3, no parent)
            // For others: max children = 2 (degree ≤ 3, minus 1 for parent)

            for (int v : graph[u]) {
                if (depth[v] == -1) { // not yet visited = child
                    depth[v] = depth[u] + 1;
                    q.push(v);
                }
            }
        }

        return true;
    }
};

// ═══════════════════════════════════════════════════════════════
// TESTS: Colored Tree Rooting
// ═══════════════════════════════════════════════════════════════

void test_colored_tree() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║   PROBLEM 3: COLORED BINARY TREE         ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";

    using Color = ColoredTreeRooting::Color;

    // ─── Test 1: Perfect RBW tree, root is node 0 ───
    {
        std::cout << "Test 1: Perfect 3-level RBW tree\n";
        std::cout << "  Structure:\n";
        std::cout << "       0(R)         ← layer 0\n";
        std::cout << "      /    \\\n";
        std::cout << "    1(B)   2(B)     ← layer 1\n";
        std::cout << "    / \\   / \\\n";
        std::cout << "  3(W)4(W)5(W)6(W) ← layer 2\n\n";

        /*
         * Node 0 (R): neighbors are 1(B) and 2(B).
         *   parentColor(R) = W.
         *   Neither 1(B) nor 2(B) is W.
         *   → Node 0 IS a valid root. ✓
         *
         * Node 1 (B): neighbors are 0(R), 3(W), 4(W).
         *   parentColor(B) = R.
         *   Node 0 is R → neighbor has parent color!
         *   → Node 1 is NOT a valid root. ✗
         *
         * Node 3 (W): neighbors are 1(B).
         *   parentColor(W) = B.
         *   Node 1 is B → neighbor has parent color!
         *   → Node 3 is NOT a valid root. ✗
         */

        int n = 7;
        std::vector<std::vector<int>> graph(n);
        auto addEdge = [&](int u, int v) {
            graph[u].push_back(v);
            graph[v].push_back(u);
        };
        addEdge(0,1); addEdge(0,2);
        addEdge(1,3); addEdge(1,4);
        addEdge(2,5); addEdge(2,6);

        std::vector<Color> colors = {Color::R, Color::B, Color::B,
                                      Color::W, Color::W, Color::W, Color::W};

        auto validRoots = ColoredTreeRooting::findValidRoots(n, graph, colors);

        std::cout << "  Analysis per node:\n";
        for (int v = 0; v < n; v++) {
            Color pc = ColoredTreeRooting::parentColor(colors[v]);
            bool hasParentNeighbor = false;
            for (int nb : graph[v]) {
                if (colors[nb] == pc) { hasParentNeighbor = true; break; }
            }
            std::cout << "  Node " << v
                      << " (" << ColoredTreeRooting::colorToChar(colors[v]) << ")"
                      << ": parentColor="
                      << ColoredTreeRooting::colorToChar(pc)
                      << ", hasParentNeighbor=" << (hasParentNeighbor ? "yes" : "no")
                      << (hasParentNeighbor ? " → NOT ROOT" : " → VALID ROOT ✓")
                      << "\n";
        }

        std::cout << "\n  Valid roots: ";
        for (int r : validRoots) std::cout << r << " ";
        std::cout << "\n";

        // Verify
        if (!validRoots.empty()) {
            bool valid = ColoredTreeRooting::verifyColoring(n, graph, colors, validRoots[0]);
            std::cout << "  Coloring verification from root "
                      << validRoots[0] << ": " << (valid ? "✓ VALID" : "✗ INVALID") << "\n\n";
        }
    }

    // ─── Test 2: Invalid coloring (no valid root) ───
    {
        std::cout << "Test 2: Invalid coloring (no valid root possible)\n";
        std::cout << "  Structure: 0(R) - 1(R) - 2(B)\n";
        std::cout << "  Two R nodes adjacent = impossible to satisfy constraints\n\n";

        int n = 3;
        std::vector<std::vector<int>> graph(n);
        graph[0].push_back(1); graph[1].push_back(0);
        graph[1].push_back(2); graph[2].push_back(1);

        std::vector<Color> colors = {Color::R, Color::R, Color::B};

        auto validRoots = ColoredTreeRooting::findValidRoots(n, graph, colors);

        std::cout << "  Valid roots found: " << validRoots.size()
                  << (validRoots.empty() ? " → NO VALID TREE POSSIBLE" : "") << "\n\n";
    }

    // ─── Test 3: Multiple valid roots (ambiguous) ───
    {
        std::cout << "Test 3: Check for uniqueness of valid root\n";

        // Path: R - B - W - R - B
        // Node 0(R) and 3(R) both might be valid roots
        int n = 5;
        std::vector<std::vector<int>> graph(n);
        for (int i = 0; i < 4; i++) {
            graph[i].push_back(i+1);
            graph[i+1].push_back(i);
        }
        std::vector<Color> colors = {Color::R, Color::B, Color::W,
                                      Color::R, Color::B};

        auto validRoots = ColoredTreeRooting::findValidRoots(n, graph, colors);

        std::cout << "  Path: R(0)-B(1)-W(2)-R(3)-B(4)\n";
        std::cout << "  Valid roots: ";
        for (int r : validRoots) std::cout << r << " ";
        if (validRoots.size() > 1) std::cout << " → MULTIPLE ROOTS (ambiguous)";
        if (validRoots.size() == 1) std::cout << " → UNIQUE ROOT ✓";
        if (validRoots.empty()) std::cout << " → NO VALID ROOT";
        std::cout << "\n\n";
    }
}
```

---

## PROBLEM 4: ZOMBIE AND GATE — MULTI-SOURCE BFS + BINARY SEARCH

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * PROBLEM: Zombie and Gate
 *
 * GRID: Cells are either:
 *   'Z' = zombie (dangerous, repels player)
 *   'G' = gate (destination)
 *   '.' = empty (can walk through)
 *   '#' = wall  (cannot walk through)
 *
 * PLAYER: Starts at some position, wants to reach gate.
 *
 * CONSTRAINT: Maximize minimum distance from ANY zombie
 *             while still reaching the gate.
 *
 * IN OTHER WORDS: Find path from start to gate that
 * maximizes the CLOSEST ZOMBIE DISTANCE along the path.
 *
 * TWO-PHASE ALGORITHM:
 *
 * PHASE 1: Multi-source BFS from ALL zombies simultaneously.
 *   Result: zombieDist[r][c] = distance from (r,c) to nearest zombie.
 *   This is the "SAFETY" of each cell.
 *
 * PHASE 2: Binary search on the safety threshold K.
 *   "Can we reach the gate while staying K+ distance from all zombies?"
 *   BFS/DFS: only traverse cells where zombieDist >= K.
 *   Binary search on K to find maximum feasible K.
 *
 * COMPLEXITY:
 *   Phase 1: O(R*C) — BFS over entire grid
 *   Phase 2: O(R*C * log(R*C)) — binary search with BFS each time
 *   Total: O(R*C * log(R*C))
 *
 * WHY BINARY SEARCH?
 * The answer K has monotone property:
 *   If we can navigate with min-zombie-dist K → can also do it with K-1.
 *   If we cannot do it with K → cannot do it with K+1.
 * Binary search exploits this monotonicity.
 * ═══════════════════════════════════════════════════════════════
 */
class ZombieAndGate {
public:
    using Grid = std::vector<std::string>;
    using Pos  = std::pair<int,int>;

    static const int dx[];
    static const int dy[];

    /*
     * PHASE 1: Multi-Source BFS from all zombies.
     *
     * WHY MULTI-SOURCE?
     * We want min distance to ANY zombie for every cell.
     * Starting BFS from each zombie separately = O(Z * R * C) — too slow.
     *
     * TRICK: Add ALL zombie positions to the BFS queue simultaneously.
     * Initialize their distance as 0.
     * BFS expands from all of them at once.
     *
     * This gives min distance to nearest zombie in ONE BFS = O(R*C).
     *
     * ANALOGY: Imagine all zombies spreading infection simultaneously.
     * Each cell's infection time = distance to nearest zombie.
     */
    static std::vector<std::vector<int>> computeZombieDist(const Grid& grid) {
        int R = grid.size(), C = grid[0].size();
        std::vector<std::vector<int>> dist(R, std::vector<int>(C, -1));
        std::queue<Pos> q;

        // Seed BFS with ALL zombie positions
        for (int r = 0; r < R; r++) {
            for (int c = 0; c < C; c++) {
                if (grid[r][c] == 'Z') {
                    dist[r][c] = 0;
                    q.push({r, c});
                }
            }
        }

        // Standard BFS expansion
        while (!q.empty()) {
            auto [r, c] = q.front();
            q.pop();

            for (int d = 0; d < 4; d++) {
                int nr = r + dx[d], nc = c + dy[d];
                if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
                if (grid[nr][nc] == '#') continue; // wall
                if (dist[nr][nc] != -1) continue;  // already visited

                dist[nr][nc] = dist[r][c] + 1;
                q.push({nr, nc});
            }
        }

        return dist;
    }

    /*
     * CHECK: Can we reach the gate with minimum zombie distance >= minSafety?
     *
     * Standard BFS but only traverse cells where zombieDist >= minSafety.
     */
    static bool canReachGate(
        const Grid& grid,
        const std::vector<std::vector<int>>& zombieDist,
        Pos start,
        int minSafety
    ) {
        int R = grid.size(), C = grid[0].size();
        std::vector<std::vector<bool>> visited(R, std::vector<bool>(C, false));
        std::queue<Pos> q;

        auto [sr, sc] = start;
        if (zombieDist[sr][sc] < minSafety && zombieDist[sr][sc] != -1) return false;

        q.push(start);
        visited[sr][sc] = true;

        while (!q.empty()) {
            auto [r, c] = q.front();
            q.pop();

            if (grid[r][c] == 'G') return true; // reached gate!

            for (int d = 0; d < 4; d++) {
                int nr = r + dx[d], nc = c + dy[d];
                if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
                if (grid[nr][nc] == '#') continue;
                if (visited[nr][nc]) continue;

                // Key constraint: cell must be safe enough
                int safety = zombieDist[nr][nc];
                if (safety != -1 && safety < minSafety) continue;

                visited[nr][nc] = true;
                q.push({nr, nc});
            }
        }

        return false; // didn't reach gate
    }

    /*
     * MAIN SOLVER: Find maximum safe path to gate.
     *
     * BINARY SEARCH on minSafety:
     *   lo = 0 (must reach gate, even if next to zombie)
     *   hi = R + C (max possible distance on grid)
     *
     * For each candidate K:
     *   Check if path exists with all cells having zombieDist >= K.
     *
     * Find maximum K for which this is possible.
     */
    static int solve(const Grid& grid, Pos start) {
        int R = grid.size(), C = grid[0].size();

        // Phase 1: Compute zombie distances
        auto zombieDist = computeZombieDist(grid);

        // Check if gate is even reachable (safety=0 means no constraint)
        if (!canReachGate(grid, zombieDist, start, 0)) {
            return -1; // impossible
        }

        // Phase 2: Binary search on safety level
        int lo = 0, hi = R + C, best = 0;

        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;

            if (canReachGate(grid, zombieDist, start, mid)) {
                best = mid;
                lo = mid + 1; // try to achieve higher safety
            } else {
                hi = mid - 1; // safety too high, lower it
            }
        }

        return best;
    }
};

const int ZombieAndGate::dx[] = {0, 0, 1, -1};
const int ZombieAndGate::dy[] = {1, -1, 0, 0};

void test_zombie_gate() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║   PROBLEM 4: ZOMBIE AND GATE             ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";

    {
        std::cout << "Test 1:\n";
        ZombieAndGate::Grid grid = {
            "S...Z",
            ".....",
            "Z.....",
            "...G."
        };
        // Fix: make uniform width
        grid = {
            "S...Z",
            ".....",
            "Z....",
            "...G."
        };

        std::cout << "  Grid:\n";
        for (size_t r = 0; r < grid.size(); r++) {
            std::cout << "  ";
            for (char c : grid[r]) std::cout << c;
            std::cout << "\n";
        }

        auto zombieDist = ZombieAndGate::computeZombieDist(grid);

        std::cout << "\n  Zombie Distance Map (safety of each cell):\n";
        std::cout << "  ";
        for (size_t r = 0; r < zombieDist.size(); r++) {
            std::cout << "  [";
            for (size_t c = 0; c < zombieDist[r].size(); c++) {
                if (zombieDist[r][c] == -1) std::cout << " ?";
                else std::cout << " " << zombieDist[r][c];
                if (c+1<zombieDist[r].size()) std::cout << ",";
            }
            std::cout << "]\n  ";
        }

        int result = ZombieAndGate::solve(grid, {0, 0});
        std::cout << "\n  Maximum minimum zombie distance: " << result << "\n\n";
    }
}
```

---

## PROBLEM 5: GRID CONNECTIVITY WITH DISJOINT SET UNION (DSU)

### Understanding DSU From Scratch

```
WHAT IS DSU (Union-Find)?
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Imagine you're managing friend groups.
Each person starts in their own group.
When two people become friends, their groups MERGE.
DSU answers: "Are these two people in the same group?"

OPERATIONS:
1. find(x): Which group is x in? (Returns group ID)
2. union(x, y): Merge x's group with y's group

NAIVE: Store group ID for each person.
  union: scan all members → O(N)

SMART: Tree structure where each node points to "parent."
  Root of tree = representative of group.

find(x): Follow parent pointers to root → group ID.

TWO OPTIMIZATIONS:
1. UNION BY RANK: Always attach shorter tree under taller tree.
   Keeps tree height O(log N).

2. PATH COMPRESSION: When finding root, make all nodes on path
   point DIRECTLY to root.
   Amortizes future finds to nearly O(1).

COMBINED: O(α(N)) per operation where α is the
   inverse Ackermann function — practically O(1) for all inputs.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

WHY DSU FOR GRID CONNECTIVITY?
Problem: Place 'X' on grid, check if any X-region spans left to right.

Naive BFS after each placement: O(N² * M²) — too slow.

DSU approach:
- Each X cell is a "node."
- When adjacent X cells exist, UNION them.
- Track: minCol and maxCol of each connected component.
- After each X placement:
  * Union with adjacent X cells
  * Check if any component has minCol=0 AND maxCol=N-1

This gives O(α(NM)) per insertion — essentially O(1).
```

```cpp
/*
 * ═══════════════════════════════════════════════════════════════
 * CLASS: GridConnectivityDSU
 *
 * PROBLEM:
 * Grid of size R×C. Player places 'X' cells one at a time.
 * After each placement, check: does any connected X-region
 * span from column 0 to column C-1?
 *
 * CONNECTED = horizontally or vertically adjacent X cells.
 *
 * ALGORITHM:
 * DSU where each cell (r,c) is a node.
 * Each component tracks: minCol and maxCol of any cell in it.
 *
 * After placing X at (r,c):
 * 1. Create DSU node for (r,c)
 * 2. Check 4 neighbors — if they're X, UNION with them
 * 3. For the component containing (r,c):
 *    Update minCol = min(component.minCol, c)
 *    Update maxCol = max(component.maxCol, c)
 * 4. If component.minCol == 0 AND component.maxCol == C-1:
 *    CONNECTED! (spans full width)
 *
 * CELL TO NODE MAPPING:
 * Node ID = r * C + c (linearizes 2D to 1D)
 *
 * AUGMENTED DSU:
 * Each root stores: minCol, maxCol of its component.
 * When two components merge (union), combine their min/max.
 * ═══════════════════════════════════════════════════════════════
 */
class GridConnectivityDSU {
    int R_, C_;

    // DSU arrays
    std::vector<int>  parent_; // parent[i] = parent of node i
    std::vector<int>  rank_;   // rank[i]   = height of subtree at i
    std::vector<int>  minCol_; // minCol[root] = min column in component
    std::vector<int>  maxCol_; // maxCol[root] = max column in component
    std::vector<bool> isX_;    // isX[node] = is this cell an X?

    int totalComponents_ = 0;

    int cellId(int r, int c) const { return r * C_ + c; }

    const int DX[4] = {0, 0, 1, -1};
    const int DY[4] = {1, -1, 0, 0};

public:
    GridConnectivityDSU(int R, int C)
        : R_(R), C_(C),
          parent_(R*C), rank_(R*C, 0),
          minCol_(R*C, C), // initialize to "no column" (beyond right edge)
          maxCol_(R*C, -1), // initialize to "no column" (before left edge)
          isX_(R*C, false)
    {
        // Initialize each cell as its own component
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    /*
     * FIND with PATH COMPRESSION:
     *
     * NAIVE: Follow parent pointers until parent[x] == x (root).
     * PROBLEM: Tree can degenerate to a chain (height O(N)).
     *
     * PATH COMPRESSION:
     * While finding root, make every node on the path point
     * DIRECTLY to the root. Future finds are O(1).
     *
     * Example (before compression):
     * 4 → 3 → 2 → 1 (root)
     *
     * find(4): traverse 4→3→2→1, then set:
     *   parent[4] = 1
     *   parent[3] = 1
     *   parent[2] = 1 (already)
     *
     * After compression:
     * 4 → 1
     * 3 → 1
     * 2 → 1
     *
     * Future find(4): O(1) — directly to root.
     */
    int find(int x) {
        if (parent_[x] != x) {
            parent_[x] = find(parent_[x]); // recursive path compression
        }
        return parent_[x];
    }

    /*
     * UNION with RANK:
     *
     * Merge the components containing x and y.
     *
     * NAIVE: Just set parent[x] = y.
     * PROBLEM: Can create long chains.
     *
     * UNION BY RANK:
     * Maintain an estimate of tree height ("rank").
     * Always attach the tree with smaller rank under the larger rank.
     *
     * Why? Smaller tree under larger tree → height doesn't increase.
     * If ranks equal → one goes under other → rank increases by 1.
     *
     * Keeps tree height O(log N) — ensures find is fast.
     *
     * AUGMENTATION:
     * When merging, update min/max columns of the new combined component.
     * The root of the merged component stores the combined min/max.
     */
    void unite(int x, int y) {
        int rx = find(x), ry = find(y);
        if (rx == ry) return; // already same component

        // Merge min/max columns
        int newMin = std::min(minCol_[rx], minCol_[ry]);
        int newMax = std::max(maxCol_[rx], maxCol_[ry]);

        // Union by rank
        if (rank_[rx] < rank_[ry]) std::swap(rx, ry);
        // Now rank_[rx] >= rank_[ry]

        parent_[ry] = rx; // attach ry under rx
        if (rank_[rx] == rank_[ry]) rank_[rx]++; // only increases when equal

        // Root of merged component is rx — update its min/max
        minCol_[rx] = newMin;
        maxCol_[rx] = newMax;

        totalComponents_--;
    }

    /*
     * PLACE X:
     *
     * 1. Mark cell as X, initialize its DSU node.
     * 2. Union with all adjacent X cells.
     * 3. Check if resulting component spans full width.
     *
     * Returns true if grid is NOW connected left-to-right.
     */
    bool placeX(int r, int c) {
        int id = cellId(r, c);
        if (isX_[id]) return false; // already X

        // Initialize this cell's component
        isX_[id] = true;
        minCol_[id] = c; // component currently has only this cell
        maxCol_[id] = c;
        totalComponents_++;

        // Union with adjacent X neighbors
        for (int d = 0; d < 4; d++) {
            int nr = r + DX[d], nc = c + DY[d];
            if (nr < 0 || nr >= R_ || nc < 0 || nc >= C_) continue;

            int nid = cellId(nr, nc);
            if (isX_[nid]) {
                unite(id, nid);
            }
        }

        // Check if current component spans full width
        int root = find(id);
        return (minCol_[root] == 0 && maxCol_[root] == C_ - 1);
    }

    // Getters for debugging
    int getMinCol(int r, int c) { return minCol_[find(cellId(r,c))]; }
    int getMaxCol(int r, int c) { return maxCol_[find(cellId(r,c))]; }
    bool isConnectedLeftToRight() const {
        // Check all X cells
        for (int r = 0; r < R_; r++) {
            for (int c = 0; c < C_; c++) {
                int id = r * C_ + c;
                if (!isX_[id]) continue;
                // Need to find root — but find() modifies parent (not const)
                // For const version, we'd store the answer separately
            }
        }
        return false;
    }
};

void test_grid_dsu() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║   PROBLEM 5: GRID CONNECTIVITY WITH DSU  ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";

    {
        std::cout << "Test 1: 4x5 grid, place X cells, detect left-right connection\n\n";

        int R = 4, C = 5;
        GridConnectivityDSU dsu(R, C);

        // Sequence of X placements
        std::vector<std::pair<int,int>> placements = {
            {0,0}, {1,0}, {2,0}, // left column
            {0,2}, {1,2},        // middle
            {0,4}, {1,4}, {2,4}, // right column
            {0,1},               // connects left+mid (row 0)
            {0,3},               // connects mid+right (row 0) — SPANS!
        };

        // Display grid state
        std::vector<std::string> grid(R, std::string(C, '.'));

        for (auto [r, c] : placements) {
            grid[r][c] = 'X';
            bool connected = dsu.placeX(r, c);

            std::cout << "  Placed X at (" << r << "," << c << "):\n";
            for (auto& row : grid) {
                std::cout << "    ";
                for (char ch : row) std::cout << ch << " ";
                std::cout << "\n";
            }

            if (connected) {
                std::cout << "  *** CONNECTED LEFT TO RIGHT! ***\n";
            } else {
                int root_minC = dsu.getMinCol(r, c);
                int root_maxC = dsu.getMaxCol(r, c);
                std::cout << "  Component at (" << r << "," << c
                          << "): columns [" << root_minC << ".." << root_maxC << "]\n";
            }
            std::cout << "\n";

            if (connected) break;
        }
    }

    {
        std::cout << "Test 2: Single row, sequential fills\n";
        int R = 1, C = 5;
        GridConnectivityDSU dsu(R, C);

        std::cout << "  Filling row from left to right:\n";
        for (int c = 0; c < C; c++) {
            bool connected = dsu.placeX(0, c);
            std::cout << "  Col " << c << ": ";
            for (int cc = 0; cc < C; cc++) std::cout << (cc<=c?"X":".");
            std::cout << (connected ? " → CONNECTED!" : "") << "\n";
        }
    }
}
```

---

## PART 6: COMPLETE MAIN + COMPLEXITY SUMMARY

```cpp
int main() {
    std::cout << std::string(60, '═') << "\n";
    std::cout << "ADVANCED GRAPH THEORY — COMPLETE TEST SUITE\n";
    std::cout << std::string(60, '═') << "\n\n";

    test_cab_sharing();
    test_01_bfs();
    test_colored_tree();
    test_zombie_gate();
    test_grid_dsu();

    std::cout << "\n" << std::string(60, '═') << "\n";
    std::cout << "COMPLEXITY SUMMARY TABLE\n";
    std::cout << std::string(60, '═') << "\n";
    std::cout << R"(
┌─────────────────────┬──────────────┬──────────────┬──────────┐
│ Problem             │ Naive        │ Optimal      │ Key Idea │
├─────────────────────┼──────────────┼──────────────┼──────────┤
│ Cab Sharing         │ O(V! paths)  │ O(V+E)       │ 3×BFS    │
│ 0-1 Teleporter      │ O(E log V)   │ O(V+E)       │ Deque    │
│ Colored Tree Root   │ O(V!)        │ O(V)         │ Parent   │
│ Zombie & Gate       │ O((RC)²)     │ O(RC log RC) │ BFS+BS   │
│ Grid Connectivity   │ O(N²M² /op) │ O(α(NM)/op)  │ DSU      │
└─────────────────────┴──────────────┴──────────────┴──────────┘

KEY PATTERNS TO MEMORIZE:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
1. Multi-source optimization → Run BFS/Dijkstra from multiple sources
   then combine. Never try all combinations explicitly.

2. 0-1 BFS → When edge weights are ONLY 0 or 1, use Deque instead
   of Priority Queue. Front=0-cost, Back=1-cost edges.

3. Binary Search on Answer → If "is X feasible?" has monotone property,
   binary search on X and check feasibility via BFS/DFS.

4. DSU for dynamic connectivity → Instead of re-running BFS on each
   update, maintain connected components with union-find.
   Track auxiliary info (min/max col) at component roots.

5. Reverse BFS → dist(v → dest) on undirected graph = dist(dest → v).
   BFS from dest gives distances TO dest from all nodes.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
)";

    return 0;
}
```

---

## INTERVIEW ANSWER FRAMEWORK

```
WHEN FACED WITH A GRAPH PROBLEM IN GOOGLE INTERVIEW:

STEP 1: IDENTIFY THE GRAPH TYPE
  □ Unweighted?         → BFS for shortest paths
  □ Weighted (general)? → Dijkstra O(E log V)
  □ Weights 0 or 1?     → 0-1 BFS O(V+E)  ← GOOGLE LOVES THIS
  □ Negative weights?   → Bellman-Ford
  □ Grid graph?         → BFS/DFS with (dr,dc) array

STEP 2: IDENTIFY MULTI-AGENT/SOURCE PATTERN
  □ Multiple starts?   → Multi-source BFS (add all to queue together)
  □ Multiple targets?  → BFS from target in reverse
  □ Two agents meet?   → 3 BFS runs, minimize sum of distances

STEP 3: IDENTIFY STATE AUGMENTATION
  □ Need min/max tracking? → Augment DSU nodes
  □ Need connectivity?     → DSU with union by rank + path compression
  □ Need feasibility?      → Binary search + BFS feasibility check

STEP 4: SPOT THE OPTIMIZATION
  CLUE                          OPTIMIZATION
  "Find path avoiding X"     →  BFS with filtered traversal
  "Maximize minimum distance" → Multi-source BFS + binary search
  "Dynamic connectivity"     → DSU instead of repeated BFS
  "Color constraints on tree" → Algebraic check, not backtracking
  "Edge weights 0 or 1"      → Deque-based 0-1 BFS, not Dijkstra
```
