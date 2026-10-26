// Exercise aliasing/failed rebuilds, and expose key-copy costs of an alternate
// header. Counts are observations, not requirements imposed on the map API.
#ifndef ELASTIC_HASH_HEADER
#define ELASTIC_HASH_HEADER "elastic_hash.hpp"
#endif
#include ELASTIC_HASH_HEADER
#include <iostream>
#include <stdexcept>
#include <string>

struct Key {
  std::string value;
  inline static int copies = 0;
  inline static int copies_until_throw = -1;
  explicit Key(std::string v) : value(std::move(v)) {}
  Key(const Key &other) : value(other.value) {
    if (copies_until_throw == 0)
      throw std::runtime_error("key copy failed");
    if (copies_until_throw > 0)
      --copies_until_throw;
    ++copies;
  }
  Key(Key &&) = default;
  bool operator==(const Key &) const = default;
};
struct Hash {
  std::size_t operator()(const Key &key) const {
    return std::hash<std::string>{}(key.value);
  }
};
void check(bool ok) {
  if (!ok)
    throw std::runtime_error("incorrect result");
}
int main() {
  using Map = ElasticHashTable<Key, int, Hash>;
  Map table;
  table.reserve(100);
  Map::value_type value(Key("a string key exceeding the short buffer"), 7);
  Key::copies = 0;
  auto inserted = table.insert(std::move(value));
  check(inserted.second && inserted.first->second == 7);
  std::cout << "insert_key_copies=" << Key::copies << '\n';
  const auto capacity = table.bucket_count();
  auto duplicate = table.insert(std::move(*inserted.first));
  check(!duplicate.second && duplicate.first->second == 7 && table.size() == 1);
  check(table.bucket_count() == capacity);
  Key::copies = 0;
  table.rehash(2 * capacity);
  std::cout << "rehash_key_copies_per_entry=" << Key::copies << '\n';
  const auto before_failure = table.bucket_count();
  Key::copies_until_throw = 0;
  bool threw = false;
  try {
    table.rehash(2 * before_failure);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  Key::copies_until_throw = -1;
  check(threw && table.size() == 1 && table.bucket_count() == before_failure);
  check(table.find(Key("a string key exceeding the short buffer"))->second ==
        7);
  // Mapped arguments may alias table storage when try_emplace grows the table.
  ElasticHashTable<int, std::string> strings(1);
  strings.insert({1, "aliased mapped value"});
  strings.insert({2, "another mapped value"});
  auto result = strings.try_emplace(3, strings.find(1)->second);
  check(result.second && result.first->second == "aliased mapped value");
  std::cout << "aliasing_and_failed_rebuild_checks=passed\n";
}
