/********************************************
 * Copyright (c) 2026 Shun/修海 (@shun4midx) *
 * Project: HyperLogLogPlusPlus-Autocorrect *
 * File Type: C++ file                      *
 * File: HyperLogLogPlusPlus.cpp            *
 ****************************************** */

// ======== INCLUDE ======== //
#include "../include/HLLPP/HyperLogLogPlusPlus.h"
#include <array>
#include <limits>
#include <utility>

// ======== FALLBACK ======== //
uint8_t clz64(uint64_t num) {
    if (num == 0) {
        return 64;
    }

#if __cplusplus >= 202002L && defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
// C++20 and <bit> header with bit operations are available
    return (uint8_t)(std::countl_zero(num));
#elif defined(__GNUC__) || defined(__clang__)
// Fallback to GCC/Clang intrinsic
    return (uint8_t)(__builtin_clzll(num));
#else
// Basic fallback (less efficient)
    uint8_t count = 0;
    uint64_t mask = 1ULL << 63;
    while (mask > 0 && (num & mask) == 0) {
        ++count;
        mask >>= 1;
    }
    return count;
#endif
}

// ======== CONSTANTS ======== //
static const std::array<double, 256> POW2_NEG = []() {
    std::array<double, 256> arr{};
    for (int i = 0; i < 256; ++i) {
        arr[i] = std::pow(2.0, -i);
    }
    return arr;
}();

// ======== HyperLogLogPlusPlus CLASS IMPLEMENTATION ======== //
HyperLogLogPlusPlus::HyperLogLogPlusPlus() : HyperLogLogPlusPlus(SketchConfig()) {}

HyperLogLogPlusPlus::HyperLogLogPlusPlus(const SketchConfig& _cfg) {
    if (_cfg.b < 4 || _cfg.b > 20) {
        throw std::invalid_argument("Precision b must be between 4 and 20.");
    }

    if (_cfg.sparse_precision < _cfg.b || _cfg.sparse_precision > 63) {
        throw std::invalid_argument("sparse_precision must satisfy b <= sparse_precision <= 63.");
    }

    if (_cfg.sparse_threshold.has_value() && _cfg.sparse_threshold.value() == 0) {
        throw std::invalid_argument("sparse_threshold must be positive.");
    }

    cfg = _cfg;
    m = 1ULL << cfg.b;
    sparse_m = 1ULL << cfg.sparse_precision;
    alpha_m = compute_alpha();
    sparse_mode = cfg.sparse;

    if (sparse_mode) {
        // sparse_index -> maximum rank after the p'-bit prefix
        sparse_entries.clear();
        registers.clear();
    } else {
        sparse_entries.clear();
        registers.assign(m, 0);
    }

    stats_cache.reset();

    if (cfg.sparse_threshold.has_value()) {
        sparse_threshold = cfg.sparse_threshold.value();
    } else {
        sparse_threshold = std::max<std::size_t>(16, (6 * m) / 32);
    }
}

// ======== PRIVATE ======= //
void HyperLogLogPlusPlus::invalidate_stats() {
    stats_cache.reset();
}

double HyperLogLogPlusPlus::compute_alpha() const {
    // Override
    if (cfg.alpha_override > 0) {
        return cfg.alpha_override;
    }

    // Otherwise
    if (m == 16) {
        return 0.673;
    } else if (m == 32) {
        return 0.697;
    } else if (m == 64) {
        return 0.709;
    } else {
        return 0.7213 / (1.0 + 1.079 / m);
    }
}

uint8_t HyperLogLogPlusPlus::rank(uint64_t suffix, int suffix_bits) {
    if (suffix == 0) {
        return suffix_bits + 1;
    }
    int bit_length = 64 - clz64(suffix);
    return suffix_bits - bit_length + 1;
}

HyperLogLogPlusPlus::IndexRank HyperLogLogPlusPlus::dense_index_and_rank(uint64_t hash) const {
    int suffix_bits = 64 - cfg.b;
    uint64_t idx = hash >> suffix_bits;
    uint64_t suffix = hash & ((1ULL << suffix_bits) - 1);
    return {idx, rank(suffix, suffix_bits)};
}

HyperLogLogPlusPlus::IndexRank HyperLogLogPlusPlus::sparse_index_and_rank(uint64_t hash) const {
    int suffix_bits = 64 - cfg.sparse_precision;
    uint64_t idx = hash >> suffix_bits;
    uint64_t suffix = hash & ((1ULL << suffix_bits) - 1);
    return {idx, rank(suffix, suffix_bits)};
}

bool HyperLogLogPlusPlus::is_sparse() const {
    return sparse_mode;
}

