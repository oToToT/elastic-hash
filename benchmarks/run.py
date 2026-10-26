#!/usr/bin/env python3
"""Collect serial, CPU-pinned comparison runs and retain every timed sample."""
import argparse
import csv
import datetime
import hashlib
import io
import itertools
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
import time

MAPS = ("std", "absl", "dense", "gnu_gp", "gnu_cc", "elastic")
OPERATIONS = ("insert_grow", "insert_reserved", "hit", "miss", "mixed", "iterate", "erase", "churn")
ROOT = Path(__file__).resolve().parent.parent


def summarize(rows):
    groups = {}
    for row in rows:
        key = tuple(row[x] for x in ("map", "key_type", "hash", "size", "operation"))
        groups.setdefault(key, []).append(row)
    results = []
    for key, values in sorted(groups.items()):
        item = dict(zip(("map", "key_type", "hash", "size", "operation"), key))
        times = sorted(float(x["ns_per_op"]) for x in values)
        item.update(samples=len(values), buckets=values[0]["buckets"], load_factor=values[0]["load_factor"])
        if key[-1] == "memory":
            item.update(median_ns_per_op="", p25_ns_per_op="", p75_ns_per_op="", min_ns_per_op="", max_ns_per_op="",
                        heap_bytes_per_entry=values[0]["heap_bytes_per_entry"])
        else:
            item.update(median_ns_per_op=statistics.median(times), p25_ns_per_op=times[(len(times)-1)//4],
                        p75_ns_per_op=times[3*(len(times)-1)//4], min_ns_per_op=min(times), max_ns_per_op=max(times),
                        heap_bytes_per_entry="")
        item["target_load"] = values[0].get("target_load", "0")
        item["configured_max_load"] = values[0].get("configured_max_load", "")
        results.append(item)
    reference = {(x["key_type"], x["hash"], x["size"], x["operation"]): x for x in results if x["map"] == "std"}
    for x in results:
        std = reference.get((x["key_type"], x["hash"], x["size"], x["operation"]))
        if std and x["operation"] != "memory":
            x["time_relative_to_std"] = x["median_ns_per_op"] / std["median_ns_per_op"]
        else:
            x["time_relative_to_std"] = ""
    return results


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def report(path, results, metadata):
    lookup = {(x["map"], x["key_type"], x["hash"], int(x["size"]), x["operation"]): x for x in results}
    method_link = os.path.relpath(ROOT/'benchmarks/README.md', path.parent)
    lines = ["# Hash-table comparison", "", f"Measured {metadata['started_utc']} on {metadata['cpu_model']} (CPU {metadata['cpu']}).",
             f"{metadata['binary_info'].strip().replace(chr(10), '; ')}. Release build: C++20, `-O3 -DNDEBUG`, without `-march=native`.",
             f"Each throughput entry is the median of {metadata['samples']} samples. Lower is better. Values are nanoseconds per map operation.", "",
             "The runner records all samples in [raw.csv](raw.csv), summaries and quartiles in [summary.csv](summary.csv),",
             f"and the build/workload environment in [metadata.json](metadata.json). See [methodology]({method_link}).", ""]
    operations = metadata.get('operations', list(OPERATIONS))
    if metadata.get('target_load', 0):
        lines += [f"Requested maximum load: {metadata['target_load']}; fixtures preallocated before timing.",
                  "Actual occupancies differ by library; see the last column and raw configured_max_load field.", ""]
    for mode, key_type, size in itertools.product(metadata['hash_modes'], metadata['key_types'], metadata['sizes']):
        lines += [f"## {key_type}, {size:,} entries, {mode} hashes", "",
                  "| Table | " + " | ".join(operations) + " | Heap B/key | Load |",
                  "| :--- | " + " | ".join(["---:"]*(len(operations)+2)) + " |"]
        for name in metadata['maps']:
            entries = [lookup[(name, key_type, mode, size, operation)] for operation in operations]
            memory = lookup[(name, key_type, mode, size, 'memory')]
            times = [f"{x['median_ns_per_op']:.2f}" for x in entries]
            lines.append("| " + " | ".join([name] + times + [f"{float(memory['heap_bytes_per_entry']):.1f}", f"{float(memory['load_factor']):.3f}"]) + " |")
        lines += [""]
    path.write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sizes", type=int, nargs="+", default=[4096, 45000, 180000])
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--target-ms", type=float, default=10)
    parser.add_argument("--maps", choices=MAPS, nargs="+", default=list(MAPS))
    parser.add_argument("--key-types", choices=["u64", "string"], nargs="+", default=["u64", "string"])
    parser.add_argument("--hash-modes", choices=["native", "shared"], nargs="+", default=["native", "shared"])
    parser.add_argument("--operations", choices=OPERATIONS, nargs="+", default=list(OPERATIONS))
    parser.add_argument("--elastic-header", type=Path, default=ROOT/'elastic_hash.hpp',
                        help="actual header compiled into this binary (for prototype provenance)")
    parser.add_argument("--timeout", type=float, default=900, help="seconds allowed per configuration")
    parser.add_argument("--target-load", type=float, default=0,
                        help="configure a maximum load and preallocate; zero keeps the default-growth protocol")
    parser.add_argument("--reference-buckets", type=int,
                        help="nominal capacity used to choose sizes for an occupancy sweep; not an enforced capacity")
    parser.add_argument("--cpu", type=int, default=min(os.sched_getaffinity(0)))
    args = parser.parse_args()
    if not (args.samples > 0 and all(0 < n <= 10000000 for n in args.sizes) and 0 < args.target_ms <= 1000):
        parser.error("invalid sizes, samples or target duration")
    if not math.isfinite(args.target_load) or not 0 <= args.target_load < 1:
        parser.error("--target-load must be zero or in (0, 1)")
    if args.target_load and "insert_grow" in args.operations:
        parser.error("--target-load preallocates: select insert_reserved instead of insert_grow")
    if args.target_load and any(x.startswith('gnu_') for x in args.maps):
        parser.error("fixed occupancy is supported for std, absl, dense and elastic")
    if len(args.operations) != len(set(args.operations)):
        parser.error("operations must be unique")
    binary = args.binary.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    for name in ("raw.csv", "summary.csv", "metadata.json", "RESULTS.md"):
        if (args.output/name).exists():
            parser.error(f"output already exists: {args.output/name}")
    os.sched_setaffinity(0, {args.cpu})
    model = next((line.split(":", 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                  if line.startswith("model name")), "unknown")
    metadata = dict(started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), cpu=args.cpu, cpu_model=model,
                    platform=platform.platform(), libc=platform.libc_ver(), python=platform.python_version(),
                    binary_info=subprocess.check_output([str(binary), "--info"], text=True),
                    binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    elastic_header_sha256=hashlib.sha256(args.elastic_header.read_bytes()).hexdigest(),
                    harness_sha256=hashlib.sha256((ROOT/'benchmarks/comparison.cc').read_bytes()).hexdigest(),
                    git_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    elastic_header_path=str(args.elastic_header.resolve()), operations=args.operations,
                    target_load=args.target_load, reference_buckets=args.reference_buckets,
                    samples=args.samples, target_ms=args.target_ms, sizes=args.sizes, maps=args.maps,
                    key_types=args.key_types, hash_modes=args.hash_modes,
                    dataset_seed="0x519b6f32d4817a09", process_order_seed=31019,
                    dependency_versions=dict(abseil="20260817.0", unordered_dense="4.8.1"), command=sys.argv)
    cache = binary.parent/'CMakeCache.txt'
    metadata['cmake_flags'] = [line for line in cache.read_text().splitlines()
                              if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS:','CMAKE_CXX_FLAGS_RELEASE:'))] if cache.exists() else []
    (args.output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    cases = list(itertools.product(args.maps, args.key_types, args.hash_modes, args.sizes))
    random.Random(31019).shuffle(cases)
    collected = []
    for number, (name, key, mode, size) in enumerate(cases, 1):
        start = time.monotonic()
        command = [str(binary), name, key, mode, str(size), str(args.samples), str(args.target_ms), ",".join(args.operations), str(args.target_load)]
        result = subprocess.run(command, text=True, capture_output=True, timeout=args.timeout, check=True)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        if len(rows) != 1 + len(args.operations)*args.samples:
            raise ValueError(f"unexpected row count: {command}")
        for operation in ("memory",) + tuple(args.operations):
            samples = [x for x in rows if x['operation'] == operation]
            expected = 1 if operation == 'memory' else args.samples
            if len(samples) != expected or {int(x['sample']) for x in samples} != set(range(expected)):
                raise ValueError(f"unexpected samples: {command} {operation}")
            if operation != 'memory' and any(float(x['ns_per_op']) <= 0 for x in samples):
                raise ValueError(f"nonpositive duration: {command} {operation}")
        collected.extend(rows)
        write_csv(args.output/'raw.csv', collected)
        print(f"[{number}/{len(cases)}] {name:7} {key:6} {mode:6} {size:7}: {time.monotonic()-start:.1f}s", flush=True)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != metadata['binary_sha256']:
        raise RuntimeError('benchmark binary changed during collection')
    if hashlib.sha256(args.elastic_header.read_bytes()).hexdigest() != metadata['elastic_header_sha256']:
        raise RuntimeError('elastic header changed during collection')
    summaries = summarize(collected)
    write_csv(args.output/'summary.csv', summaries)
    metadata['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    (args.output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    report(args.output/'RESULTS.md', summaries, metadata)
    print(f"Report: {args.output/'RESULTS.md'}", flush=True)


if __name__ == '__main__':
    main()
