#ifndef ELASTIC_HASH_HPP
#define ELASTIC_HASH_HPP

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace elastic_hash_detail {
// The same dyadic allowance modeled in verification/Scheduler.lean.
constexpr std::size_t epoch_probe_limit(unsigned epoch, unsigned level,
                                        std::size_t max_probe) {
  const unsigned band = std::bit_width(level) - 1;
  if (band > epoch)
    return 0;
  const unsigned exponent = (epoch - band) / 2;
  constexpr auto bits = std::numeric_limits<std::size_t>::digits;
  return exponent >= bits ? max_probe
                          : std::min(max_probe, std::size_t{1} << exponent);
}
} // namespace elastic_hash_detail

// A growing map using the paper's elastic insertion batches. The paper's
// probabilistic bounds assume independent random probes and insertion-only
// operation; the deterministic mixer, fallback, deletion and growth here are
// engineering extensions. See verification/README.md for the proved scope.
template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ElasticHashTable {
  template <bool Const> class iterator_impl {
    using table_type =
        std::conditional_t<Const, const ElasticHashTable, ElasticHashTable>;
    friend class ElasticHashTable;
    template <bool> friend class iterator_impl;
    table_type *table_ = nullptr;
    std::size_t index_ = 0;
    iterator_impl(table_type *table, std::size_t index)
        : table_(table), index_(index) {
      while (index_ < table_->data_.size() && !table_->data_[index_])
        ++index_;
    }

  public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, T>;
    using pointer = std::conditional_t<Const, const value_type *, value_type *>;
    using reference =
        std::conditional_t<Const, const value_type &, value_type &>;
    iterator_impl() = default;
    template <bool Other>
      requires(Const && !Other)
    iterator_impl(const iterator_impl<Other> &other)
        : table_(other.table_), index_(other.index_) {}
    reference operator*() const { return *table_->data_[index_]; }
    pointer operator->() const { return &*table_->data_[index_]; }
    iterator_impl &operator++() {
      do {
        ++index_;
      } while (index_ < table_->data_.size() && !table_->data_[index_]);
      return *this;
    }
    iterator_impl operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }
    template <bool Other>
    bool operator==(const iterator_impl<Other> &other) const {
      return table_ == other.table_ && index_ == other.index_;
    }
  };

  struct Level {
    std::size_t offset, capacity;
    std::size_t used =
        0; // Live slots plus tombstones; monotone until a rebuild.
    std::size_t live = 0;
    std::size_t max_probe = 0; // Largest 1-based probe used by any insertion.
  };
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();
  static constexpr unsigned word_bits =
      std::numeric_limits<std::size_t>::digits;

