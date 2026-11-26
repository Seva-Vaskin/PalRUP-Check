#include "ClauseIdCache.hpp"

#include <algorithm>
#include <bit>

#include "error.hpp"

namespace {
    [[nodiscard]] uint32_t lowest_set_bit(const uint32_t x) noexcept {
        return x & (0u - x);
    }
}

FenwickTree::FenwickTree(const uint32_t size) : tree_(size + 1, 0) {
}

void FenwickTree::add(uint32_t position, const int32_t delta) {
    const auto size = static_cast<uint32_t>(tree_.size() - 1);
    for (; position <= size; position += lowest_set_bit(position)) {
        tree_[position] += delta;
    }
}

void FenwickTree::clear() {
    std::ranges::fill(tree_, 0);
}

uint32_t FenwickTree::prefix_sum(uint32_t position) const {
    int32_t sum = 0;
    for (; position > 0; position -= lowest_set_bit(position)) {
        sum += tree_[position];
    }
    return static_cast<uint32_t>(sum);
}

uint32_t FenwickTree::first_position_with_prefix_sum(const uint32_t sum) const {
    const auto size = static_cast<uint32_t>(tree_.size() - 1);
    uint32_t position = 0;
    uint32_t remaining = sum;
    for (uint32_t step = std::bit_floor(size); step > 0; step >>= 1) {
        const uint32_t next = position + step;
        if (next <= size && static_cast<uint32_t>(tree_[next]) < remaining) {
            position = next;
            remaining -= static_cast<uint32_t>(tree_[next]);
        }
    }
    return position + 1;
}

RecencyRanking::RecencyRanking(const uint32_t slot_count)
    : stamp_limit_(2 * std::max<uint32_t>(slot_count, 1)),
      stamp_of_slot_(slot_count, kNoStamp),
      slot_of_stamp_(stamp_limit_ + 1, kNoSlot),
      used_stamps_(stamp_limit_) {
}

void RecencyRanking::make_most_recent(const uint32_t slot) {
    drop_stamp(slot);
    if (next_stamp_ > stamp_limit_) {
        renumber_stamps();
    }
    give_new_stamp(slot);
}

void RecencyRanking::remove(const uint32_t slot) {
    drop_stamp(slot);
}

uint32_t RecencyRanking::rank_of(const uint32_t slot) const {
    const uint32_t stamp = stamp_of_slot_[slot];
    CHECK(stamp != kNoStamp);
    return size_ - used_stamps_.prefix_sum(stamp);
}

uint32_t RecencyRanking::slot_at_rank(const uint32_t rank) const {
    CHECK(rank < size_);
    return slot_of_stamp_[used_stamps_.first_position_with_prefix_sum(size_ - rank)];
}

uint32_t RecencyRanking::least_recent_slot() {
    CHECK(size_ > 0);
    while (slot_of_stamp_[oldest_stamp_] == kNoSlot) {
        ++oldest_stamp_;
    }
    return slot_of_stamp_[oldest_stamp_];
}

void RecencyRanking::give_new_stamp(const uint32_t slot) {
    const uint32_t stamp = next_stamp_++;
    stamp_of_slot_[slot] = stamp;
    slot_of_stamp_[stamp] = slot;
    used_stamps_.add(stamp, +1);
    ++size_;
}

void RecencyRanking::drop_stamp(const uint32_t slot) {
    const uint32_t stamp = stamp_of_slot_[slot];
    if (stamp == kNoStamp) {
        return;
    }
    used_stamps_.add(stamp, -1);
    slot_of_stamp_[stamp] = kNoSlot;
    stamp_of_slot_[slot] = kNoStamp;
    --size_;
}

void RecencyRanking::renumber_stamps() {
    std::vector<uint32_t> slots_oldest_first;
    slots_oldest_first.reserve(size_);
    for (uint32_t stamp = 1; stamp <= stamp_limit_; ++stamp) {
        if (slot_of_stamp_[stamp] != kNoSlot) {
            slots_oldest_first.push_back(slot_of_stamp_[stamp]);
        }
    }
    std::ranges::fill(slot_of_stamp_, kNoSlot);
    used_stamps_.clear();
    next_stamp_ = 1;
    oldest_stamp_ = 1;
    size_ = 0;
    for (const uint32_t slot: slots_oldest_first) {
        give_new_stamp(slot);
    }
}

IdSlotMap::IdSlotMap(const uint32_t capacity)
    : entries_(std::bit_ceil(kSizePerCapacity * std::max<uint32_t>(capacity, 1))),
      mask_(entries_.size() - 1),
      shift_(64 - static_cast<uint32_t>(std::countr_zero(entries_.size()))) {
}

