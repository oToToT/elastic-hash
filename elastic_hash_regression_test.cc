#include "elastic_hash.hpp"

#include <concepts>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n";        \
      std::abort();                                                            \
    }                                                                          \
  } while (false)

struct ConstantHash {
  std::size_t operator()(int) const { return 0; }
};
struct SaltedHash {
  std::size_t salt;
  std::size_t operator()(int key) const { return std::hash<int>{}(key) ^ salt; }
};
using Table = ElasticHashTable<int, int>;
static_assert(std::forward_iterator<Table::iterator>);
static_assert(std::forward_iterator<Table::const_iterator>);
static_assert(std::is_same_v<decltype(*std::declval<const Table &>().begin()),
                             const Table::value_type &>);

template <class Hash> void differential(float load, int operations) {
  ElasticHashTable<int, int, Hash> table(1);
  table.max_load_factor(load);
  std::unordered_map<int, int, Hash> expected;
  std::mt19937 rng(31019);
  for (int step = 0; step < operations; ++step) {
    const int key = rng() % 200, value = rng() % 100000;
    switch (rng() % 8) {
    case 0:
    case 1: {
      auto a = table.insert({key, value});
      auto b = expected.insert({key, value});
      CHECK(a.second == b.second);
      CHECK(a.first->first == b.first->first &&
            a.first->second == b.first->second);
      break;
    }
    case 2:
      table.erase(key);
      expected.erase(key);
      break;
    case 3:
      table[key] = value;
      expected[key] = value;
      break;
    case 4:
      CHECK(table.contains(key) == expected.contains(key));
      break;
    case 5:
      table.reserve(rng() % 400);
      break;
    case 6:
      table.rehash(rng() % 400);
      break;
    case 7:
      if (step % 97 == 0) {
        table.clear();
        expected.clear();
      }
      break;
    }
    CHECK(table.size() == expected.size());
    CHECK(table.max_load_factor() == load);
    if (step % 31 == 0) {
      std::unordered_map<int, int> actual;
      const auto &view = table;
      for (const auto &[k, v] : view)
        CHECK(actual.emplace(k, v).second);
      CHECK(actual.size() == expected.size());
      for (const auto &[k, v] : expected) {
        auto it = view.find(k);
        CHECK(it != view.end() && it->second == v);
        CHECK(actual.at(k) == v);
      }
    }
  }
}

