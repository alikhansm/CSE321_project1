# CSE321 Project #1 — B-tree, B\*-tree, B+-tree

This repository contains a from-scratch C++ implementation of three index
structures (B-tree, B\*-tree, B+-tree) and the experiment driver that produces
all numbers reported in the project paper.

---

## 1. Environment

| Item | Version used | Notes |
| --- | --- | --- |
| Language | **C++17** | only the standard library is used |
| Compiler | **g++ 13.3.0** | any g++ ≥ 9 / clang++ ≥ 10 with `-std=c++17` works |
| Build tool | **GNU Make** | a single `Makefile` is provided |
| OS | Linux (Ubuntu 24.04) | also fine on macOS; on Windows use WSL or MSYS2 |

No external libraries, no `boost`, no STL tree containers. Only `<vector>`,
`<random>`, `<chrono>`, `<fstream>`, etc.

---

## 2. Directory layout

```
btree_project/
├── Makefile
├── README.md                  <- you are here
├── src/
│   ├── common.h               StudentRecord, Key, RID typedefs
│   ├── btree.{h,cpp}          B-tree (CLRS-style "split as descend")
│   ├── bstartree.{h,cpp}      B*-tree (redistribute → 2-to-3 split)
│   ├── bplustree.{h,cpp}      B+-tree (linked leaves, RIDs in leaves only)
│   ├── gen_data.cpp           dataset generator (100 000 student records)
│   └── main.cpp               experiment driver (4 workloads + integrity)
├── data/
│   └── students.csv           generated dataset (100 000 records)
└── results/
    ├── insertion_results.csv
    ├── search_results.csv
    ├── range_results.csv
    └── deletion_results.csv
```

---

## 3. Reproducing all results

From the project root:

```bash
make            # builds gen_data and run_experiments
# Place your students.csv at data/students.csv, then:
make run        # runs all four experiments, writes results/*.csv
```

`make run` prints a formatted summary to stdout and writes one CSV per
experiment under `results/`. The whole pipeline takes well under a minute on
a modern laptop.

### Expected CSV format

The experiment driver expects `data/students.csv` to look like:

```
Student ID,Name,Gender,GPA,Height,Weight
202038411,Pamela Kwak,Female,3.36,160.3,55.2,
...
```

Specifically:

* The first line is treated as a header and skipped — the exact column names
  do not matter.
* Each subsequent line has six comma-separated fields (a trailing comma after
  `Weight` is tolerated).
* `Gender` may be `M`/`F` or `Male`/`Female` — the parser only inspects the
  first character.
* `Student ID` is parsed as an integer (the manual specifies 9-digit IDs of
  the form `2020xxxxx`–`2026xxxxx`).

### Generating a synthetic dataset (optional)

If you do not already have a CSV and want one, run:

```bash
make gendata    # writes data/students.csv (100 000 records, fixed seed=42)
```

This produces a file in exactly the format above. **Skip this step if you
already have your own `data/students.csv`** — `make gendata` will overwrite
it.

To wipe binaries: `make clean`. To wipe binaries **and** data/results:
`make distclean` (this also deletes `data/students.csv`, so back it up first
if you supplied your own).

---

## 4. Manual usage of the binaries

If you prefer to invoke the binaries directly:

```bash
# 1. build
g++ -std=c++17 -O2 -Wall -o gen_data        src/gen_data.cpp
g++ -std=c++17 -O2 -Wall -o run_experiments \
        src/main.cpp src/btree.cpp src/bstartree.cpp src/bplustree.cpp

# 2. generate dataset:  ./gen_data <out.csv> [count=100000] [seed=42]
mkdir -p data results
./gen_data data/students.csv 100000 42

# 3. run experiments:   ./run_experiments <data.csv> <results_dir>
./run_experiments data/students.csv results
```

The seed is fixed (default `42`) so the dataset, the 10 000 search keys, and
the 2 000 deletion keys are all deterministic and re-runs are reproducible.

---

## 5. What the four experiments measure

All four are required by the project manual (Section 3.1).

