#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmvr::states {
using Bytes = std::vector<uint8_t>;
// Build and ABI identity are deliberately strict. A state is never a portable
// raw-memory image, and another build must not interpret an old type layout.
struct Identity {
    std::string build, assets, abi;
    bool operator==(const Identity&) const = default;
};
enum class ReferenceKind : uint8_t { Owned = 0, Asset = 1, Function = 2 };
struct Reference {
    uint64_t at = 0;
    ReferenceKind kind = ReferenceKind::Owned;
    std::string target;
    uint64_t offset = 0;
};
struct Block {
    std::string id;
    uint32_t schema = 0;
    Bytes bytes;
    std::vector<Reference> references;
};
struct Snapshot {
    Identity identity;
    uint64_t tick = 0;
    std::vector<Block> blocks;
};
class Error : public std::runtime_error { public: using std::runtime_error::runtime_error; };
inline constexpr uint64_t MaxArchiveBytes = 256ull * 1024 * 1024;
inline constexpr uint32_t MaxBlocks = 8192, MaxReferences = 1048576;
// Only explicitly described pointer fields are removed. Never scan scalar data
// looking for values that happen to resemble an address.
void SetReference(Block&, Reference);
void Validate(const Snapshot&);
Bytes Encode(const Snapshot&);
// Migration inspection only: verifies format, checksum, graph and pointer width.
// Caller must validate compatibility before interpreting or restoring any block.
Snapshot DecodeUnbound(std::span<const uint8_t>);
Snapshot Decode(std::span<const uint8_t>, const Identity& expected);
class Store {
    std::filesystem::path directory;
    std::filesystem::path Slot(int) const;
public:
    explicit Store(std::filesystem::path gameDirectory);
    void Save(int slot, const Snapshot&);
    Snapshot Load(int slot, const Identity&) const;
    Snapshot LoadUnbound(int slot) const; // Read-only migration inspection; never rewrites the slot.
    bool Exists(int slot) const;
};
struct ExternalRange { void* address = nullptr; uint64_t bytes = 0; };
using Resolver = std::function<ExternalRange(ReferenceKind, const std::string&)>;
// Owns a disconnected candidate graph. Failure destroys only the candidate;
// live gameplay is not touched. Commit belongs to the complete game backend.
class Graph {
    std::map<std::string, std::unique_ptr<uint8_t[]>> blocks;
    std::map<std::string, uint64_t> sizes;
public:
    explicit Graph(const Snapshot&, const Resolver&);
    void* Address(const std::string&) const;
    uint64_t Size(const std::string&) const;
};
// A component must prepare all changes before returning. Applying a prepared
// component is a non-failing swap/copy at the paused native frame boundary.
struct PreparedComponent { virtual ~PreparedComponent() = default; virtual void Commit() noexcept = 0; };
struct Component {
    std::string id;
    uint32_t schema = 0;
    std::function<Block()> capture;
    std::function<std::unique_ptr<PreparedComponent>(const Block&)> prepare;
};
class Transaction {
    std::vector<std::unique_ptr<PreparedComponent>> prepared;
    bool committed = false;
public:
    Transaction(const Snapshot&, const std::vector<Component>&, const std::vector<std::string>& required);
    void Commit() noexcept;
};
} // namespace mmvr::states
