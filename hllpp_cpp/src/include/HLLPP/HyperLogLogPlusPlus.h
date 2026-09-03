/********************************************
 * Copyright (c) 2026 Shun/修海 (@shun4midx) *
 * Project: HyperLogLogPlusPlus-Autocorrect *
 * File Type: C++ Header file               *
 * File: HyperLogLogPlusPlus.h              *
 ****************************************** */

// ======== INCLUDE ======== //
#pragma once
#include "Hasher.h"
#include <bit>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <optional>
#include <unordered_map>

// ======== STRUCTS AND CLASSES ======== //
struct SketchConfig {
    int b = 10;
    double alpha_override = -1.0;
    bool sparse = true;
    int sparse_precision = 25;
    std::optional<std::size_t> sparse_threshold = std::nullopt;
};

class HyperLogLogPlusPlus {
public:
    explicit HyperLogLogPlusPlus();
    explicit HyperLogLogPlusPlus(const SketchConfig& _cfg);
    void insert(uint64_t hash);
    void insert(const std::string& str);
    void merge(const HyperLogLogPlusPlus& other);
    double estimate() const;
    double raw_estimate() const;
    double union_estimate(const HyperLogLogPlusPlus& other) const;
    HyperLogLogPlusPlus copy() const;
    std::size_t zero_count() const;
    void reset();

private:
    // ~~~~~~~~ STRUCTS ~~~~~~~~ //
    struct IndexRank {
        uint64_t index;
        uint8_t rank;
    };

    struct RegisterStats {
        std::size_t nonzero_count;
        double nonzero_sum;
    };

    // ~~~~~~~~ VARIABLES ~~~~~~~~ //
    SketchConfig cfg;
    std::size_t m;
    uint64_t sparse_m;
    double alpha_m;
    bool sparse_mode;
    std::unordered_map<uint64_t, uint8_t> sparse_entries;
    std::vector<uint8_t> registers;
    std::size_t sparse_threshold;
    mutable std::optional<RegisterStats> stats_cache;

    // ~~~~~~~~ FUNCTIONS ~~~~~~~~ //
    double compute_alpha() const;
    void invalidate_stats();
    
    static uint8_t rank(uint64_t suffix, int suffix_bits);
    IndexRank dense_index_and_rank(uint64_t hash) const;
    IndexRank sparse_index_and_rank(uint64_t hash) const;
    bool is_sparse() const;
    IndexRank sparse_to_dense_entry(uint64_t sparse_idx, uint8_t sparse_rank) const;
    void promote_to_dense();
    static double linear_counting(uint64_t bucket_count, uint64_t zero_count);
    double sparse_estimate() const;
    RegisterStats register_stats() const;
    double raw_from_harmonic_sum(double harmonic_sum) const;
    double correct_raw_estimate(double raw_estimate, std::size_t zero_count) const;
    std::vector<uint8_t> dense_registers_from_sparse() const;
    std::vector<uint8_t> registers_for_union() const;
    
    void require_compatible(const HyperLogLogPlusPlus& other) const;
};

// ======== BACKWARDS COMPATIBILITY ======== //
using HyperLogLog = HyperLogLogPlusPlus;