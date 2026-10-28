#!/usr/bin/env python3
"""Summarize and compare JA2PROF.csv files written by -DJA2_PROFILE=ON builds.

    ja2prof_report.py run.csv                 # one run
    ja2prof_report.py baseline.csv new.csv    # compare, new vs baseline
    ja2prof_report.py run.csv --budget-ms 33  # flag frames over a budget

Zones are inclusive (blit/present are also counted inside render/video).
Only the last 1024 frames are stored per frame; the summary covers all frames.
"""

import argparse
import csv
import sys


def load(path):
    run = {"meta": {}, "summary": {}, "markers": [], "frames": [], "columns": []}
    with open(path, newline="", encoding="latin-1") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            kind = row[0]
            if kind == "meta":
                run["meta"][row[1]] = row[2] if len(row) > 2 else ""
            elif kind == "summary" and row[1] != "zone":
                run["summary"][row[1]] = {
                    "frames": int(row[2]), "calls": int(row[3]),
                    "mean": int(row[4]), "p50": int(row[5]), "p95": int(row[6]),
                    "p99": int(row[7]), "max": int(row[8]),
                }
            elif kind == "marker":
                run["markers"].append((int(row[1]), row[2]))
            elif kind == "frame" and row[1] == "index":
                run["columns"] = row[2:]
            elif kind == "frame":
                run["frames"].append([int(value) for value in row[1:]])
    return run


def ms(us):
    return f"{us / 1000:7.2f}"


def print_summary(run, name):
    meta = run["meta"]
    frame = run["summary"].get("frame")
    print(f"## {name}  label={meta.get('label')}  frames={meta.get('frames')}"
          f"  timebase={meta.get('timebase_hz')} Hz  uncapped={meta.get('uncapped')}"
          f"  min_free={meta.get('min_free_kb')} KB")
    if frame and frame["mean"]:
        print(f"   avg fps {1e6 / frame['mean']:.1f}   p95 frame {frame['p95'] / 1000:.2f} ms")
    print("   zone        frames    calls  mean_ms   p50_ms   p95_ms   p99_ms   max_ms")
    for zone, stats in run["summary"].items():
        if stats["frames"] == 0:
            continue
        print(f"   {zone:<9} {stats['frames']:8d} {stats['calls']:8d} {ms(stats['mean'])} "
              f" {ms(stats['p50'])}  {ms(stats['p95'])}  {ms(stats['p99'])}  {ms(stats['max'])}")
    if run["markers"]:
        print("   markers: " + ", ".join(f"{frame}:{label}" for frame, label in run["markers"]))


def print_budget(run, budget_ms):
    if not run["frames"]:
        return
    frame_index = run["columns"].index("frame_us") + 1
    budget_us = budget_ms * 1000
    over = [row for row in run["frames"] if row[frame_index] > budget_us]
    print(f"   {len(over)}/{len(run['frames'])} stored frames over {budget_ms} ms")
    worst = sorted(over, key=lambda row: row[frame_index], reverse=True)[:5]
    for row in worst:
        zones = sorted(((value, name) for name, value in
                        zip(run["columns"][1:], row[2:]) if name != "frame_us"),
                       reverse=True)[:3]
        detail = ", ".join(f"{name[:-3]}={value / 1000:.1f}" for value, name in zones)
        print(f"     frame {row[0]}: {row[frame_index] / 1000:.1f} ms ({detail})")


def print_compare(base, new):
    print("## comparison (new vs baseline, mean / p95)")
    for zone, stats in new["summary"].items():
        old = base["summary"].get(zone)
        if not old or (stats["frames"] == 0 and old["frames"] == 0):
            continue

        def delta(key):
            if old[key] == 0:
                return "    n/a"
            return f"{(stats[key] - old[key]) * 100 / old[key]:+6.1f}%"
        print(f"   {zone:<9} mean {ms(old['mean'])} -> {ms(stats['mean'])} {delta('mean')}"
              f"   p95 {ms(old['p95'])} -> {ms(stats['p95'])} {delta('p95')}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="+", help="one run, or baseline then new")
    parser.add_argument("--budget-ms", type=float, default=0,
                        help="list stored frames slower than this budget")
    args = parser.parse_args()
    if len(args.csv) > 2:
        parser.error("pass one or two CSV files")
    runs = [load(path) for path in args.csv]
    for path, run in zip(args.csv, runs):
        print_summary(run, path)
        if args.budget_ms:
            print_budget(run, args.budget_ms)
    if len(runs) == 2:
        print_compare(runs[0], runs[1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