1. **Insertion & parameter tuning** — for `d ∈ {3, 5, 10}` build each tree from
   scratch by inserting all 100 000 records. Reports total time, leaf+internal
   utilisation, split count, redistribute count (B\*-tree only), node count,
   and height.

2. **Point search** — 10 000 random keys drawn from the dataset are looked up
   in each tree (built with `d = 10`). Reports total time and mean
   microseconds per query. Each lookup also validates that the returned RID
   matches the true array index.

3. **Range query** — *"average GPA and height of male students with IDs in
   [202000000, 202100000]"* (year-2020 cohort). The B-tree and B\*-tree probe
   every ID in the range; the B+-tree uses its leaf linked list via
   `rangeQuery()`. All three return identical aggregates — this is the
   cross-implementation correctness check.

   *Note on the range bounds.* The manual gives the example range
   `[20200000, 20210000]`, but actual student IDs are 9-digit values
   (`2020xxxxx`–`2026xxxxx`), so that literal range matches no records. We use
   the semantically equivalent 9-digit range `[202000000, 202100000]` that
   covers the full year-2020 cohort, which is what the example clearly
   intends. The chosen range is hard-coded in `src/main.cpp` and easy to
   change if needed.

4. **Deletion** — remove 2 000 random records from each tree built with
   `d = 10`. Reports time, post-deletion utilisation, and node count.
   A separate **post-deletion integrity stage** then re-issues a search for
   every kept and every deleted key on each tree: the expected
   `found = 98 000, deleted-confirmed = 2 000` is verified for all three
   trees. If any tree reports anything else the program will print a clear
   mismatch line.

---

## 6. Output CSV schemas

| File | Columns |
| --- | --- |
| `insertion_results.csv` | `d, tree, time_sec, utilization, splits, redistributes, nodes, height` |
| `search_results.csv`    | `tree, d, total_time_sec, mean_us_per_query` |
| `range_results.csv`     | `tree, d, time_sec, male_count, avg_gpa, avg_height` |
| `deletion_results.csv`  | `tree, d, time_sec, deleted, util_after, nodes_after` |

These are the inputs to the figures and tables in the report.

---

## 7. Implementation notes

* **Order convention.** Order `d` means each non-root node holds at least `d`
  keys and at most `2d` keys (so `d = 3 → 6 max keys`, `d = 10 → 20 max keys`).
  This matches CLRS and is consistent across all three trees.
* **B-tree split policy.** CLRS-style "split as you descend": before recursing
  into a full child, split it. Internal and leaf nodes are split at the
  median.
* **B\*-tree split policy.** On overflow, attempt redistribution with the
  left sibling, then the right sibling. If both siblings are also full, do a
  2-to-3 split (left sibling + overflowing node ⇒ three nodes, two
  separators promoted). This is what produces the much higher utilisation
  visible in `insertion_results.csv`.
* **B+-tree split policy.** Internal nodes hold *only* keys + child pointers;
  RIDs live exclusively in leaves. On a leaf split the boundary key is
  *copied* up; on an internal split the median is *promoted* (removed from
  both halves). Leaves are linked via a `next` pointer so range scans are a
  single sweep through the leaf list.
* **Deletion.** All three trees use CLRS-style deletion: replace with
  predecessor/successor for internal-node keys, and proactively keep
  children at ≥`d` keys while descending (borrow from a sibling when
  possible, otherwise merge). The B+-tree variant additionally maintains the
  leaf linked list across merges and refreshes parent separators after
  borrows.

A more detailed write-up of the data structures, split/merge policies, and
experimental analysis is in the project report PDF.

---

## 8. Troubleshooting

* **"`shuffle` is not a member of `std`"** — your compiler is older than C++17;
  install g++ ≥ 9 or pass `-std=c++17` explicitly.
* **"`data/students.csv` not found"** when running `make run` — run
  `make gendata` first.
* **Different numbers from those reported** — absolute timings depend on the
  machine, but the *trends* (B\*-tree highest utilisation, B+-tree fastest
  range query, all three identical correctness) should reproduce exactly
  because the dataset and query keys are seeded.
