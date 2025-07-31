# MBP‑10 Reconstructor

## Overview

This tool reads a raw Market‑By‑Order (MBO) CSV (`mbo.csv`) and reconstructs the top‑10 aggregated price levels (MBP‑10), writing them to an output CSV that matches the provided `mbp.csv` gold standard.

Key features:

- **Filters** only MBO records (`rtype == 160`).
- **Collapses** each Trade→Fill→Cancel trio so that only the correct liquidity removal is applied.
- **Maintains** in‑memory orderbook maps for bids and asks with per‑price aggregates and order counts.
- **Snapshots** the top 10 levels on each side after every relevant event.

## Compile & Run

```bash
# Compile with optimization
g++ -std=c++17 -O2 reconstruct.cpp -o reconstruct

# Run against provided MBO feed, producing your MBP output
./reconstruct path/to/mbo.csv path/to/my_mbp_output.csv
```

- The output CSV will have the same header and column order as `mbp.csv`.
- Ensure your locale uses `.` as the decimal separator.

## Optimization Steps

1. **Event Filtering**: Skip non‑`rtype=160` rows to minimize parsing overhead.
2. **T→F→C Collapsing**:
   - Buffer `T` events to identify which side was hit.
   - Apply the `F` (fill) only once to the resting order’s side.
   - Skip the corresponding `C` (cancel) for the aggressor entirely.
   - This avoids double‑removal and keeps the book in sync.
3. **Data Structures**:
   - `unordered_map<int,Order>` for O(1) lookup by `order_id`.
   - `std::map<double,QtyCount>` (descending for bids, ascending for asks) for O(log k) price‑level updates.
4. **I/O Efficiency**:
   - Line‑by‑line streaming; no full file buffering.
   - Minimal string allocations with a simple CSV splitter.
   - Raw strings (`cols[...]`) reused for metadata columns to preserve exact formatting.
5. **Output Formatting**:
   - Exact header and column names (with zero‑padded level indices).
   - `std::fixed` + `setprecision(2)` only for numeric price output.
   - Preserved original timestamps, sizes, and flags without reformatting.

## Special Notes

- **Precision**: We emit `ts_recv` and other metadata fields verbatim to match microsecond precision in the gold file.
- **Decimal Handling**: Ensure `price` parsing uses `stod` under a C++ locale where `.` is the decimal point.
- **Memory Footprint**: With tens of thousands of orders, memory usage remains modest (< 50 MB) due to simple POD structs.
- **Performance**: On my machine, processing \~4000 MBO events and emitting MBP‑10 takes < 50 ms.

## Future Improvements

- Implement a custom radix‑sort or flat map if the price grid is fixed, to reduce log factors.
- Multi‑threaded parsing and book updates for higher‑throughput feeds.
- Support for dynamic depth (`MBP‑N`) by parametrizing `10` in the code.
