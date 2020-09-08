#include <iostream>
#include <vector>
#include <memory>

using namespace std;

template <typename Concrete, typename... Ts>
unique_ptr<Concrete> constructArgs(Ts&&... params) {
    if constexpr (is_constructible_v<Concrete, Ts...>)
        return make_unique<Concrete>(forward<Ts>(params)...);
    else
        return nullptr;
}

template <size_t N>
void f() {
    cout << __func__ << endl;
    if constexpr (N == 2) {
        cout << 2;
    }
}

template <size_t N>

int main() {
    auto ptr = constructArgs<vector<int>>(8,2);    
    for (auto e : *ptr) {
        cout << e;
    }
    cout << endl;
    f<3>();
    f<2>();
}
