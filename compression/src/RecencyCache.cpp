#include "RecencyCache.hpp"

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

CountingBitset::CountingBitset(const uint32_t limit)
    : words_(limit / kWordBits + 1, 0),
      word_counts_(static_cast<uint32_t>(words_.size())) {
}

void CountingBitset::insert(const uint32_t x) {
    uint64_t &word = words_[x / kWordBits];
    const uint64_t bit = uint64_t{1} << (x % kWordBits);
    CHECK((word & bit) == 0);
    word |= bit;
    word_counts_.add(x / kWordBits + 1, +1);
}

void CountingBitset::erase(const uint32_t x) {
    uint64_t &word = words_[x / kWordBits];
    const uint64_t bit = uint64_t{1} << (x % kWordBits);
    CHECK((word & bit) != 0);
    word &= ~bit;
    word_counts_.add(x / kWordBits + 1, -1);
}

void CountingBitset::assign_interval(const uint32_t first, const uint32_t last) {
    std::ranges::fill(words_, 0);
    for (uint32_t x = first; x <= last; ++x) {
        words_[x / kWordBits] |= uint64_t{1} << (x % kWordBits);
    }
    word_counts_.clear();
    for (uint32_t w = 0; w < words_.size(); ++w) {
        word_counts_.add(w + 1, std::popcount(words_[w]));
    }
}

bool CountingBitset::contains(const uint32_t x) const {
    return ((words_[x / kWordBits] >> (x % kWordBits)) & 1) != 0;
}

uint32_t CountingBitset::count_up_to(const uint32_t x) const {
    const uint32_t w = x / kWordBits;
    const uint64_t members_up_to_x = words_[w] & (~uint64_t{0} >> (kWordBits - 1 - x % kWordBits));
    return word_counts_.prefix_sum(w) + static_cast<uint32_t>(std::popcount(members_up_to_x));
}

uint32_t CountingBitset::nth(const uint32_t n) const {
    CHECK(n >= 1);
    const uint32_t w = word_counts_.first_position_with_prefix_sum(n) - 1;
    CHECK(w < words_.size());
    uint64_t word = words_[w];
    for (uint32_t below = n - word_counts_.prefix_sum(w); below > 1; --below) {
        word &= word - 1;
    }
    return w * kWordBits + static_cast<uint32_t>(std::countr_zero(word));
}

RecencyRanking::RecencyRanking(const uint32_t slot_count)
    : stamp_limit_(2 * std::max<uint32_t>(slot_count, 1)),
      stamp_of_slot_(slot_count, kNoStamp),
      slot_of_stamp_(stamp_limit_ + 1, 0),
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
    return size_ - used_stamps_.count_up_to(stamp);
}

uint32_t RecencyRanking::slot_at_rank(const uint32_t rank) const {
    CHECK(rank < size_);
    return slot_of_stamp_[used_stamps_.nth(size_ - rank)];
}

uint32_t RecencyRanking::least_recent_slot() {
    CHECK(size_ > 0);
    while (!used_stamps_.contains(oldest_stamp_)) {
        ++oldest_stamp_;
    }
    return slot_of_stamp_[oldest_stamp_];
}

void RecencyRanking::give_new_stamp(const uint32_t slot) {
    const uint32_t stamp = next_stamp_++;
    stamp_of_slot_[slot] = stamp;
    slot_of_stamp_[stamp] = slot;
    used_stamps_.insert(stamp);
    ++size_;
}

void RecencyRanking::drop_stamp(const uint32_t slot) {
    const uint32_t stamp = stamp_of_slot_[slot];
    if (stamp == kNoStamp) {
        return;
    }
    used_stamps_.erase(stamp);
    stamp_of_slot_[slot] = kNoStamp;
    --size_;
}

void RecencyRanking::renumber_stamps() {
    uint32_t renumbered = 0;
    for (uint32_t stamp = oldest_stamp_; stamp < next_stamp_; ++stamp) {
        if (used_stamps_.contains(stamp)) {
            const uint32_t slot = slot_of_stamp_[stamp];
            ++renumbered;
            slot_of_stamp_[renumbered] = slot;
            stamp_of_slot_[slot] = renumbered;
        }
    }
    CHECK(renumbered == size_);
    used_stamps_.assign_interval(1, size_);
    next_stamp_ = size_ + 1;
    oldest_stamp_ = 1;
}