struct CopyCheckedKey {
  std::string text;
  inline static int copies = 0;
  inline static int budget = -1;
  explicit CopyCheckedKey(std::string value) : text(std::move(value)) {}
  CopyCheckedKey(const CopyCheckedKey &other) : text(other.text) {
    if (budget == 0)
      throw std::runtime_error("key copy failed");
    if (budget > 0)
      --budget;
    ++copies;
  }
  CopyCheckedKey(CopyCheckedKey &&) = default;
  bool operator==(const CopyCheckedKey &) const = default;
};
struct CopyCheckedHash {
  std::size_t operator()(const CopyCheckedKey &key) const {
    return std::hash<std::string>{}(key.text);
  }
};
void copy_and_aliasing_regressions() {
  using Map = ElasticHashTable<CopyCheckedKey, int, CopyCheckedHash>;
  Map table;
  table.reserve(100);
  Map::value_type entry(CopyCheckedKey("a long key that allocates a string"),
                        7);
  CopyCheckedKey::copies = 0;
  auto inserted = table.insert(std::move(entry));
  CHECK(inserted.second && inserted.first->second == 7);
  CHECK(CopyCheckedKey::copies <= 1); // No redundant const-key allocation.
  const auto buckets = table.bucket_count();
  auto duplicate = table.insert(std::move(*inserted.first));
  CHECK(!duplicate.second && duplicate.first->second == 7);
  CHECK(table.size() == 1 && table.bucket_count() == buckets);
  table.insert({CopyCheckedKey("another long key with an owned string"), 8});
  table.insert({CopyCheckedKey("a third long key with an owned string"), 9});
  CopyCheckedKey::copies = 0;
  table.rehash(2 * buckets);
  CHECK(CopyCheckedKey::copies <= static_cast<int>(table.size()));
  const auto before_failure = table.bucket_count();
  CopyCheckedKey::budget = 1; // Fail after the replacement has one live entry.
  bool threw = false;
  try {
    table.rehash(2 * before_failure);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  CopyCheckedKey::budget = -1;
  CHECK(threw && table.bucket_count() == before_failure && table.size() == 3);
  CHECK(table.find(CopyCheckedKey("a long key that allocates a string"))
            ->second == 7);
  CHECK(table.find(CopyCheckedKey("another long key with an owned string"))
            ->second == 8);
  CHECK(table.find(CopyCheckedKey("a third long key with an owned string"))
            ->second == 9);

  ElasticHashTable<int, std::string> alias(1);
  alias.insert({1, "mapped value used across growth"});
  alias.insert({2, "second value"});
  auto placed = alias.try_emplace(3, alias.find(1)->second);
  CHECK(placed.second &&
        placed.first->second == "mapped value used across growth");
}

void scheduler_regressions() {
  // Check the C++ allowance against the formally counted recurrence.
  std::size_t work[13] = {1, 3};
  for (unsigned epoch = 0; epoch < 13; ++epoch) {
    if (epoch >= 2)
      work[epoch] = 2 * work[epoch - 2] + 3 * (std::size_t{1} << (epoch - 1));
    std::size_t actual = 0;
    for (unsigned i = 1; i < (1u << (epoch + 1)); ++i)
      actual += elastic_hash_detail::epoch_probe_limit(
          epoch, i, std::numeric_limits<std::size_t>::max());
    CHECK(actual == work[epoch]);
    CHECK(actual <= 4 * (std::size_t{1} << epoch));
  }
  // Every pair is emitted once, in increasing j at its level, within the bound.
  std::size_t next[64] = {}, rank = 0;
  for (unsigned epoch = 0; epoch < 20; ++epoch) {
    for (unsigned i = 1; i <= 64; ++i) {
      const auto limit = elastic_hash_detail::epoch_probe_limit(epoch, i, 64);
      while (next[i - 1] < limit) {
        const auto j = ++next[i - 1];
        ++rank;
        CHECK(rank <= 16 * i * j * j);
      }
    }
  }
  CHECK(rank == 64 * 64);
  CHECK(elastic_hash_detail::epoch_probe_limit(200, 64, 123) == 123);
}

int main() {
  scheduler_regressions();
  copy_and_aliasing_regressions();
  Table table;
  for (int i = 0; i < 20; ++i)
    table.insert({i, i});
  auto duplicate = table.insert({0, 100});
  CHECK(!duplicate.second && duplicate.first->second == 0 &&
        table.size() == 20);
  for (int i = 0; i < 20; ++i)
    CHECK(!table.insert({i, -1}).second);
  CHECK(table.size() == 20);

  ElasticHashTable<int, int, ConstantHash> collisions(2);
  collisions.insert({1, 1});
  collisions.insert({2, 2});
  CHECK(
      !collisions.contains(3)); // Previously an infinite loop in a full level.
  collisions.erase(1);
  CHECK(collisions.contains(2));
  collisions.insert({3, 3});
  CHECK(collisions.contains(2) && collisions.contains(3));

  table.max_load_factor(0.5f);
  table.reserve(100);
  CHECK(table.max_load_factor() == 0.5f);
  auto buckets = table.bucket_count();
  for (int i = 20; i < 100; ++i)
    table.insert({i, i});
  CHECK(table.bucket_count() == buckets);
  table.max_load_factor(0.95f);
  for (int i = 0; i < 100; ++i)
    CHECK(table.contains(i));
  const Table &view = table;
  CHECK(view.find(1)->second == 1);
  CHECK(table.begin() == view.begin());
  CHECK(view.begin() == table.begin());
  auto expected_next = table.begin();
  ++expected_next;
  CHECK(table.erase(table.begin()->first) == expected_next);
  CHECK(table.erase(-999) == table.end());
  auto copy = table;
  Table assigned;
  assigned = copy;
  Table moved(std::move(copy));
  copy.insert({999, 999}); // A moved-from table remains usable.
  CHECK(copy.contains(999));
  CHECK(moved.size() == assigned.size());
  for (auto it = moved.begin(); it != moved.end();)
    it = moved.erase(it);
  CHECK(moved.empty());
  moved.insert({42, 43});
  CHECK(moved[42] == 43);

  ElasticHashTable<int, int, SaltedHash> salted(4, SaltedHash{12345});
  for (int i = 0; i < 1000; ++i)
    salted.insert({i, i});
  salted.rehash(4000);
  for (int i = 0; i < 1000; ++i)
    CHECK(salted.find(i)->second == i);

  ElasticHashTable<int, std::unique_ptr<int>> pointers;
  for (int i = 0; i < 100; ++i)
    pointers.try_emplace(i, std::make_unique<int>(i));
  pointers.reserve(1000);
  for (int i = 0; i < 100; ++i)
    CHECK(*pointers.find(i)->second == i);
  auto retained = std::make_unique<int>(5);
  CHECK(!pointers.try_emplace(0, std::move(retained)).second);
  CHECK(retained && *retained == 5);

  for (float invalid :
       {0.0f, -1.0f, 1.1f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
    bool threw = false;
    try {
      table.max_load_factor(invalid);
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    CHECK(threw);
  }
  bool threw = false;
  try {
    table.reserve(std::numeric_limits<std::size_t>::max());
  } catch (const std::length_error &) {
    threw = true;
  }
  CHECK(threw);

  for (float load : {0.01f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 1.0f}) {
    differential<std::hash<int>>(load, 6000);
    differential<ConstantHash>(load, 2000);
  }
  // Exercise every small batch boundary at high occupancy and after deletions.
  for (int capacity : {1, 2, 3, 4, 7, 8, 15, 16, 31, 64, 127, 256}) {
    ElasticHashTable<int, int, ConstantHash> t(capacity);
    t.max_load_factor(1.0f);
    for (int i = 0; i < 300; ++i)
      t.insert({i, i});
    for (int i = 0; i < 300; ++i)
      CHECK(t.contains(i));
    CHECK(!t.contains(301));
    for (int i = 0; i < 300; i += 2)
      t.erase(i);
    for (int i = 1; i < 300; i += 2)
      CHECK(t.contains(i));
  }
  std::cout << "All elastic hash regressions passed\n";
}