public:
  using value_type = std::pair<const Key, T>;
  using key_type = Key;
  using mapped_type = T;
  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;

  ElasticHashTable() : ElasticHashTable(16) {}
  explicit ElasticHashTable(std::size_t bucket_count, const Hash &hash = Hash{},
                            const KeyEqual &equal = KeyEqual{})
      : hash_(hash), key_equal_(equal), data_(rounded_capacity(bucket_count)),
        tags_(data_.size(), 0) {
    for (std::size_t capacity = (data_.size() + 1) / 2, offset = 0; capacity;
         offset += capacity, capacity /= 2)
      levels_.push_back({offset, capacity});
  }
  ElasticHashTable(const ElasticHashTable &) = default;
  ElasticHashTable &operator=(const ElasticHashTable &other) {
    if (this != &other) {
      ElasticHashTable copy(other);
      swap(copy);
    }
    return *this;
  }
  ElasticHashTable(ElasticHashTable &&other)
      : ElasticHashTable(1, other.hash_, other.key_equal_) {
    swap(other);
  }
  ElasticHashTable &operator=(ElasticHashTable &&other) {
    if (this != &other) {
      ElasticHashTable moved(std::move(other));
      swap(moved);
    }
    return *this;
  }
  void swap(ElasticHashTable &other) noexcept(
      std::is_nothrow_swappable_v<Hash> &&
      std::is_nothrow_swappable_v<KeyEqual>) {
    using std::swap;
    swap(hash_, other.hash_);
    swap(key_equal_, other.key_equal_);
    swap(data_, other.data_);
    swap(tags_, other.tags_);
    swap(levels_, other.levels_);
    swap(size_, other.size_);
    swap(used_, other.used_);
    swap(batch_, other.batch_);
    swap(load_factor_, other.load_factor_);
    swap(active_levels_, other.active_levels_);
  }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, data_.size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, data_.size()); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  std::size_t bucket_count() const { return data_.size(); }
  float load_factor() const { return static_cast<float>(size_) / data_.size(); }
  float max_load_factor() const { return load_factor_; }
  void max_load_factor(float value) {
    if (!std::isfinite(value) || value <= 0 || value > 1)
      throw std::invalid_argument("max_load_factor must be in (0, 1]");
    if (value == load_factor_)
      return;
    // Batch targets depend on delta: changing it requires rebuilding the
    // batches.
    rebuild(std::max(data_.size(), buckets_for(size_, value)), value);
  }

  std::pair<iterator, bool> insert(const value_type &value) {
    return insert(value_type(value));
  }
  std::pair<iterator, bool> insert(value_type &&value) {
    const auto hash = hash_(value.first);
    const auto found = find_index(value.first, hash);
    if (found != npos)
      return {iterator(this, found), false};
    // Stage before a possible rebuild, including when value aliases this table.
    value_type staged(std::move(value));
    prepare_insert();
    return {iterator(this, insert_absent(std::move(staged), hash)), true};
  }
  template <class... Args> std::pair<iterator, bool> insert(Args &&...args) {
    return insert(value_type(std::forward<Args>(args)...));
  }
  template <class... Args>
  std::pair<iterator, bool> try_emplace(const Key &key, Args &&...args) {
    return emplace_key(key, std::forward<Args>(args)...);
  }
  template <class... Args>
  std::pair<iterator, bool> try_emplace(Key &&key, Args &&...args) {
    return emplace_key(std::move(key), std::forward<Args>(args)...);
  }
  T &operator[](const Key &key) { return try_emplace(key).first->second; }
  T &operator[](Key &&key) { return try_emplace(std::move(key)).first->second; }
  const_iterator find(const Key &key) const {
    const auto index = find_index(key, hash_(key));
    return const_iterator(this, index == npos ? data_.size() : index);
  }
  iterator find(const Key &key) {
    const auto index = find_index(key, hash_(key));
    return iterator(this, index == npos ? data_.size() : index);
  }
  bool contains(const Key &key) const {
    return find_index(key, hash_(key)) != npos;
  }

  // Both erase overloads return the next physical iterator (end if absent).
  iterator erase(const Key &key) {
    auto it = find(key);
    return it == end() ? it : erase(it);
  }
  iterator erase(iterator pos) {
    auto next = pos;
    ++next;
    const auto index = pos.index_;
    data_[index].reset();
    tags_[index] = 1; // A tombstone must not terminate a search.
    --size_;
    for (std::size_t i = 0; i < levels_.size(); ++i) {
      auto &level = levels_[i];
      if (index < level.offset + level.capacity) {
        if (--level.live == 0)
          active_levels_ &= ~(std::uint64_t{1} << i);
        break;
      }
    }
    return next;
  }
  void clear() {
    for (auto &value : data_)
      value.reset();
    std::fill(tags_.begin(), tags_.end(), 0);
    for (auto &level : levels_)
      level.used = level.live = level.max_probe = 0;
    size_ = used_ = batch_ = 0;
    active_levels_ = 0;
  }
  void reserve(std::size_t count) {
    if (count > capacity_limit())
      rehash(buckets_for(count, load_factor_));
  }
  void rehash(std::size_t count) {
    rebuild(std::max(count, buckets_for(size_, load_factor_)), load_factor_);
  }