KeySlotMap::KeySlotMap(const uint32_t capacity)
    : entries_(std::bit_ceil(kSizePerCapacity * std::max<uint32_t>(capacity, 1))),
      mask_(entries_.size() - 1),
      shift_(64 - static_cast<uint32_t>(std::countr_zero(entries_.size()))) {
}

uint64_t KeySlotMap::home_of(const uint64_t key) const noexcept {
    return (key * 0x9E3779B97F4A7C15ull) >> shift_;
}

uint64_t KeySlotMap::next(uint64_t position) const noexcept { return (position + 1) & mask_; }

uint64_t KeySlotMap::distance(uint64_t from, uint64_t to) const noexcept { return (to - from) & mask_; }

uint64_t KeySlotMap::probe(const uint64_t key) const {
    uint64_t position = home_of(key);
    while (entries_[position].slot != kEmpty && entries_[position].key != key) {
        position = next(position);
    }
    return position;
}

std::optional<uint32_t> KeySlotMap::find(const uint64_t key) const {
    const Entry &entry = entries_[probe(key)];
    if (entry.slot == kEmpty) {
        return std::nullopt;
    }
    return entry.slot;
}

void KeySlotMap::insert(const uint64_t key, const uint32_t slot) {
    const uint64_t position = probe(key);
    CHECK(entries_[position].slot == kEmpty);
    entries_[position] = Entry{.key = key, .slot = slot};
}

void KeySlotMap::erase(const uint64_t key) {
    uint64_t hole = probe(key);
    CHECK(entries_[hole].slot != kEmpty);
    for (uint64_t position = next(hole); entries_[position].slot != kEmpty; position = next(position)) {
        const uint64_t home = home_of(entries_[position].key);
        if (distance(home, position) >= distance(hole, position)) {
            entries_[hole] = entries_[position];
            hole = position;
        }
    }
    entries_[hole].slot = kEmpty;
}

uint32_t RecencyCache::capacity_for(const uint8_t cache_bits) {
    CHECK(MIN_CACHE_BITS <= cache_bits && cache_bits <= MAX_CACHE_BITS);
    return uint32_t{1} << cache_bits;
}

RecencyCache::RecencyCache(const uint8_t cache_bits)
    : capacity_(capacity_for(cache_bits)),
      ranking_(capacity_),
      key_of_slot_(capacity_, 0),
      slot_of_key_(capacity_) {
    free_slots_.reserve(capacity_);
    for (uint32_t slot = capacity_; slot-- > 0;) {
        free_slots_.push_back(slot);
    }
}

bool RecencyCache::contains(const uint64_t key) const {
    return slot_of_key_.find(key).has_value();
}

std::optional<uint32_t> RecencyCache::rank_of(const uint64_t key) const {
    const auto slot = slot_of_key_.find(key);
    if (!slot) {
        return std::nullopt;
    }
    return ranking_.rank_of(*slot);
}

uint32_t RecencyCache::make_most_recent(const uint64_t key) {
    const auto slot = slot_of_key_.find(key);
    CHECK(slot.has_value());
    const uint32_t rank = ranking_.rank_of(*slot);
    ranking_.make_most_recent(*slot);
    return rank;
}

uint64_t RecencyCache::make_most_recent_at_rank(const uint32_t rank) {
    const uint32_t slot = ranking_.slot_at_rank(rank);
    ranking_.make_most_recent(slot);
    return key_of_slot_[slot];
}

bool RecencyCache::make_most_recent_if_cached(const uint64_t key) {
    const auto slot = slot_of_key_.find(key);
    if (!slot) {
        return false;
    }
    ranking_.make_most_recent(*slot);
    return true;
}

void RecencyCache::insert(const uint64_t key) {
    if (contains(key)) {
        return;
    }
    if (free_slots_.empty()) {
        evict_least_recent();
    }
    const uint32_t slot = free_slots_.back();
    free_slots_.pop_back();
    key_of_slot_[slot] = key;
    slot_of_key_.insert(key, slot);
    ranking_.make_most_recent(slot);
}

void RecencyCache::remove(const uint64_t key) {
    const auto slot = slot_of_key_.find(key);
    if (!slot) {
        return;
    }
    ranking_.remove(*slot);
    free_slots_.push_back(*slot);
    slot_of_key_.erase(key);
}

void RecencyCache::evict_least_recent() {
    const uint32_t slot = ranking_.least_recent_slot();
    slot_of_key_.erase(key_of_slot_[slot]);
    ranking_.remove(slot);
    free_slots_.push_back(slot);
}
