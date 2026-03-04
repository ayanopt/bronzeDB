# BronzeDB Implementation Outcome

## Build

Clean compile, zero warnings:
```
g++ -Wall -std=c++17 -I../include -c ../main.cpp
g++ -Wall -std=c++17 -I../include -o bronzedb main.o
```

## Manual Tests (all passed)

All 6 commands verified with the `monkeys` table from the README:
- `CREATE` — creates `./data/<table>.bdb` with schema + indexes
- `INSERT` — appends JSON row to file
- `QUERY` — equality, range (`>`, `<`, `>=`, `<=`), `--use-index`, `--output-fields`
- `UPDATE` — updates rows matching `--at` condition using `--set '{field:value}'`
- `EDIT` — `--add-fields`, `--remove-fields`, `--delete-index`, `--add-index`
- `DELETE` — removes `.bdb` file

## Stress Test Results (10,000 rows, monkeys table)

| Operation | Time |
|-----------|------|
| Insert 10,000 rows | 561,959ms (~9.4 min) |
| Full scan query (10k rows) | 86ms |
| PK equality query | 75ms |
| Range query on sort key | 78ms |
| Index query | 72ms |
| Update 100 rows | 11,094ms |

Row count verification: 100 updated rows confirmed correct.

## Performance Notes

**Queries are fast** (72–86ms for 10k rows): the entire table is loaded into memory as `std::vector<Row>` and scanned linearly. For read-heavy workloads this is efficient.

**Inserts and updates are slow** due to the load-entire-file → mutate → save-entire-file pattern. Each of the 10k inserts reads and rewrites an O(N)-sized file, making total insert time O(N²) in I/O. At 10k rows:
- Avg file size grows from 0 → ~800KB
- Each insert rewrites the full file

**Remediation paths** (not implemented — out of scope):
- Append-only writes for INSERT (skip the load step)
- Batch inserts
- B-tree or LSM-tree indexing for O(log N) lookups

## Design Decisions Made

1. **`--set` flag added to UPDATE** — the original spec had no way to specify what to update. Added `--set '{field:value, ...}'` to `VALID_FLAGS[UPDATE]` and to `validate_args`.

2. **File format** — human-readable `.bdb` text format with a `SCHEMA` header section and `DATA` JSON lines. Easy to inspect and debug.

3. **No external dependencies** — pure C++17 STL (`<variant>`, `<filesystem>`, `<fstream>`).

4. **`field.h` left in place** — the stub file is unused; new types live in `types.h`. Removing it was avoided to not break potential future references.
