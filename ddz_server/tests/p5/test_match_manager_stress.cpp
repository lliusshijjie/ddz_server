#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "service/match/match_manager.h"

int main() {
    ddz::MatchManager match_manager;

    constexpr int kThreads = 8;
    constexpr int kPlayersPerThread = 200;  // total 1600, divisible by 4
    constexpr int kMode = 4;

    std::atomic<int> join_success{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    const auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([t, &match_manager, &join_success, kPlayersPerThread, kMode]() {
            const int64_t base = 100000 + t * 10000;
            for (int i = 0; i < kPlayersPerThread; ++i) {
                const int64_t player_id = base + i;
                const bool ok = match_manager.Join(ddz::MatchPlayer{
                    player_id, 0, 1000, kMode
                });
                if (ok) {
                    join_success.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : workers) {
        th.join();
    }
    const auto join_end = std::chrono::steady_clock::now();

    int popped_players = 0;
    while (true) {
        auto group = match_manager.TryPopMatchedPlayers(kMode);
        if (!group.has_value()) {
            break;
        }
        popped_players += static_cast<int>(group->size());
    }
    const auto end = std::chrono::steady_clock::now();

    const int total = kThreads * kPlayersPerThread;
    assert(join_success.load() == total);
    assert(popped_players == total);

    const auto join_ms = std::chrono::duration_cast<std::chrono::milliseconds>(join_end - start).count();
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "test_p5_match_manager_stress passed"
              << " total_players=" << total
              << " join_ms=" << join_ms
              << " total_ms=" << total_ms
              << std::endl;
    return 0;
}