HyperLogLogPlusPlus::IndexRank HyperLogLogPlusPlus::sparse_to_dense_entry(uint64_t sparse_idx, uint8_t sparse_rank) const {
    int extra_count = cfg.sparse_precision - cfg.b;
    uint64_t dense_idx = sparse_idx >> extra_count;
    if (extra_count == 0) {
        return {dense_idx, sparse_rank};
    }

    uint64_t extra_mask = (1ULL << extra_count) - 1;
    uint64_t extra_bits = sparse_idx & extra_mask;
    uint8_t dense_rank;
    if (extra_bits == 0) {
        dense_rank = extra_count + sparse_rank;
    } else {
        int bit_length = 64 - clz64(extra_bits);
        dense_rank = extra_count - bit_length + 1;
    }

    return {dense_idx, dense_rank};
}

void HyperLogLogPlusPlus::promote_to_dense() {
    if (!is_sparse()) {
        return;
    }

    std::vector<uint8_t> dense(m, 0);
    for (auto& [sparse_idx, sparse_rank] : sparse_entries) {
        IndexRank entry = sparse_to_dense_entry(sparse_idx, sparse_rank);
        if (entry.rank > dense[entry.index]) {
            dense[entry.index] = entry.rank;
        }
    }

    registers = std::move(dense);
    sparse_entries.clear();
    sparse_mode = false;
    invalidate_stats();
}

double HyperLogLogPlusPlus::linear_counting(uint64_t bucket_count, uint64_t zero_count) {
    if (zero_count == 0) {
        return std::numeric_limits<double>::infinity();
    }
    return (double)(bucket_count) * std::log((double)(bucket_count) / (double)(zero_count));
}

double HyperLogLogPlusPlus::sparse_estimate() const {
    std::size_t occupied = sparse_entries.size();
    if (occupied == 0) {
        return 0.0;
    }
    return linear_counting(sparse_m, sparse_m - occupied);
}

HyperLogLogPlusPlus::RegisterStats HyperLogLogPlusPlus::register_stats() const {
    if (is_sparse()) {
        throw std::runtime_error("Dense register statistics are unavailable in sparse mode");
    }

    if (stats_cache.has_value()) {
        return stats_cache.value();
    }

    std::size_t nonzero_count = 0;
    double nonzero_sum = 0.0;

    for (uint8_t reg : registers) {
        if (reg == 0) {
            continue;
        }
        ++nonzero_count;
        nonzero_sum += POW2_NEG[reg];
    }

    RegisterStats result = {nonzero_count, nonzero_sum};
    stats_cache = result;
    return result;
}

double HyperLogLogPlusPlus::raw_from_harmonic_sum(double harmonic_sum) const {
    if (harmonic_sum <= 0.0) {
        return 0.0;
    }
    return alpha_m * m * m / harmonic_sum;
}

double HyperLogLogPlusPlus::correct_raw_estimate(double raw_estimate, std::size_t zero_count) const {
    if (raw_estimate <= 0.0) {
        return 0.0;
    }

    if (raw_estimate <= 2.5 * m) {
        if (zero_count > 0) {
            return linear_counting(m, zero_count);
        }
        return raw_estimate;
    }

    double TWO64 = std::exp2(64.0);
    if (raw_estimate <= TWO64 / 30.0) {
        return raw_estimate;
    }

    double ratio = raw_estimate / TWO64;
    if (ratio >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }

    return -(TWO64 * std::log(1.0 - ratio));
}

std::vector<uint8_t> HyperLogLogPlusPlus::dense_registers_from_sparse() const {
    std::vector<uint8_t> dense(m, 0);

    for (auto& [sparse_idx, sparse_rank] : sparse_entries) {
        IndexRank entry = sparse_to_dense_entry(sparse_idx, sparse_rank);
        if (entry.rank > dense[entry.index]) {
            dense[entry.index] = entry.rank;
        }
    }

    return dense;
}

std::vector<uint8_t> HyperLogLogPlusPlus::registers_for_union() const {
    if (is_sparse()) {
        return dense_registers_from_sparse();
    }
    return registers;
}

void HyperLogLogPlusPlus::require_compatible(const HyperLogLogPlusPlus& other) const {
    if (cfg.b != other.cfg.b || cfg.sparse_precision != other.cfg.sparse_precision) {
        throw std::invalid_argument("Cannot combine HLL++ sketches with different precisions");
    }
}