private:
  static std::size_t rounded_capacity(std::size_t requested) {
    // At least three slots, and leave room for offsets and the bounded
    // fallback.
    constexpr auto limit = std::size_t{1} << (word_bits - 2);
    if (requested >= limit)
      throw std::length_error("elastic hash capacity overflow");
    return std::bit_ceil(std::max<std::size_t>(requested + 1, 4)) - 1;
  }
  static std::size_t buckets_for(std::size_t elements, float load) {
    const long double needed =
        std::ceil(static_cast<long double>(elements) / load);
    constexpr auto limit = std::size_t{1} << (word_bits - 2);
    if (needed >= limit || elements >= limit - 1)
      throw std::length_error("elastic hash capacity overflow");
    return std::max(elements + 1, static_cast<std::size_t>(needed));
  }
  std::size_t capacity_limit() const {
    return std::min(data_.size() - 1, static_cast<std::size_t>(
        static_cast<long double>(data_.size()) * load_factor_));
  }
  double delta() const {
    return std::max(1.0 - load_factor_, 1.0 / data_.size());
  }
  static std::size_t first_target(const Level &level) {
    return level.capacity - level.capacity / 4;
  }
  std::size_t final_target(const Level &level) const {
    return level.capacity -
           static_cast<std::size_t>(level.capacity * delta() / 2);
  }
  std::size_t probe_bucket(std::size_t i, std::size_t j,
                           std::size_t hash) const {
    const auto &level = levels_[i];
    // Keep the paper-style random prefix. A full linear cycle after that prefix
    // guarantees termination even for a pathological deterministic hash stream.
    const auto position = j <= level.capacity ? rapid_hash(i + 1, j, hash)
                                              : hash + (j - level.capacity - 1);
    return level.offset + (position & (level.capacity - 1));
  }

  std::size_t find_index(const Key &key, std::size_t hash) const {
    auto active = active_levels_;
    if (!active)
      return npos;
    // Dyadic energy epochs. Level i (1-based) has band floor(log2(i));
    // epoch r visits j <= 2^floor((r-band)/2). Each pair is visited once.
    // verification/Scheduler.lean proves the corresponding rank <= 16*i*j^2.
    std::size_t next[word_bits] = {};
    for (unsigned epoch = 0; active; ++epoch) {
      auto mask = active;
      while (mask) {
        const unsigned i = std::countr_zero(mask);
        const unsigned band = std::bit_width(i + 1) - 1;
        if (band > epoch)
          break;
        mask &= mask - 1;
        const auto &level = levels_[i];
        const auto limit = elastic_hash_detail::epoch_probe_limit(
            epoch, i + 1, level.max_probe);
        auto j = next[i];
        while (j < limit) {
          const auto bucket = probe_bucket(i, ++j, hash);
          const auto stored_tag = tags_[bucket];
          if (stored_tag == 0) {
            active &= ~(std::uint64_t{1} << i);
            break;
          }
          if (stored_tag >= 2 && key_equal_(data_[bucket]->first, key))
            return bucket;
        }
        next[i] = j;
        if (j == level.max_probe)
          active &= ~(std::uint64_t{1} << i);
      }
    }
    return npos;
  }
  template <class K, class... Args>
  std::pair<iterator, bool> emplace_key(K &&key, Args &&...args) {
    const auto hash = hash_(key);
    const auto found = find_index(key, hash);
    if (found != npos)
      return {iterator(this, found), false};
    value_type value(std::piecewise_construct,
                     std::forward_as_tuple(std::forward<K>(key)),
                     std::forward_as_tuple(std::forward<Args>(args)...));
    prepare_insert();
    return {iterator(this, insert_absent(std::move(value), hash)), true};
  }
  void prepare_insert() {
    // Reclaim tombstones without growing; historical occupancy determines
    // batches.
    if (used_ != size_ &&
        (used_ - size_ > data_.size() / 8 || used_ >= capacity_limit()))
      rehash(data_.size());
    if (size_ >= capacity_limit()) {
      rehash(std::max(data_.size() + 1, buckets_for(size_ + 1, load_factor_)));
    }
  }
  std::size_t insert_absent(value_type &&value, std::size_t hash) {
    auto into = [&](std::size_t i, std::size_t limit = npos) {
      auto &level = levels_[i];
      limit = std::min(limit, 2 * level.capacity);
      for (std::size_t j = 1; j <= limit; ++j) {
        const auto bucket = probe_bucket(i, j, hash);
        if (tags_[bucket] >= 2)
          continue;
        data_[bucket].emplace(std::move(value));
        if (tags_[bucket] == 0) {
          ++level.used;
          ++used_;
        }
        tags_[bucket] = 2;
        ++level.live;
        ++size_;
        level.max_probe = std::max(level.max_probe, j);
        active_levels_ |= std::uint64_t{1} << i;
        return bucket;
      }
      return npos;
    };
    std::size_t result;
    if (batch_ == 0) {
      result = into(0);
      if (levels_[0].used >= first_target(levels_[0]))
        batch_ = 1;
    } else {
      const auto i = batch_ - 1;
      auto &current = levels_[i];
      auto &next = levels_[i + 1];
      const bool current_done = current.used >= final_target(current);
      const bool next_done = next.used >= first_target(next);
      if (current_done)
        result = into(i + 1);
      else if (next_done)
        result = into(i);
      else {
        const double epsilon =
            1.0 - static_cast<double>(current.used) / current.capacity;
        const double log_epsilon = -std::log(epsilon);
        const auto limit = static_cast<std::size_t>(
            200 * std::min(log_epsilon * log_epsilon, -std::log(delta())));
        result = into(i, limit);
        if (result == npos)
          result = into(i + 1);
      }
      if (current.used >= final_target(current) &&
          next.used >= first_target(next))
        ++batch_;
    }
    if (result == npos)
      throw std::logic_error("elastic hash batch has no free slot");
    return result;
  }
  void rebuild(std::size_t count, float load) {
    ElasticHashTable replacement(count, hash_, key_equal_);
    replacement.load_factor_ = load;
    for (auto &bucket : data_) {
      if (!bucket)
        continue;
      const auto hash = replacement.hash_(bucket->first);
      if constexpr (std::is_copy_constructible_v<value_type>) {
        value_type value(*bucket);
        replacement.insert_absent(std::move(value), hash);
      } else {
        replacement.insert_absent(std::move(*bucket), hash);
      }
    }
    swap(replacement);
  }

  Hash hash_;
  KeyEqual key_equal_;
  std::vector<std::optional<value_type>> data_;
  // 0 = never occupied, 1 = tombstone, 2 = live entry.
  std::vector<std::uint8_t> tags_;
  std::vector<Level> levels_;
  std::size_t size_ = 0, used_ = 0, batch_ = 0;
  float load_factor_ = 0.75f;
  std::uint64_t active_levels_ = 0;

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
    constexpr uint64_t secret[8] = {
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
