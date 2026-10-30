/* Exact-thread return correlation. No guest reads, address-space fallbacks or
 * inferred parentage. The adapter owns epoch changes and validated birth facts.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace syscalls2 {

struct ThreadKey {
    uint64_t epoch;
    uint64_t task;
    uint64_t lifetime;
    uint32_t lifetime_format;

    bool operator==(const ThreadKey &other) const {
        return epoch == other.epoch && task == other.task &&
               lifetime == other.lifetime && lifetime_format == other.lifetime_format;
    }
};

/* Capacity bounds invocations, each of which has at most two branches. Payload
 * is an owning value (not a pointer into the tracker's storage). In particular,
 * a Delivery remains valid after the tracker is reset or its entry is erased.
 * Methods are single-threaded; the adapter must serialize multiple vCPUs.
 */
template <typename Payload>
class ThreadReturnTracker {
public:
    struct Delivery {
        uint64_t invocation;
        ThreadKey origin;
        ThreadKey returning_thread;
        bool child;
        Payload payload;
    };

    explicit ThreadReturnTracker(size_t capacity, uint64_t epoch = 0)
        : capacity_(capacity), epoch_(epoch) {}

    /* Returns no ID on invalid identity, capacity exhaustion or sequence
     * exhaustion. Never evicts another invocation to make a guess fit. */
    std::optional<uint64_t> begin(ThreadKey origin, uint64_t retpc,
                                  Payload payload, bool may_spawn = false) {
        if (!valid(origin) || entries_.size() >= capacity_ ||
            next_ == std::numeric_limits<uint64_t>::max()) {
            return std::nullopt;
        }
        uint64_t sequence = ++next_;
        entries_.push_back({sequence, origin, retpc, std::move(payload),
                            may_spawn, true, std::nullopt, false});
        return sequence;
    }

    /* The caller must have observed this edge in kernel execution for this
     * invocation, before either return. CLONE_PARENT is irrelevant: origin is
     * the executing creator, not task->real_parent. A second child is refused.
     * No child branch exists until this succeeds. */
    bool birth(uint64_t invocation, ThreadKey origin, ThreadKey child) {
        if (!valid(origin) || !valid(child) || origin == child) return false;
        for (auto &entry : entries_) {
            if (entry.sequence == invocation && entry.origin == origin &&
                entry.parent_pending && entry.may_spawn && !entry.child) {
                entry.child = child;
                entry.child_pending = true;
                return true;
            }
        }
        return false;
    }

    /* Explicitly close a failed spawn, without suppressing its parent return.
     * A contradictory failure after an observed birth is refused. */
    bool failed_spawn(uint64_t invocation, ThreadKey origin) {
        for (auto &entry : entries_) {
            if (entry.sequence == invocation && entry.origin == origin &&
                entry.parent_pending && entry.may_spawn && !entry.child) {
                entry.may_spawn = false;
                return true;
            }
        }
        return false;
    }

    /* Exact thread and PC are necessary, not sufficient: two matching pending
     * invocations are ambiguous. Leave both untouched rather than picking one
     * by insertion order, stack guesses, or the return value. */
    std::optional<Delivery> take(ThreadKey current, uint64_t retpc) {
        if (!valid(current)) return std::nullopt;
        size_t found = entries_.size();
        bool child = false;
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto &entry = entries_[i];
            if (entry.retpc != retpc) continue;
            bool parent_match = entry.parent_pending && entry.origin == current;
            bool child_match = entry.child_pending && *entry.child == current;
            if (!parent_match && !child_match) continue;
            if (found != entries_.size()) return std::nullopt;
            found = i;
            child = child_match;
        }
        if (found == entries_.size()) return std::nullopt;
        auto &entry = entries_[found];
        Delivery result{entry.sequence, entry.origin, current, child, entry.payload};
        if (child) entry.child_pending = false;
        else entry.parent_pending = false;
        collect();
        return result;
    }

    bool drop(uint64_t invocation) {
        for (auto i = entries_.begin(); i != entries_.end(); ++i) {
            if (i->sequence == invocation) {
                entries_.erase(i);
                return true;
            }
        }
        return false;
    }

    /* Death/exec of one thread drops only its branches; a known child or
     * parent remains independently deliverable. Call before task reuse. */
    void death(ThreadKey task) { drop_thread(task); }
    void exec(ThreadKey task) { drop_thread(task); }

    /* Caller must supply a fresh epoch on snapshot/replay lifetime transitions.
     * Sequence IDs are deliberately not reused, even after reset. */
    void reset(uint64_t epoch) {
        entries_.clear();
        epoch_ = epoch;
    }

    size_t size() const { return entries_.size(); }

private:
    struct Entry {
        uint64_t sequence;
        ThreadKey origin;
        uint64_t retpc;
        Payload payload;
        bool may_spawn;
        bool parent_pending;
        std::optional<ThreadKey> child;
        bool child_pending;
    };

    bool valid(ThreadKey key) const {
        return key.epoch == epoch_ && key.task != 0 && key.lifetime_format != 0;
    }

    void drop_thread(ThreadKey task) {
        for (auto &entry : entries_) {
            if (entry.origin == task) entry.parent_pending = false;
            if (entry.child && *entry.child == task) entry.child_pending = false;
        }
        collect();
    }

    void collect() {
        for (auto i = entries_.begin(); i != entries_.end();) {
            if (!i->parent_pending && !i->child_pending) i = entries_.erase(i);
            else ++i;
        }
    }

    size_t capacity_;
    uint64_t epoch_;
    uint64_t next_ = 0;
    std::vector<Entry> entries_;
};

} // namespace syscalls2
