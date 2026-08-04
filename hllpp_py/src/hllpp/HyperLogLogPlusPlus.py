############################################
# Copyright (c) 2026 Shun/修海 (@shun4midx) #
# Project: HyperLogLogPlusPlus-Autocorrect #
# File Type: Python file                   #
# File: HyperLogLogPlusPlus.py             #
# Variant: HLL++ sparse representation     #
############################################

import math

from .Hasher import str_to_u64

_POW2_NEG = tuple(2.0 ** -value for value in range(256))

class SketchConfig:
    def __init__(self, b=10, alpha_override=-1.0, sparse=True, sparse_precision=25, sparse_threshold=None):
        if not isinstance(b, int) or isinstance(b, bool):
            raise TypeError("`b` must be an integer")

        if not 4 <= b <= 20:
            raise ValueError("`b` must be between 4 and 20")

        if not isinstance(sparse_precision, int) or isinstance(sparse_precision, bool):
            raise TypeError("`sparse_precision` must be an integer")

        if not b <= sparse_precision <= 63:
            raise ValueError("`sparse_precision` must satisfy `b <= sparse_precision <= 63`")

        if sparse_threshold is not None and (not isinstance(sparse_threshold, int) or isinstance(sparse_threshold, bool) or sparse_threshold <= 0):
            raise ValueError("`sparse_threshold` must be a positive integer or None")

        self.b = b
        self.alpha_override = float(alpha_override)
        self.sparse = bool(sparse)
        self.sparse_precision = sparse_precision
        self.sparse_threshold = sparse_threshold

