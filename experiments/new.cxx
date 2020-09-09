#include <iostream>
#include <vector>
#include <optional>
#include <string>
#include <cstdio>
#include <variant>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <bitset>
#include <tuple>
#include <memory>

using namespace std;

template <typename A>
struct Tree {
    union {
        A a;
        std::pair<Tree<A>*, Tree<A>*> b;
    };
    enum class State {
        EMPTY,
        NODE,
        LEAF
    } state;
    Tree();
    void insert(bool*, size_t, A);
    optional<A> lookup(bool*);
};

template <typename A>
Tree<A>::Tree() {
   state = State::EMPTY; 
}

template <typename A>
void Tree<A>::insert(bool* v, size_t n, A val) {
    using namespace std;
    if (state == State::EMPTY) {
        if (n == 0) {
            state = State::LEAF;
            a = val;
        }
        else {
            state = State::NODE;
            auto tree1 = new Tree<A>();
            auto tree2 = new Tree<A>();
            tree1->insert(v + 1, n - 1, val);
            if (*v) {
                b = make_pair(tree1, tree2);
            }
            else {
                b = make_pair(tree2, tree1);
            }
        }
    }
    else if (state == State::NODE) {
        if (*v) {
            b.first->insert(v + 1, n - 1, val);
        }
        else {
            b.second->insert(v + 1, n - 1, val);
        }
    }
}

template<typename A>
optional<A> Tree<A>::lookup(bool* bit) {
    if (state == State::EMPTY) return {};
    if (state == State::LEAF) return a;
    if (*bit) return b.first->lookup(bit + 1);
    return b.second->lookup(bit + 1);
}


int main() {
    using namespace std;
    Tree<int> t;    
    bool v[] = {0};
    t.insert(v, 1, 12);
    bool v1[] = {1, 0};
    t.insert(v1, 2, 8);
    bool v2[] = {1, 1};
    t.insert(v2, 2, 9);
    cout << t.lookup(v).value_or(-1);
    return 0;
}