uint64_t IdSlotMap::home_of(const uint64_t id) const noexcept {
    return (id * 0x9E3779B97F4A7C15ull) >> shift_;
}

uint64_t IdSlotMap::next(uint64_t position) const noexcept { return (position + 1) & mask_; }

uint64_t IdSlotMap::distance(uint64_t from, uint64_t to) const noexcept { return (to - from) & mask_; }

uint64_t IdSlotMap::probe(const uint64_t id) const {
    uint64_t position = home_of(id);
    while (entries_[position].slot != kEmpty && entries_[position].id != id) {
        position = next(position);
    }
    return position;
}

std::optional<uint32_t> IdSlotMap::find(const uint64_t id) const {
    const Entry &entry = entries_[probe(id)];
    if (entry.slot == kEmpty) {
        return std::nullopt;
    }
    return entry.slot;
}

void IdSlotMap::insert(const uint64_t id, const uint32_t slot) {
    const uint64_t position = probe(id);
    CHECK(entries_[position].slot == kEmpty);
    entries_[position] = Entry{.id = id, .slot = slot};
}

void IdSlotMap::erase(const uint64_t id) {
    uint64_t hole = probe(id);
    CHECK(entries_[hole].slot != kEmpty);
    for (uint64_t position = next(hole); entries_[position].slot != kEmpty; position = next(position)) {
        const uint64_t home = home_of(entries_[position].id);
        if (distance(home, position) >= distance(hole, position)) {
            entries_[hole] = entries_[position];
            hole = position;
        }
    }
    entries_[hole].slot = kEmpty;
}

uint32_t ClauseIdCache::capacity_for(const uint8_t cache_bits) {
    CHECK(MIN_CACHE_BITS <= cache_bits && cache_bits <= MAX_CACHE_BITS);
    return uint32_t{1} << cache_bits;
}

ClauseIdCache::ClauseIdCache(const uint8_t cache_bits, const uint64_t clause_count)
    : capacity_(capacity_for(cache_bits)),
      ranking_(capacity_),
      id_of_slot_(capacity_, 0),
      slot_of_id_(capacity_) {
    free_slots_.reserve(capacity_);
    for (uint32_t slot = capacity_; slot-- > 0;) {
        free_slots_.push_back(slot);
    }
    prefill_with_formula_ids(clause_count);
}

void ClauseIdCache::prefill_with_formula_ids(const uint64_t clause_count) {
    const uint64_t count = std::min<uint64_t>(clause_count, capacity_);
    const uint64_t first = clause_count - count + 1;
    for (uint64_t k = 0; k < count; ++k) {
        insert(first + k);
    }
}

bool ClauseIdCache::contains(const uint64_t id) const {
    return slot_of_id_.find(id).has_value();
}

std::optional<uint32_t> ClauseIdCache::rank_of(const uint64_t id) const {
    const auto slot = slot_of_id_.find(id);
    if (!slot) {
        return std::nullopt;
    }
    return ranking_.rank_of(*slot);
}

uint64_t ClauseIdCache::id_at_rank(const uint32_t rank) const {
    return id_of_slot_[ranking_.slot_at_rank(rank)];
}

void ClauseIdCache::make_most_recent(const uint64_t id) {
    const auto slot = slot_of_id_.find(id);
    CHECK(slot.has_value());
    ranking_.make_most_recent(*slot);
}

bool ClauseIdCache::make_most_recent_if_cached(const uint64_t id) {
    const auto slot = slot_of_id_.find(id);
    if (!slot) {
        return false;
    }
    ranking_.make_most_recent(*slot);
    return true;
}

void ClauseIdCache::insert(const uint64_t id) {
    if (contains(id)) {
        return;
    }
    if (free_slots_.empty()) {
        evict_least_recent();
    }
    const uint32_t slot = free_slots_.back();
    free_slots_.pop_back();
    id_of_slot_[slot] = id;
    slot_of_id_.insert(id, slot);
    ranking_.make_most_recent(slot);
}

void ClauseIdCache::remove(const uint64_t id) {
    const auto slot = slot_of_id_.find(id);
    if (!slot) {
        return;
    }
    ranking_.remove(*slot);
    free_slots_.push_back(*slot);
    slot_of_id_.erase(id);
}

void ClauseIdCache::evict_least_recent() {
    const uint32_t slot = ranking_.least_recent_slot();
    slot_of_id_.erase(id_of_slot_[slot]);
    ranking_.remove(slot);
    free_slots_.push_back(slot);
}
