#include "RestorePlan.h"
#include <algorithm>
#include <chrono>
#include <iostream>
using namespace mmvr::states;
int main() {
    for (size_t count : {1000, 4000, 8000}) {
        Snapshot snapshot{{"benchmark", "assets", "native"}, 0, {}};
        Bytes destination(count * 32, 0);
        std::vector<RestoreBinding> bindings;
        for (size_t i = 0; i < count; ++i) {
            auto id = "block/" + std::to_string(i);
            snapshot.blocks.push_back({id, 1, Bytes(32, uint8_t(i)), {}});
            bindings.push_back({id, 1, {destination.data() + i * 32, 32}});
        }
        std::reverse(bindings.begin(), bindings.end());
        for (int repeat = 0; repeat < 5; ++repeat) {
            const auto start = std::chrono::steady_clock::now();
            RestorePlan plan(snapshot, bindings, {});
            const auto end = std::chrono::steady_clock::now();
            plan.Commit();
            for (size_t i = 0; i < count; ++i)
                if (destination[i * 32] != uint8_t(i)) return 1;
            std::cout << count << ',' << repeat << ','
                      << std::chrono::duration<double, std::milli>(end - start).count() << '\n';
        }
    }
}