class HyperLogLogPlusPlus:
    # HLL using the higher-precision sparse representation introduced by HLL++. Sparse mode stores occupied p'-bit hash prefixes and estimates cardinality using Linear Counting over 2**p' buckets. When sparse stoarage exceeds its threshold, entries are converted into ordinary p-bit HLL registers. 

    def __init__(self, cfg=None):
        if cfg is None:
            cfg = SketchConfig()

        self.cfg = cfg
        self.m = 1 << self.cfg.b
        self.sparse_m = 1 << self.cfg.sparse_precision
        self.alpha_m = self.compute_alpha()

        if self.cfg.sparse:
            # sparse_index -> maximum rank after the p'-bit prefix
            self.sparse_entries = {}
            self.registers = None
        else:
            self.sparse_entries = None
            self.registers = bytearray(self.m)

        self._stats_cache = {}

        self.sparse_threshold = max(16, (6 * self.m) // 32) if self.cfg.sparse_threshold is None else self.cfg.sparse_threshold

    def _invalidate_stats(self):
        self._stats_cache.clear()

    def compute_alpha(self):
        if self.cfg.alpha_override > 0:
            return self.cfg.alpha_override

        if self.m == 16:
            return 0.673

        if self.m == 32:
            return 0.697

        if self.m == 64:
            return 0.709

        return 0.7213 / (1.0 + 1.079 / self.m)

    @staticmethod
    def _hash_value(value):
        if isinstance(value, str):
            return str_to_u64(value)

        if isinstance(value, int) and not isinstance(value, bool):
            if not 0 <= value < (1 << 64):
                raise ValueError("Integer hash values must fit in unsigned 64 bits")
            return value

        raise TypeError("`value` must be a string or an unsigned 64-bit integer")

    @staticmethod
    def _rank(suffix, suffix_bits):
        if suffix == 0:
            return suffix_bits + 1
        return suffix_bits - suffix.bit_length() + 1

    def _dense_index_and_rank(self, hash_value):
        suffix_bits = 64 - self.cfg.b
        idx = hash_value >> suffix_bits
        suffix = hash_value & ((1 << suffix_bits) - 1)
        return idx, self._rank(suffix, suffix_bits)

    def _sparse_index_and_rank(self, hash_value):
        suffix_bits = 64 - self.cfg.sparse_precision
        idx = hash_value >> suffix_bits
        suffix = hash_value & ((1 << suffix_bits) - 1)
        return idx, self._rank(suffix, suffix_bits)

    def _is_sparse(self):
        return self.registers is None

    def _sparse_to_dense_entry(self, sparse_idx, sparse_rank):
        extra_count = self.cfg.sparse_precision - self.cfg.b
        dense_idx = sparse_idx >> extra_count
        
        if extra_count == 0:
            return (dense_idx, sparse_rank)

        extra_mask = (1 << extra_count) - 1
        extra_bits = sparse_idx & extra_mask
        if extra_bits == 0:
            dense_rank = extra_count + sparse_rank
        else:
            dense_rank = extra_count - extra_bits.bit_length() + 1
            
        return (dense_idx, dense_rank)
        
    def _promote_to_dense(self):
        if not self._is_sparse():
            return

        dense = bytearray(self.m)

        for (sparse_idx, sparse_rank) in self.sparse_entries.items():
            dense_idx, dense_rank = self._sparse_to_dense_entry(sparse_idx, sparse_rank)
            
            if dense_rank > dense[dense_idx]:
                dense[dense_idx] = dense_rank

        self.registers = dense
        self.sparse_entries = None
        self._invalidate_stats()

    def insert(self, value):
        hash_value = self._hash_value(value)

        if self._is_sparse():
            idx, rank = self._sparse_index_and_rank(hash_value)
            previous = self.sparse_entries.get(idx, 0)
            
            if rank > previous:
                self.sparse_entries[idx] = rank
                self._invalidate_stats()

            if len(self.sparse_entries) > self.sparse_threshold:
                self._promote_to_dense()

            return

        idx, rank = self._dense_index_and_rank(hash_value)
        
        if rank > self.registers[idx]:
            self.registers[idx] = rank
            self._invalidate_stats()

    @staticmethod
    def _linear_counting(bucket_count, zero_count):
        if zero_count <= 0:
            return float("inf")

        return bucket_count * math.log(bucket_count / zero_count)

    def _sparse_estimate(self):
        occupied = len(self.sparse_entries)

        if occupied == 0:
            return 0.0

        return self._linear_counting(self.sparse_m, self.sparse_m - occupied)

    def register_stats(self): # return dense (nonzero_count, nonzero_harmonic_sum)
        if self._is_sparse():
            raise RuntimeError("Dense register statistics are unavailable in sparse mode")

        cached = self._stats_cache.get(0)

        if cached is not None:
            return cached

        nonzero_count = 0
        nonzero_sum = 0.0

        for register in self.registers:
            if register == 0:
                continue

            nonzero_count += 1
            nonzero_sum += _POW2_NEG[register]

        result = (nonzero_count, nonzero_sum)

        self._stats_cache[0] = result
        return result

    def _raw_from_harmonic_sum(self, harmonic_sum):
        if harmonic_sum <= 0.0:
            return 0.0
        
        return self.alpha_m * self.m**2 / harmonic_sum

    def raw_estimate(self):
        if self._is_sparse():
            return self._sparse_estimate()

        nonzero_count, nonzero_sum = self.register_stats()
        
        if nonzero_count == 0:
            return 0.0

        zero_count = self.m - nonzero_count
        
        return self._raw_from_harmonic_sum(zero_count + nonzero_sum)

    def _correct_raw_estimate(self, raw_estimate, zero_count):
        if raw_estimate <= 0.0:
            return 0.0

        if raw_estimate <= 2.5 * self.m:
            if zero_count > 0:
                return self._linear_counting(self.m, zero_count)

            return raw_estimate

        if raw_estimate <= (1 << 64) / 30:
            return raw_estimate

        ratio = raw_estimate / (1 << 64)
        
        if ratio >= 1.0:
            return float("inf")

        return -((1 << 64) * math.log(1.0 - ratio))

    def estimate(self):
        if self._is_sparse():
            return self._sparse_estimate()

        return self._correct_raw_estimate(self.raw_estimate(), self.zero_count())

    def _dense_registers_from_sparse(self):
        dense = bytearray(self.m)

        for (sparse_idx, sparse_rank,) in self.sparse_entries.items():
            dense_idx, dense_rank = self._sparse_to_dense_entry(sparse_idx, sparse_rank)

            if dense_rank > dense[dense_idx]:
                dense[dense_idx] = dense_rank

        return dense

    def _registers_for_union(self):
        if self._is_sparse():
            return self._dense_registers_from_sparse()

        return self.registers

    def _require_compatible(self, other):
        if not isinstance(other, HyperLogLogPlusPlus):
            raise TypeError("`other` must be a HyperLogLogPlusPlus sketch")

        if self.cfg.b != other.cfg.b or self.cfg.sparse_precision != other.cfg.sparse_precision:
            raise ValueError("Cannot combine HLL++ sketches with different precisions")

    def union_estimate(self, other):
        self._require_compatible(other)

        if self._is_sparse() and other._is_sparse():
            occupied = len(self.sparse_entries.keys() | other.sparse_entries.keys())

            if occupied == 0:
                return 0.0

            return self._linear_counting(self.sparse_m, self.sparse_m - occupied)

        left_registers = self._registers_for_union()
        right_registers = other._registers_for_union()
        
        nonzero_count = 0
        nonzero_sum = 0.0

        for idx in range(self.m):
            union_register = max(left_registers[idx], right_registers[idx])

            if union_register == 0:
                continue

            nonzero_count += 1
            nonzero_sum += _POW2_NEG[union_register]

        zero_count = self.m - nonzero_count

        raw_estimate = self._raw_from_harmonic_sum(zero_count + nonzero_sum)
        
        return self._correct_raw_estimate(raw_estimate, zero_count)

    def merge(self, other):
        self._require_compatible(other)

        if self._is_sparse() and other._is_sparse():
            for idx, rank in other.sparse_entries.items():
                if rank > self.sparse_entries.get(idx, 0):
                    self.sparse_entries[idx] = rank

            self._invalidate_stats()

            if len(self.sparse_entries) > self.sparse_threshold:
                self._promote_to_dense()

            return

        if self._is_sparse():
            self._promote_to_dense()

        other_registers = other._registers_for_union()

        for idx in range(self.m):
            if other_registers[idx] > self.registers[idx]:
                self.registers[idx] = other_registers[idx]

        self._invalidate_stats()

    def copy(self):
        new_cfg = SketchConfig(b=self.cfg.b, alpha_override=self.cfg.alpha_override, sparse=self.cfg.sparse, sparse_precision=self.cfg.sparse_precision, sparse_threshold=self.cfg.sparse_threshold)
        copied = HyperLogLogPlusPlus(new_cfg)

        if self._is_sparse():
            copied.sparse_entries = dict(self.sparse_entries)
            copied.registers = None
        else:
            copied.sparse_entries = None
            copied.registers = bytearray(self.registers)

        return copied

    def zero_count(self):
        if self._is_sparse():
            return self.sparse_m - len(self.sparse_entries)
            
        return self.registers.count(0)

    def reset(self):
        self._invalidate_stats()

        if self.cfg.sparse:
            self.sparse_entries = {}
            self.registers = None
        else:
            self.sparse_entries = None
            self.registers = bytearray(self.m)

HyperLogLog = HyperLogLogPlusPlus