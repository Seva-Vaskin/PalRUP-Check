#pragma once

#include <cstdint>
#include <optional>
#include <vector>

class FenwickTree {
public:
    explicit FenwickTree(uint32_t size);

    void add(uint32_t position, int32_t delta);

    void clear();

    [[nodiscard]] uint32_t prefix_sum(uint32_t position) const;

    [[nodiscard]] uint32_t first_position_with_prefix_sum(uint32_t sum) const;

private:
    std::vector<int32_t> tree_;
};

class CountingBitset {
public:
    explicit CountingBitset(uint32_t limit);

    void insert(uint32_t x);

    void erase(uint32_t x);

    void assign_interval(uint32_t first, uint32_t last);

    [[nodiscard]] bool contains(uint32_t x) const;

    [[nodiscard]] uint32_t count_up_to(uint32_t x) const;

    [[nodiscard]] uint32_t nth(uint32_t n) const;

private:
    static constexpr uint32_t kWordBits = 64;

    std::vector<uint64_t> words_;
    FenwickTree word_counts_;
};

class RecencyRanking {
public:
    explicit RecencyRanking(uint32_t slot_count);

    void make_most_recent(uint32_t slot);

    void remove(uint32_t slot);

    [[nodiscard]] uint32_t rank_of(uint32_t slot) const;

    [[nodiscard]] uint32_t slot_at_rank(uint32_t rank) const;

    [[nodiscard]] uint32_t least_recent_slot();

    [[nodiscard]] uint32_t size() const noexcept { return size_; }

private:
    static constexpr uint32_t kNoStamp = 0;

    uint32_t stamp_limit_;
    std::vector<uint32_t> stamp_of_slot_;
    std::vector<uint32_t> slot_of_stamp_;
    CountingBitset used_stamps_;
    uint32_t next_stamp_ = 1;
    uint32_t oldest_stamp_ = 1;
    uint32_t size_ = 0;

    void give_new_stamp(uint32_t slot);

    void drop_stamp(uint32_t slot);

    void renumber_stamps();
};

class KeySlotMap {
public:
    explicit KeySlotMap(uint32_t capacity);

    [[nodiscard]] std::optional<uint32_t> find(uint64_t key) const;

    void insert(uint64_t key, uint32_t slot);

    void erase(uint64_t key);

private:
    static constexpr uint32_t kEmpty = UINT32_MAX;
    static constexpr uint64_t kSizePerCapacity = 4;

    struct Entry {
        uint64_t key = 0;
        uint32_t slot = kEmpty;
    };

    std::vector<Entry> entries_;
    uint64_t mask_;
    uint32_t shift_;

    [[nodiscard]] uint64_t home_of(uint64_t key) const noexcept;

    [[nodiscard]] uint64_t next(uint64_t position) const noexcept;

    [[nodiscard]] uint64_t distance(uint64_t from, uint64_t to) const noexcept;

    [[nodiscard]] uint64_t probe(uint64_t key) const;
};

class RecencyCache {
public:
    static constexpr uint8_t MIN_CACHE_BITS = 1;
    static constexpr uint8_t MAX_CACHE_BITS = 24;

    explicit RecencyCache(uint8_t cache_bits);

    [[nodiscard]] bool contains(uint64_t key) const;

    [[nodiscard]] std::optional<uint32_t> rank_of(uint64_t key) const;

    [[nodiscard]] uint64_t key_at_rank(uint32_t rank) const;

    [[nodiscard]] uint32_t make_most_recent(uint64_t key);

    [[nodiscard]] uint64_t make_most_recent_at_rank(uint32_t rank);

    [[nodiscard]] bool make_most_recent_if_cached(uint64_t key);

    void insert(uint64_t key);

    void remove(uint64_t key);

    [[nodiscard]] uint32_t size() const noexcept { return ranking_.size(); }

    [[nodiscard]] uint32_t capacity() const noexcept { return capacity_; }

private:
    uint32_t capacity_;
    RecencyRanking ranking_;
    std::vector<uint64_t> key_of_slot_;
    KeySlotMap slot_of_key_;
    std::vector<uint32_t> free_slots_;

    [[nodiscard]] static uint32_t capacity_for(uint8_t cache_bits);

    void evict_least_recent();
};
