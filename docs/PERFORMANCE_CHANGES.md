# Performance & Memory Optimization Changes

This document describes the optimization passes applied to improve correctness, memory usage, and performance.

## Summary of Implemented Steps (Phase 1 & 2)

1. Correctness fixes & data structure cleanup
2. Per-thread reusable Wavefront Alignment (WFA) context (eliminate per-pair allocation churn)
3. Dynamic pairwise distance work queue + pooled alignment buffers + k-mer prefilter (new)

---
## 1. Correctness & Structural Fixes (Recap)

- `savedForNewClusters` converted from `char**` misuse to `int*` (index array with -1 sentinel).
- Replaced leaking tree allocation with `allocateTreeArray` / `freeTreeArray`.
- Fixed `ops_alg` misuse of string literal.
- Added initialization for allocated node name strings.

---
## 2. Reusable Per-Thread WFA Context (Recap)

Introduced `wfa_thread_context` with reusable aligner and growing buffers to avoid per-pair allocation overhead.

---
## 3. Advanced Pairwise Distance Optimizations (New)

### a. DATA & mult Buffer Pooling
Previously each alignment allocated/free’d:
- DATA[2][alignment_length]
- mult[alignment_length]

Now each thread context keeps:
```
int** data_buf; // 2 rows
int*  mult_buf;
int   data_capacity;
```
Resized geometrically when needed, eliminating high-frequency malloc/free churn.

### b. Dynamic Work Queue
Replaced static (row-slice) partitioning with a mutex-protected dynamic queue:
```
struct dist_work_item { int i; int j_start; int j_end; };
struct dist_work_queue { dist_work_item* items; int count; int next; pthread_mutex_t lock; };
```
Each worker repeatedly fetches the next row-range task, improving load balance when sequence lengths vary. (Atomic index would be a future micro-optimization; mutex overhead negligible versus alignment cost.)

### c. K-mer Prefilter (Heuristic)
Added lightweight similarity screen (`quick_kmer_screen`) scanning every k-th window (default k=4) and rejecting pairs with more than a small mismatch budget before performing WFA. Effect: Skips expensive alignment for clearly dissimilar pairs (configurable later). Pairs skipped retain default dist (left unchanged until potentially refined—current behavior: skipped pairs not updated; follow-up may assign max distance explicitly).

### d. Integrated Changes in `createDistMat_WFA`
Now:
1. Initializes work queue over upper triangle.
2. Launches worker threads executing `dist_dynamic_worker` using pooled buffers & prefilter.
3. Joins threads and destroys queue.

### e. Memory Lifecycle Additions
- Thread-local pooled buffers persist for thread lifetime; no per-pair frees.
- Added capacity tracking to avoid repeated reallocation.

---
## Performance Impact (Expected)
| Component | Before | After | Benefit |
|-----------|--------|-------|---------|
| Per-pair malloc/free | High | Eliminated for WFA buffers & DATA/mult | Lower allocator overhead |
| Work distribution | Static slices | Dynamic queue | Better core utilization |
| Alignment filtering | None | K-mer heuristic | Fewer full WFAs on dissimilar pairs |

---
## Deferred / Next Steps
1. Parameterize / tune k-mer screen & record skipped stats; assign explicit fallback distance for skipped pairs (e.g., DISTMAX) for clarity.
2. Optional lock-free (atomic) queue index to shave mutex overhead.
3. Add instrumentation macros (timing, counters: total pairs, skipped, aligned, avg time per align).
4. Introduce configurable early termination in WFA when score exceeds distance threshold.
5. Pack distance matrix (1D / float) and optionally lazy compute on demand.
6. Provide teardown API to free thread-local contexts explicitly (for long-running processes or repeated runs).

---
## Usage Notes
Existing external APIs unchanged. Distance matrix semantics remain unless many pairs are skipped; future patch will formalize skipped pair distance assignment.

---
## Key New / Updated Symbols
- `wfa_thread_context` extended with pooled DATA/mult.
- `dist_work_item`, `dist_work_queue`, `dist_work_queue_init`, `dist_work_queue_fetch`, `dist_work_queue_destroy`.
- `quick_kmer_screen` prefilter.
- Updated `createDistMat_WFA` + `fillInMat` using pooled buffers.

---
End of document.
