#include "elastic_hash.hpp"

#include <map>
#include <string>

#include "gtest/gtest.h"

TEST(ElasticHashTableTest, Construction) {
  ElasticHashTable<int, int> table;
  EXPECT_EQ(table.begin(), table.end());
}

TEST(ElasticHashTableTest, InsertAndFind) {
  ElasticHashTable<int, std::string> table;
  auto [it, success] = table.insert({1, "one"});
  EXPECT_TRUE(success);
  EXPECT_NE(it, table.end());
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "one");

  auto found_it = table.find(1);
  EXPECT_NE(found_it, table.end());
  EXPECT_EQ(found_it->first, 1);
  EXPECT_EQ(found_it->second, "one");
}

TEST(ElasticHashTableTest, InsertExisting) {
  ElasticHashTable<int, std::string> table;
  table.insert({1, "one"});
  auto [it, success] = table.insert({1, "uno"});
  EXPECT_FALSE(success);
  EXPECT_NE(it, table.end());
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "one"); // Should not have been updated
}

TEST(ElasticHashTableTest, FindNonExistent) {
  ElasticHashTable<int, std::string> table;
  table.insert({1, "one"});
  auto it = table.find(2);
  EXPECT_EQ(it, table.end());
}

TEST(ElasticHashTableTest, Erase) {
  ElasticHashTable<int, std::string> table;
  table.insert({1, "one"});
  table.insert({2, "two"});

  auto it = table.find(1);
  EXPECT_NE(it, table.end());

  table.erase(1);

  auto it_after_erase = table.find(1);
  EXPECT_EQ(it_after_erase, table.end());

  auto it2 = table.find(2);
  EXPECT_NE(it2, table.end());
}

TEST(ElasticHashTableTest, EraseIterator) {
    ElasticHashTable<int, std::string> table;
    table.insert({1, "one"});
    table.insert({2, "two"});

    auto it = table.find(1);
    ASSERT_NE(it, table.end());

    table.erase(it);

    auto it_after_erase = table.find(1);
    EXPECT_EQ(it_after_erase, table.end());
    
    int count = 0;
    for(auto& val : table) {
        count++;
        EXPECT_EQ(val.first, 2);
        EXPECT_EQ(val.second, "two");
    }
    EXPECT_EQ(count, 1);
}


TEST(ElasticHashTableTest, Iterator) {
  ElasticHashTable<int, std::string> table;
  table.insert({1, "one"});
  table.insert({2, "two"});
  table.insert({3, "three"});

  int count = 0;
  std::map<int, std::string> expected = {{1, "one"}, {2, "two"}, {3, "three"}};
  for (const auto& pair : table) {
    EXPECT_EQ(expected.count(pair.first), 1);
    EXPECT_EQ(expected[pair.first], pair.second);
    expected.erase(pair.first);
    count++;
  }
  EXPECT_EQ(count, 3);
  EXPECT_TRUE(expected.empty());
}

TEST(ElasticHashTableTest, LargeNumberOfInserts) {
    ElasticHashTable<int, int> table(16);
    const int num_inserts = 100;
    for (int i = 0; i < num_inserts; ++i) {
        auto [it, success] = table.insert({i, i});
        ASSERT_TRUE(success);
        ASSERT_NE(it, table.end());
    }

    for (int i = 0; i < num_inserts; ++i) {
        auto it = table.find(i);
        ASSERT_NE(it, table.end());
        EXPECT_EQ(it->first, i);
        EXPECT_EQ(it->second, i);
    }

    int count = 0;
    for([[maybe_unused]] auto& val : table) {
        count++;
    }
    EXPECT_EQ(count, num_inserts);
}