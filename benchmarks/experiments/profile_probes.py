#!/usr/bin/env python3
"""Count Elastic's bucket probes on the benchmark's exact integer-key dataset."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sizes', nargs='+', type=int, default=[1000000,3000000])
parser.add_argument('--cpu', type=int, default=min(os.sched_getaffinity(0)))
args = parser.parse_args()
os.sched_setaffinity(0, {args.cpu})
root = Path(__file__).resolve().parents[2]
source = (root/'elastic_hash.hpp').read_text()
marker = 'namespace elastic_hash_detail {\n'
assert source.count(marker) == 1
source = source.replace(marker, marker+'inline thread_local std::uint64_t debug_probes = 0;\n')
marker = '    const auto &level = levels_[i];\n    // Keep the paper-style random prefix.'
assert source.count(marker) == 1
source = source.replace(marker, '    ++elastic_hash_detail::debug_probes;\n'+marker)
with tempfile.TemporaryDirectory(prefix='elastic-probe-count-') as folder:
    folder = Path(folder)
    (folder/'elastic_hash.hpp').write_text(source)
    (folder/'profile.cc').write_text((root/'benchmarks/experiments/probe_profile.cc').read_text())
    subprocess.run([os.environ.get('CXX','clang++'), '-std=c++20','-O2', str(folder/'profile.cc'), '-o',str(folder/'profile')],check=True)
    for i,n in enumerate(args.sizes):
        lines = subprocess.check_output([str(folder/'profile'),str(n)],text=True).splitlines()
        print('\n'.join(lines if i == 0 else lines[1:]),flush=True)
