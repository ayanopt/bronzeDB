#!/usr/bin/env bash
set -e
BINARY="./src/build/bronzedb"
cd "$(dirname "$0")"

N=${1:-10000}      # default 10k, pass e.g. 100000 for 100k
UPDATE_N=${2:-100} # rows to update

echo "=== BronzeDB Stress Test (N=$N rows, UPDATE_N=$UPDATE_N) ==="
echo ""

# ── Build ──────────────────────────────────────────────────────────────────
echo "[1] Building..."
cd src/build && make clean && make 2>&1 | tail -2
cd ../..
echo "Build: OK"
echo ""

# ── Clean state ────────────────────────────────────────────────────────────
rm -rf src/build/data

# ── CREATE ─────────────────────────────────────────────────────────────────
echo "[2] Creating 'monkeys' table..."
"$BINARY" --create monkeys \
    --fields '{monkey_id:string,species:string,age:int,fur_color:string,is_rabid:int,tail_length:double,metadata:string}' \
    --primary-key '{monkey_id}' \
    --sort-key age \
    --add-index rabid_species \
    --add-index-pk rabid_species '{species,is_rabid}' \
    --add-index-sk rabid_species '{tail_length}' \
    --add-index-projection rabid_species '{monkey_id:string}'
echo ""

# ── INSERT $N rows ─────────────────────────────────────────────────────────
echo "[3] Inserting $N rows..."
START=$(date +%s%N)

for i in $(seq 1 "$N"); do
    SPECIES_NUM=$((i % 5))
    case $SPECIES_NUM in
        0) SPECIES="lemur" ;;
        1) SPECIES="macaque" ;;
        2) SPECIES="baboon" ;;
        3) SPECIES="gorilla" ;;
        4) SPECIES="chimpanzee" ;;
    esac
    AGE=$((i % 20 + 1))
    TAIL=$(echo "scale=1; $i % 100 / 10.0 + 1" | bc)
    RABID=$((i % 2))
    "$BINARY" --insert monkeys \
        --data "{\"monkey_id\":\"monkey$i\",\"species\":\"$SPECIES\",\"age\":$AGE,\"fur_color\":\"grey\",\"is_rabid\":$RABID,\"tail_length\":$TAIL,\"metadata\":\"row$i\"}"
done

END=$(date +%s%N)
INSERT_MS=$(( (END - START) / 1000000 ))
INSERT_PER_S=$(( N * 1000 / (INSERT_MS + 1) ))
FILE_KB=$(du -k src/build/data/monkeys.bdb | awk '{print $1}')
echo "Insert $N rows:  ${INSERT_MS}ms  (~${INSERT_PER_S} rows/s)  file: ${FILE_KB}KB"
echo ""

# ── Full table scan ────────────────────────────────────────────────────────
echo "[4] Full table scan (age > 0)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --query-condition '{age:>:0}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
SCAN_MS=$(( (END - START) / 1000000 ))
SCAN_PER_S=$(( COUNT * 1000 / (SCAN_MS + 1) ))
echo "Full scan ($COUNT rows): ${SCAN_MS}ms  (~${SCAN_PER_S} rows/s)"
echo ""

# ── PK equality lookup ─────────────────────────────────────────────────────
echo "[5] PK equality lookup (monkey_id=monkey$((N/2)))..."
START=$(date +%s%N)
"$BINARY" --query monkeys --query-condition "{monkey_id:=:monkey$((N/2))}" > /dev/null
END=$(date +%s%N)
EQ_MS=$(( (END - START) / 1000000 ))
echo "PK equality: ${EQ_MS}ms"
echo ""

# ── Range query (sort key) ─────────────────────────────────────────────────
echo "[6] Range query on sort key (age >= 15)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --query-condition '{age:>=:15}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
RANGE_MS=$(( (END - START) / 1000000 ))
RANGE_PER_S=$(( (COUNT + 1) * 1000 / (RANGE_MS + 1) ))
echo "Range (${COUNT} rows): ${RANGE_MS}ms  (~${RANGE_PER_S} rows/s)"
echo ""

# ── Index query ────────────────────────────────────────────────────────────
echo "[7] Index query (species=lemur, is_rabid=1)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --use-index rabid_species \
    --query-condition '{species:=:lemur,is_rabid:=:1}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
