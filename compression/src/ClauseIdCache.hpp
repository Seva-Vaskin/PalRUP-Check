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
    static constexpr uint32_t kNoSlot = UINT32_MAX;

    uint32_t stamp_limit_;
    std::vector<uint32_t> stamp_of_slot_;
    std::vector<uint32_t> slot_of_stamp_;
    FenwickTree used_stamps_;
    uint32_t next_stamp_ = 1;
    uint32_t oldest_stamp_ = 1;
    uint32_t size_ = 0;

    void give_new_stamp(uint32_t slot);

    void drop_stamp(uint32_t slot);

    void renumber_stamps();
};

class IdSlotMap {
public:
    explicit IdSlotMap(uint32_t capacity);

    [[nodiscard]] std::optional<uint32_t> find(uint64_t id) const;

    void insert(uint64_t id, uint32_t slot);

    void erase(uint64_t id);

private:
    static constexpr uint32_t kEmpty = UINT32_MAX;
    static constexpr uint64_t kSizePerCapacity = 4;

    struct Entry {
        uint64_t id = 0;
        uint32_t slot = kEmpty;
    };

    std::vector<Entry> entries_;
    uint64_t mask_;
    uint32_t shift_;

    [[nodiscard]] uint64_t home_of(uint64_t id) const noexcept;

    [[nodiscard]] uint64_t next(uint64_t position) const noexcept;

    [[nodiscard]] uint64_t distance(uint64_t from, uint64_t to) const noexcept;

    [[nodiscard]] uint64_t probe(uint64_t id) const;
};

class ClauseIdCache {
public:
    static constexpr uint8_t MIN_CACHE_BITS = 1;
    static constexpr uint8_t MAX_CACHE_BITS = 24;

    ClauseIdCache(uint8_t cache_bits, uint64_t clause_count);

    [[nodiscard]] bool contains(uint64_t id) const;

    [[nodiscard]] std::optional<uint32_t> rank_of(uint64_t id) const;

    [[nodiscard]] uint64_t id_at_rank(uint32_t rank) const;

    void make_most_recent(uint64_t id);

    [[nodiscard]] bool make_most_recent_if_cached(uint64_t id);

    void insert(uint64_t id);

    void remove(uint64_t id);

    [[nodiscard]] uint32_t size() const noexcept { return ranking_.size(); }

private:
    uint32_t capacity_;
    RecencyRanking ranking_;
    std::vector<uint64_t> id_of_slot_;
    IdSlotMap slot_of_id_;
    std::vector<uint32_t> free_slots_;

    [[nodiscard]] static uint32_t capacity_for(uint8_t cache_bits);

    void prefill_with_formula_ids(uint64_t clause_count);

    void evict_least_recent();
};
