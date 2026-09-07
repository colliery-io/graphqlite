# Python performance harness

Companion scripts for the performance review tracked in Metis as GQLITE-I-0051. They load the
built extension into Python's `sqlite3` module (no `sqlite3` CLI needed),
build a synthetic graph with typed properties, and time individual Cypher
query shapes in isolated subprocesses so peak memory can be attributed.

```
make extension RELEASE=1
cd tests/performance/python
python3 harness.py 10000 50000 match_all 3     # nodes edges query reps
python3 bench_write.py 10000 1000              # write path, us/op
python3 bench_alt.py 20000                     # generated SQL vs alternative SQL shapes
./sweep.sh out.jsonl                           # full sweep at 10K and 50K nodes
```

Set `GQL_EXT=/path/to/graphqlite.so` to point at a different build
(for example one compiled with `-g` for callgrind). The default picks the
platform's library name (`.so`, `.dylib`, `.dll`). On macOS there is no
`/proc`, so the memory columns fall back to `ru_maxrss` (peak RSS only; the
`rss_delta_kb` column is then peak-to-peak rather than current).
