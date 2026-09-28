#pragma once
#include "Fingerprint.h"
#include <chrono>
#include <future>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mmvr::states {
// Main-thread-owned cache. The worker reads only immutable path/stamp copies;
// it never touches game state, archives, rendering, or resource-manager locks.
class AsyncFingerprintCache {
    struct Stamp {
        std::filesystem::path path;
        uintmax_t size;
        std::filesystem::file_time_type modified;
        bool operator==(const Stamp&) const = default;
    };
    struct Entry { Stamp stamp; std::string digest; };
    std::unordered_map<std::string, Entry> entries;
    std::atomic_bool cancellation{false};
    std::future<std::vector<Entry>> pending;
    static Stamp Inspect(const std::filesystem::path& path) {
        return {path, std::filesystem::file_size(path), std::filesystem::last_write_time(path)};
    }
public:
    ~AsyncFingerprintCache() { cancellation.store(true, std::memory_order_relaxed); }
    std::optional<std::vector<std::string>> Get(const std::vector<std::filesystem::path>& files,
                                               bool wait = false) {
        if (pending.valid()) {
            if (!wait && pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return std::nullopt;
            for (auto& entry : pending.get()) {
                const auto key = entry.stamp.path.generic_string();
                entries.insert_or_assign(key, std::move(entry));
            }
        }
        std::vector<Stamp> missing;
        std::vector<std::string> result;
        for (const auto& path : files) {
            auto stamp = Inspect(path);
            auto found = entries.find(path.generic_string());
            if (found == entries.end() || !(found->second.stamp == stamp)) missing.push_back(std::move(stamp));
            else result.push_back(found->second.digest);
        }
        if (missing.empty()) return result;
        pending = std::async(std::launch::async, [missing = std::move(missing), stop = &cancellation] {
            std::vector<Entry> ready;
            for (const auto& stamp : missing) {
                if (!(Inspect(stamp.path) == stamp)) throw Error("State dependency changed before fingerprinting");
                auto digest = FingerprintFile(stamp.path, stop);
                if (!(Inspect(stamp.path) == stamp)) throw Error("State dependency changed while fingerprinting");
                ready.push_back({stamp, std::move(digest)});
            }
            return ready;
        });
        if (wait) return Get(files, true);
        return std::nullopt;
    }
};
}
