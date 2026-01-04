#ifndef ELASTIC_HASH_HPP
#define ELASTIC_HASH_HPP

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ElasticHashTable {
public:
  using value_type = std::pair<const Key, T>;
  using key_type = Key;
  using mapped_type = T;

  class iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, T>;
    using pointer = value_type *;
    using reference = value_type &;

    iterator(ElasticHashTable *table, size_t index)
        : table_(table), index_(index) {
      advance_to_valid();
    }

    reference operator*() { return *table_->data_[index_]; }
    pointer operator->() { return &(*table_->data_[index_]); }

    iterator &operator++() {
      ++index_;
      advance_to_valid();
      return *this;
    }
    iterator operator++(int) {
      iterator temp = *this;
      ++(*this);
      return temp;
    }

    bool operator==(const iterator &other) const {
      return table_ == other.table_ && index_ == other.index_;
    }

    bool operator!=(const iterator &other) const { return !(*this == other); }

  private:
    friend class ElasticHashTable;

    void advance_to_valid() {
      while (index_ < table_->data_.size() &&
             !table_->data_[index_].has_value()) {
        ++index_;
      }
    }
    ElasticHashTable *table_;
    size_t index_;
  };

  ElasticHashTable() : ElasticHashTable(/*bucket_count=*/1 << 16) {}
  explicit ElasticHashTable(size_t bucket_count)
      : total_bucket_count_(std::bit_floor(bucket_count) * 2 - 1),
        data_(total_bucket_count_),
        level_loads(std::bit_width(total_bucket_count_)) {}

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, data_.size()); }
  void max_load_factor(float ml) { delta_ = 1 - ml; }
  float max_load_factor() const { return 1 - delta_; }

  std::pair<iterator, bool> insert(const value_type &value) {
    auto hash = hash_(value.first);

    auto insert_into_level = [&](size_t level,
                                 size_t probe_limit =
                                     -1) -> std::pair<iterator, bool> {
      size_t level_offset = get_level_offset(level);
      for (size_t j = 0; j < probe_limit; j++) {
        size_t bucket = level_offset + h(level, j + 1, hash);
        if (!data_[bucket].has_value()) {
          data_[bucket].emplace(value);
          ++level_loads[level - 1];
          return {iterator(this, bucket), true};
        } else if (key_equal_(data_[bucket]->first, value.first)) {
          return {iterator(this, bucket), false};
        }
      }
      return {end(), false};
    };

    std::pair<iterator, bool> result = {end(), false};

    // Special case for the first batch.
    if (current_batch_ == 0) {
      result = insert_into_level(1);
    } else {
      size_t current_level = current_batch_;
      size_t current_level_size = get_level_size(current_level);
      size_t next_level_size = current_level_size / 2;
      float current_level_load_factor =
          static_cast<float>(level_loads[current_level - 1]) /
          static_cast<float>(current_level_size);
      float next_level_load_factor =
          static_cast<float>(level_loads[current_level]) /
          static_cast<float>(next_level_size);

      if (current_level_load_factor > delta_ / 2 &&
          next_level_load_factor > 1 - kFirstRoundLoadFactor) {
        result = insert_into_level(current_level,
                                   get_probe_limit(current_level_load_factor));
        if (result.first == end()) {
          result = insert_into_level(current_level + 1);
        }
      } else if (current_level_load_factor <= delta_ / 2) {
        result = insert_into_level(current_level + 1);
      } else {
        result = insert_into_level(current_level);
      }
    }

    // Check if we need to advance to the next batch.
    if (current_batch_ == 0) {
      if (level_loads[0] == get_first_round_max_capacity(1)) {
        current_batch_ = 1;
      }
    } else {
      if (level_loads[current_batch_ - 1] ==
              get_final_max_capacity(current_batch_) &&
          level_loads[current_batch_] ==
              get_first_round_max_capacity(current_batch_ + 1)) {
        current_batch_++;
      }
    }

    return result;
  }

  iterator find(const Key &key) {
    size_t hash = hash_(key);
    // inverse_phi(1) and inverse_phi(2) are std::nullopt, so we start from 3.
    for (uint64_t index = 3;; index++) {
      auto position = inverse_phi(index);
      if (!position.has_value()) {
        continue;
      }
      auto [i, j] = position.value();
      if (i >= level_loads.size()) {
        continue;
      }
      size_t bucket = get_level_offset(i) + h(i, j, hash);
      if (data_[bucket].is_deleted()) {
        continue;
      }
      if (!data_[bucket].has_value()) {
        break;
      }
      if (key_equal_(data_[bucket]->first, key)) {
        return iterator(this, bucket);
      }
    }

    return end();
  }

  iterator erase(const Key &key) {
    iterator it = find(key);
    if (it != end()) {
      erase(it);
    }
    return it;
  }
  iterator erase(iterator pos) {
    auto current_pos = pos++;
    data_[current_pos.index_].reset();
    return pos;
  }