IDX_MS=$(( (END - START) / 1000000 ))
IDX_PER_S=$(( (COUNT + 1) * 1000 / (IDX_MS + 1) ))
echo "Index query (${COUNT} rows): ${IDX_MS}ms  (~${IDX_PER_S} rows/s)"
echo ""

# ── Compound conditions ────────────────────────────────────────────────────
echo "[8] Compound query (age >= 10 AND age <= 15)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys \
    --query-condition '{age:>=:10}' \
    --query-condition '{age:<=:15}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
COMPOUND_MS=$(( (END - START) / 1000000 ))
echo "Compound query (${COUNT} rows): ${COMPOUND_MS}ms"
echo ""

# ── Output-fields projection ───────────────────────────────────────────────
echo "[9] Projection query (--output-fields monkey_id,species)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys \
    --query-condition '{age:>:0}' \
    --output-fields '{monkey_id,species}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
PROJ_MS=$(( (END - START) / 1000000 ))
PROJ_PER_S=$(( COUNT * 1000 / (PROJ_MS + 1) ))
echo "Projection ($COUNT rows): ${PROJ_MS}ms  (~${PROJ_PER_S} rows/s)"
echo ""

# ── UPDATE $UPDATE_N rows ──────────────────────────────────────────────────
echo "[10] Updating $UPDATE_N rows (fur_color=black)..."
START=$(date +%s%N)
for i in $(seq 1 "$UPDATE_N"); do
    "$BINARY" --update monkeys --at "{monkey_id:=:monkey$i}" --set '{fur_color:black}'
done
END=$(date +%s%N)
UPDATE_MS=$(( (END - START) / 1000000 ))
UPDATE_PER_S=$(( UPDATE_N * 1000 / (UPDATE_MS + 1) ))
echo "Update $UPDATE_N rows: ${UPDATE_MS}ms  (~${UPDATE_PER_S} rows/s)"
echo ""

# ── Verify updates ─────────────────────────────────────────────────────────
echo "[11] Verifying updates..."
UPDATED=$("$BINARY" --query monkeys --query-condition '{fur_color:=:black}' \
    | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
if [ "$UPDATED" -eq "$UPDATE_N" ]; then
    echo "Verification: OK ($UPDATED rows updated, expected $UPDATE_N)"
else
    echo "ERROR: expected $UPDATE_N rows, got $UPDATED"
    exit 1
fi
echo ""

# ── EDIT: add field ────────────────────────────────────────────────────────
echo "[12] Edit table (add sex:string field)..."
"$BINARY" --edit monkeys --add-fields '{sex:string}'
echo "Edit: OK"
echo ""

# ── DELETE table ───────────────────────────────────────────────────────────
echo "[13] Deleting table..."
"$BINARY" --delete monkeys
if [ ! -f src/build/data/monkeys.bdb ]; then
    echo "Delete: OK"
else
    echo "ERROR: table file still exists"
    exit 1
fi
echo ""

# ── Summary ────────────────────────────────────────────────────────────────
echo "=== Timing Summary (N=$N rows) ==="
printf "%-30s %8s  %15s\n" "Operation" "Time(ms)" "Throughput"
printf "%-30s %8s  %15s\n" "---------" "--------" "----------"
printf "%-30s %8d  %12d r/s\n" "Insert $N rows"       "$INSERT_MS"   "$INSERT_PER_S"
printf "%-30s %8d  %12d r/s\n" "Full scan"             "$SCAN_MS"     "$SCAN_PER_S"
printf "%-30s %8d  %15s\n"    "PK equality lookup"    "$EQ_MS"       "-"
printf "%-30s %8d  %12d r/s\n" "Range query (sk)"      "$RANGE_MS"    "$RANGE_PER_S"
printf "%-30s %8d  %12d r/s\n" "Index query"           "$IDX_MS"      "$IDX_PER_S"
printf "%-30s %8d  %15s\n"    "Compound query"        "$COMPOUND_MS"  "-"
printf "%-30s %8d  %12d r/s\n" "Projection query"      "$PROJ_MS"     "$PROJ_PER_S"
printf "%-30s %8d  %12d r/s\n" "Update $UPDATE_N rows" "$UPDATE_MS"   "$UPDATE_PER_S"
echo ""
echo "=== All tests passed ==="