// ======== PUBLIC ======== //
void HyperLogLogPlusPlus::insert(uint64_t hash) {
    if (is_sparse()) {
        IndexRank entry = sparse_index_and_rank(hash);
        auto it = sparse_entries.find(entry.index);
        uint8_t previous = (it != sparse_entries.end() ? it->second : 0);

        if (entry.rank > previous) {
            sparse_entries[entry.index] = entry.rank;
            invalidate_stats();
        }

        if (sparse_entries.size() > sparse_threshold) {
            promote_to_dense();
        }

        return;
    }

    IndexRank entry = dense_index_and_rank(hash);
    if (entry.rank > registers[entry.index]) {
        registers[entry.index] = entry.rank;
        invalidate_stats();
    }
}

void HyperLogLogPlusPlus::insert(const std::string& str) {
    insert(str_to_u64(str));
}

double HyperLogLogPlusPlus::raw_estimate() const {
    if (is_sparse()) {
        return sparse_estimate();
    }

    RegisterStats stats = register_stats();
    if (stats.nonzero_count == 0) {
        return 0.0;
    }

    std::size_t zero_count = m - stats.nonzero_count;

    return raw_from_harmonic_sum(zero_count + stats.nonzero_sum);
}

double HyperLogLogPlusPlus::estimate() const {
    if (is_sparse()) {
        return sparse_estimate();
    }
    return correct_raw_estimate(raw_estimate(), zero_count());
}

double HyperLogLogPlusPlus::union_estimate(const HyperLogLogPlusPlus& other) const {
    require_compatible(other);
    if (is_sparse() && other.is_sparse()) {
        std::size_t occupied = sparse_entries.size();
        for (auto& [idx, rank] : other.sparse_entries) {
            if (sparse_entries.find(idx) == sparse_entries.end()) {
                ++occupied;
            }
        }

        if (occupied == 0) {
            return 0.0;
        }

        return linear_counting(sparse_m, sparse_m - occupied);
    }

    std::vector<uint8_t> left_registers = registers_for_union();
    std::vector<uint8_t> right_registers = other.registers_for_union();
    std::size_t nonzero_count = 0;
    double nonzero_sum = 0.0;

    for (std::size_t idx = 0; idx < m; ++idx) {
        uint8_t union_register = std::max(left_registers[idx], right_registers[idx]);
        if (union_register == 0) {
            continue;
        }

        ++nonzero_count;
        nonzero_sum += POW2_NEG[union_register];
    }

    std::size_t zero_count = m - nonzero_count;
    double raw_estimate = raw_from_harmonic_sum(zero_count + nonzero_sum);

    return correct_raw_estimate(raw_estimate, zero_count);
}

void HyperLogLogPlusPlus::merge(const HyperLogLogPlusPlus& other) {
    require_compatible(other);
    if (is_sparse() && other.is_sparse()) {
        for (auto& [idx, rank] : other.sparse_entries) {
            auto it = sparse_entries.find(idx);
            uint8_t previous = (it != sparse_entries.end() ? it->second : 0);
            if (rank > previous) {
                sparse_entries[idx] = rank;
            }
        }

        invalidate_stats();

        if (sparse_entries.size() > sparse_threshold) {
            promote_to_dense();
        }

        return;
    }

    if (is_sparse()) {
        promote_to_dense();
    }

    std::vector<uint8_t> other_registers = other.registers_for_union();
    for (std::size_t idx = 0; idx < m; ++idx) {
        if (other_registers[idx] > registers[idx]) {
            registers[idx] = other_registers[idx];
        }
    }

    invalidate_stats();
}

HyperLogLogPlusPlus HyperLogLogPlusPlus::copy() const {
    SketchConfig new_cfg;
    new_cfg.b = cfg.b;
    new_cfg.alpha_override = cfg.alpha_override;
    new_cfg.sparse = cfg.sparse;
    new_cfg.sparse_precision = cfg.sparse_precision;
    new_cfg.sparse_threshold = cfg.sparse_threshold;
    HyperLogLogPlusPlus copied(new_cfg);

    if (is_sparse()) {
        copied.sparse_entries = sparse_entries;
        copied.registers.clear();
        copied.sparse_mode = true;
    } else {
        copied.sparse_entries.clear();
        copied.registers = registers;
        copied.sparse_mode = false;
    }

    return copied;
}

std::size_t HyperLogLogPlusPlus::zero_count() const {
    if (is_sparse()) {
        return sparse_m - sparse_entries.size();
    }

    return std::count(registers.begin(), registers.end(), 0);
}

void HyperLogLogPlusPlus::reset() {
    invalidate_stats();
    if (cfg.sparse) {
        sparse_entries.clear();
        registers.clear();
        sparse_mode = true;
    } else {
        sparse_entries.clear();
        registers.assign(m, 0);
        sparse_mode = false;
    }
}