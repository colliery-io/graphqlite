#!/bin/bash
OUT=$1; shift
: > $OUT
for scale in "10000 50000" "50000 250000"; do
  set -- $scale
  for q in return1 lookup lookup_prop count match_all match_all_props filter filter_return_node hop1 hop2 hop3 hop2_nodes varlen edges_all agg pagerank pagerank_top wcc louvain dijkstra set_one; do
    timeout 600 python3 harness.py $1 $2 $q 3 >> $OUT 2>&1 || echo "{\"q\":\"$q\",\"nodes\":$1,\"error\":true}" >> $OUT
  done
done
# expensive ones at small scale only
for q in betweenness nodesim; do
  timeout 600 python3 harness.py 5000 25000 $q 1 >> $OUT 2>&1 || echo "{\"q\":\"$q\",\"nodes\":5000,\"error\":true}" >> $OUT
done
echo DONE >> $OUT