private:
  size_t h(size_t i, size_t j, uint64_t x) const {
    return rapid_hash(i, j, x) & (get_level_size(i) - 1);
  }

  // This phi function is a different function from the one in the paper, but it
  // serves the same purpose. You can use the same proof technique in the paper
  // to show that this phi function also satisfies phi(i, j) <= O(i * j^2). And
  // this is a version that is more friendly to the modern computer hardware.
  static constexpr uint64_t phi(size_t i, size_t j) {
    const size_t k = std::bit_width(j);
    const size_t p = k - 1;
    const size_t high_bit = 1ull << p;
    const size_t j_rest = j & (high_bit - 1);
    return (i << (2 * k - 1)) | (j_rest << k) | high_bit;
  }
  static constexpr std::optional<std::pair<size_t, size_t>>
  inverse_phi(uint64_t v) {
    const size_t p = std::countr_zero(v);
    const size_t k = p + 1;
    const size_t temp = v >> k;
    const size_t j_rest = temp & ((1 << p) - 1);
    const size_t i = temp >> p;
    const size_t j = (1 << p) | j_rest;
    if (i == 0 || j == 0) {
      return std::nullopt;
    }
    return std::make_pair(i, j);
  }

  // 1-based level size
  size_t get_level_size(size_t level) const {
    const size_t max_level_size = std::bit_floor(total_bucket_count_);
    return max_level_size >> (level - 1);
  }

  size_t get_level_offset(size_t level) const {
    const size_t max_level_size = std::bit_floor(total_bucket_count_);
    return (max_level_size - (max_level_size >> (level - 1))) * 2;
  }

  // According to the analysis in the paper, we need to set C to something at
  // least 100(\pi^2/6-1 + 1). These could be even larger due to some hidden
  // constants behiind the big-O notation. Setting C to 200 should be good
  // from emperical experiments.
  size_t get_probe_limit(float level_load_factor) const {
    constexpr size_t C = 200;
    float v1 = -std::log(level_load_factor);
    float v2 = -std::log(delta_);
    return static_cast<size_t>(C * std::min(v1 * v1, v2));
  }

  // We set the max capacity of the first round to be 0.75 of the level size.
  // According the analysis in the paper, any value larger than 0.54 and smaller
  // than 1 should work. We choose 0.75 here from empirical experiments.
  float kFirstRoundLoadFactor = 0.75f;
  size_t get_first_round_max_capacity(size_t level) const {
    size_t level_size = get_level_size(level);
    // Calculate the ceiling of level_size * 0.75.
    return (level_size * 3 + 3) / 4;
  }

  size_t get_final_max_capacity(size_t level) const {
    size_t level_size = get_level_size(level);
    return level_size - static_cast<size_t>(level_size * delta_ / 2);
  }

  class Bucket {
  public:
    Bucket() : state(State::EMPTY) {};
    Bucket(const value_type &val) : state(State::OCCUPIED) {
      new (value) value_type(val);
    }
    Bucket(value_type &&val) : state(State::OCCUPIED) {
      new (value) value_type(std::move(val));
    }
    ~Bucket() { reset(); }
    void reset() {
      if (state == State::OCCUPIED) {
        reinterpret_cast<value_type *>(value)->~value_type();
      }
      state = State::DELETED;
    }
    bool has_value() const { return state == State::OCCUPIED; }
    bool is_deleted() const { return state == State::DELETED; }
    value_type &operator*() { return *reinterpret_cast<value_type *>(value); }
    value_type *operator->() { return reinterpret_cast<value_type *>(value); }
    void emplace(const value_type &val) {
      new (value) value_type(val);
      state = State::OCCUPIED;
    }
    void emplace(value_type &&val) {
      new (value) value_type(std::move(val));
      state = State::OCCUPIED;
    }

  private:
    enum class State : uint8_t { EMPTY, OCCUPIED, DELETED };
    State state = State::EMPTY;
    alignas(value_type) std::byte value[sizeof(value_type)];
  };

  size_t total_bucket_count_;
  float delta_ = 0.25f;
  Hash hash_;
  KeyEqual key_equal_;
  std::vector<Bucket> data_;
  size_t current_batch_ = 0;
  std::vector<size_t> level_loads;

  /*
   * rapidhash V3 - Very fast, high quality, platform-independent hashing
   * algorithm.
   *
   * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
   *
   * Copyright (C) 2025 Nicolas De Carli
   *
   * Permission is hereby granted, free of charge, to any person obtaining a
   * copy of this software and associated documentation files (the "Software"),
   * to deal in the Software without restriction, including without limitation
   * the rights to use, copy, modify, merge, publish, distribute, sublicense,
   * and/or sell copies of the Software, and to permit persons to whom the
   * Software is furnished to do so, subject to the following conditions:
   *
   * The above copyright notice and this permission notice shall be included in
   * all copies or substantial portions of the Software.
   *
   * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   * DEALINGS IN THE SOFTWARE.
   *
   * You can contact the author at:
   *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
   */
  constexpr void rapid_mum(uint64_t *A, uint64_t *B) const noexcept {
    __uint128_t r = *A;
    r *= *B;
    *A = (uint64_t)r;
    *B = (uint64_t)(r >> 64);
  }

  /*
   *  Multiply and xor mix function.
   *
   *  @param A  64-bit number.
   *  @param B  64-bit number.
   *
   *  Calculates 128-bit C = A * B.
   *  Returns 64-bit xor between high and low 64 bits of C.
   */
  constexpr uint64_t rapid_mix(uint64_t A, uint64_t B) const noexcept {
    rapid_mum(&A, &B);
    return A ^ B;
  }

  constexpr uint64_t rapid_hash(uint64_t x, uint64_t y,
                                uint64_t z) const noexcept {
    static constexpr uint64_t secret[8] = {
        0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull,
        0x4d5a2da51de1aa47ull, 0xa0761d6478bd642full, 0xe7037ed1a0b428dbull,
        0x90ed1765281c388cull, 0xaaaaaaaaaaaaaaaaull};
    uint64_t a = y ^ 24 ^ secret[1];
    uint64_t b =
        z ^ rapid_mix(x ^ secret[2], y ^ rapid_mix(secret[2], secret[1]));
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[7], b ^ secret[1] ^ 24);
  }
};

#endif // ELASTIC_HASH_HPP
