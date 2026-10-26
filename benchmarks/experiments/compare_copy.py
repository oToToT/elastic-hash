#!/usr/bin/env python3
"""Alternate optimized/baseline binaries on an identical reserved-insert workload."""
import argparse
import csv
from decimal import Decimal
import hashlib
import io
import json
import os
from pathlib import Path
import random
import resource
import statistics
import subprocess
import sys
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('optimized',type=Path)
parser.add_argument('baseline',type=Path)
parser.add_argument('--baseline-header',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--buckets',type=int,default=262143)
parser.add_argument('--load',default='0.75')
parser.add_argument('--pairs',type=int,default=5)
parser.add_argument('--samples',type=int,default=5)
parser.add_argument('--cpu',type=int,default=min(os.sched_getaffinity(0)))
args=parser.parse_args()
if args.pairs < 1 or args.samples < 1 or not Decimal(0)<Decimal(args.load)<Decimal(1):parser.error('invalid settings')
args.output.mkdir(parents=True,exist_ok=True)
if (args.output/'metadata.json').exists():parser.error('output exists')
os.sched_setaffinity(0,{args.cpu})
root=Path(__file__).resolve().parents[2]
binaries={'optimized':args.optimized.resolve(),'baseline':args.baseline.resolve()}
headers={'optimized':root/'elastic_hash.hpp','baseline':args.baseline_header.resolve()}
metadata={'command':sys.argv,'cpu':args.cpu,'pairs':args.pairs,'samples':args.samples,'reference_buckets':args.buckets,
          'load':args.load,'entries':int(Decimal(args.buckets)*Decimal(args.load)),
          'binary_sha256':{k:hashlib.sha256(v.read_bytes()).hexdigest() for k,v in binaries.items()},
          'header_sha256':{k:hashlib.sha256(v.read_bytes()).hexdigest() for k,v in headers.items()}}
(args.output/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
rows=[];paired=[]
start_order=random.Random(31019).choice([['optimized','baseline'],['baseline','optimized']])
for pair in range(args.pairs):
    order=start_order if pair%2 == 0 else list(reversed(start_order))
    medians={}
    for variant in order:
        command=[str(binaries[variant]),'elastic','string','native',str(metadata['entries']),str(args.samples),
                 '20','insert_reserved',args.load]
        cpu_before=resource.getrusage(resource.RUSAGE_CHILDREN)
        start=time.monotonic()
        result=subprocess.run(command,check=True,text=True,capture_output=True,timeout=300)
        elapsed=time.monotonic()-start
        cpu_after=resource.getrusage(resource.RUSAGE_CHILDREN)
        cpu=(cpu_after.ru_utime+cpu_after.ru_stime)-(cpu_before.ru_utime+cpu_before.ru_stime)
        samples=[x for x in csv.DictReader(io.StringIO(result.stdout)) if x['operation']=='insert_reserved']
        if len(samples)!=args.samples:raise RuntimeError('wrong sample count')
        medians[variant]=statistics.median(float(x['ns_per_op']) for x in samples)
        for x in samples:rows.append(dict(pair=pair,variant=variant,process_wall_seconds=elapsed,process_cpu_seconds=cpu,**x))
        print(f'{pair+1}/{args.pairs} {variant}: {medians[variant]:.1f} ns, process CPU/wall {cpu/elapsed:.2f}',flush=True)
    paired.append(dict(pair=pair,optimized_ns=medians['optimized'],baseline_ns=medians['baseline'],
                       optimized_over_baseline=medians['optimized']/medians['baseline']))
    for name,data in [('raw.csv',rows),('pairs.csv',paired)]:
        with (args.output/name).open('w',newline='') as stream:
            writer=csv.DictWriter(stream,fieldnames=list(data[0]),lineterminator="\n");writer.writeheader();writer.writerows(data)
for k,path in binaries.items():
    if hashlib.sha256(path.read_bytes()).hexdigest()!=metadata['binary_sha256'][k]:raise RuntimeError('binary changed')
for k,path in headers.items():
    if hashlib.sha256(path.read_bytes()).hexdigest()!=metadata['header_sha256'][k]:raise RuntimeError('header changed')
ratio=statistics.median(x['optimized_over_baseline'] for x in paired)
metadata['median_paired_ratio']=ratio
metadata['median_paired_time_reduction_percent']=100*(1-ratio)
(args.output/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
print(f'median paired optimized/baseline ratio: {ratio:.4f}',flush=True)
