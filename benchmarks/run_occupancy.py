#!/usr/bin/env python3
"""Sweep target occupancy, using floor(reference buckets * target) entries."""
import argparse
from decimal import Decimal
import json
import os
from pathlib import Path
import random
import subprocess
import sys

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--buckets', type=int, default=262143)
parser.add_argument('--loads', nargs='+', default=['0.75','0.90','0.95','0.99'])
parser.add_argument('--samples', type=int, default=5)
parser.add_argument('--target-ms', type=float, default=20)
parser.add_argument('--maps', nargs='+', choices=['std','absl','dense','elastic'], default=['std','absl','dense','elastic'])
parser.add_argument('--key-types', nargs='+', choices=['u64','string'], default=['u64','string'])
parser.add_argument('--hash-modes', nargs='+', choices=['native','shared'], default=['native'])
parser.add_argument('--elastic-header', type=Path)
parser.add_argument('--cpu', type=int, default=min(os.sched_getaffinity(0)))
args=parser.parse_args()
if args.buckets < 3 or args.buckets > 10000000 or args.samples < 1:
    parser.error('invalid buckets or samples')
loads=[Decimal(x) for x in args.loads]
if not all(x.is_finite() and 0 < x < 1 for x in loads) or len(loads)!=len(set(loads)):
    parser.error('loads must be unique fractions in (0, 1)')
if (args.output/'campaign.json').exists():
    parser.error('campaign already exists')
args.output.mkdir(parents=True,exist_ok=True)
manifest={'reference_buckets':args.buckets,'loads':args.loads,'order_seed':31019,
          'sampling':'preallocated, native library capacity rules; actual occupancy recorded',
          'command':sys.argv,'runs':[]}
random.Random(31019).shuffle(loads)
for load in loads:
    count=int(Decimal(args.buckets)*load)
    if count < 1: parser.error('load produces no entries')
    name='load-'+str(load).replace('.','p')
    output=args.output/name
    command=[sys.executable,str(HERE/'run.py'),str(args.binary),
             '--output',str(output),'--sizes',str(count),'--samples',str(args.samples),
             '--target-ms',str(args.target_ms),'--cpu',str(args.cpu),
             '--target-load',str(load),'--reference-buckets',str(args.buckets),
             '--maps',*args.maps,'--key-types',*args.key_types,'--hash-modes',*args.hash_modes,
             '--operations','insert_reserved','hit','miss','mixed']
    if args.elastic_header:
        command += ['--elastic-header',str(args.elastic_header)]
    print(f'Occupancy {load}: {count:,} entries, reference {args.buckets:,} buckets',flush=True)
    subprocess.run(command,check=True)
    manifest['runs'].append({'load':str(load),'entries':count,'directory':name})
    (args.output/'campaign.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(args.output/'campaign.json',flush=True)
