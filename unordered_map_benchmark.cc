#include "benchmark/benchmark.h"
#include "elastic_hash.hpp"

#include <ext/pb_ds/assoc_container.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define BENCHMARK_ALL(FUNC, TYPE, ARGS)                                        \
  BENCHMARK_TEMPLATE(FUNC, std::unordered_map<TYPE, TYPE>)->ARGS;              \
  BENCHMARK_TEMPLATE(FUNC, __gnu_pbds::cc_hash_table<TYPE, TYPE>)->ARGS;       \
  BENCHMARK_TEMPLATE(FUNC, __gnu_pbds::gp_hash_table<TYPE, TYPE>)->ARGS;       \
  BENCHMARK_TEMPLATE(FUNC, ElasticHashTable<TYPE, TYPE>)->ARGS;

// Benchmark for insertion of different types
template <typename MapType> static void BM_Map_Insert(benchmark::State &state) {
  using K = typename MapType::key_type;
  using V = typename MapType::mapped_type;
  for (auto _ : state) {
    MapType map;
    for (int i = 0; i < state.range(0); ++i) {
      if constexpr (std::is_same_v<K, std::string>) {
        map.insert({std::to_string(i), std::to_string(i)});
      } else {
        map.insert({(K)i, (V)i});
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_ALL(BM_Map_Insert, int, Range(8, 8 << 10));
BENCHMARK_ALL(BM_Map_Insert, std::string, Range(8, 8 << 10));

// Benchmark for lookup
template <typename MapType> static void BM_Map_Lookup(benchmark::State &state) {
  using K = typename MapType::key_type;
  using V = typename MapType::mapped_type;
  MapType map;
  std::vector<K> keys;
  for (int i = 0; i < state.range(0); ++i) {
    if constexpr (std::is_same_v<K, std::string>) {
      map.insert({std::to_string(i), std::to_string(i)});
      keys.push_back(std::to_string(i));
    } else {
      map.insert({(K)i, (V)i});
      keys.push_back((K)i);
    }
  }

  for (auto _ : state) {
    for (const auto &key : keys) {
      auto it = map.find(key);
      if (it != map.end()) {
        benchmark::DoNotOptimize(it->second);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_ALL(BM_Map_Lookup, int, Range(8, 8 << 10));
BENCHMARK_ALL(BM_Map_Lookup, std::string, Range(8, 8 << 10));

// Benchmark for unsuccessful lookup
template <typename MapType>
static void BM_Map_Lookup_Miss(benchmark::State &state) {
  using K = typename MapType::key_type;
  using V = typename MapType::mapped_type;
  MapType map;
  std::vector<K> keys;
  for (int i = 0; i < state.range(0); ++i) {
    if constexpr (std::is_same_v<K, std::string>) {
      map.insert({std::to_string(i), std::to_string(i)});
      keys.push_back(std::to_string(i + state.range(0)));
    } else {
      map.insert({(K)i, (V)i});
      keys.push_back((K)(i + state.range(0)));
    }
  }

  for (auto _ : state) {
    for (const auto &key : keys) {
      auto it = map.find(key);
      if (it != map.end()) {
        benchmark::DoNotOptimize(it->second);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_ALL(BM_Map_Lookup_Miss, int, Range(8, 8 << 10));
BENCHMARK_ALL(BM_Map_Lookup_Miss, std::string, Range(8, 8 << 10));

// Benchmark for iteration
template <typename MapType>
static void BM_Map_Iterate(benchmark::State &state) {
  using K = typename MapType::key_type;
  using V = typename MapType::mapped_type;
  MapType map;
  for (int i = 0; i < state.range(0); ++i) {
    if constexpr (std::is_same_v<K, std::string>) {
      map.insert({std::to_string(i), std::to_string(i)});
    } else {
      map.insert({(K)i, (V)i});
    }
  }

  for (auto _ : state) {
    for (auto &pair : map) {
      K key = pair.first;
      benchmark::DoNotOptimize(key);
      benchmark::DoNotOptimize(pair.second);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_ALL(BM_Map_Iterate, int, Range(8, 8 << 10));
BENCHMARK_ALL(BM_Map_Iterate, std::string, Range(8, 8 << 10));

// Benchmark for deletion
template <typename MapType> static void BM_Map_Delete(benchmark::State &state) {
  using K = typename MapType::key_type;
  using V = typename MapType::mapped_type;
  MapType map;
  std::vector<K> keys;
  for (int i = 0; i < state.range(0); ++i) {
    if constexpr (std::is_same_v<K, std::string>) {
      keys.push_back(std::to_string(i));
    } else {
      keys.push_back((K)i);
    }
  }

  for (auto _ : state) {
    state.PauseTiming();
    for (const auto &key : keys) {
      if constexpr (std::is_same_v<K, std::string>) {
        map.insert({key, ""});
      } else {
        map.insert({key, (V)0});
      }
    }
    state.ResumeTiming();

    for (const auto &key : keys) {
      map.erase(key);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_ALL(BM_Map_Delete, int, Range(8, 8 << 10));
BENCHMARK_ALL(BM_Map_Delete, std::string, Range(8, 8 << 10));

BENCHMARK_MAIN();
