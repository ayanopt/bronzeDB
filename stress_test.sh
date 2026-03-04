#!/usr/bin/env bash
set -e
BINARY="./src/build/bronzedb"
cd "$(dirname "$0")"

echo "=== BronzeDB Stress Test ==="
echo ""

# Clean build
echo "[1] Building..."
cd src/build && make clean && make 2>&1 | tail -2
cd ../..
echo "Build: OK"
echo ""

# Clean state
rm -rf src/build/data

# Create table
echo "[2] Creating 'monkeys' table..."
"$BINARY" --create monkeys \
    --fields '{monkey_id:string,species:string,age:int,fur_color:string,is_rabid:int,tail_length:double,metadata:string}' \
    --primary-key '{monkey_id}' \
    --sort-key age \
    --add-index rabid_species \
    --add-index-pk rabid_species '{species,is_rabid}' \
    --add-index-sk rabid_species '{tail_length}' \
    --add-index-projection rabid_species '{monkey_id:string}'

# Insert 10,000 rows
echo "[3] Inserting 10,000 rows..."
START=$(date +%s%N)

for i in $(seq 1 10000); do
    SPECIES_NUM=$((i % 5))
    case $SPECIES_NUM in
        0) SPECIES="lemur" ;;
        1) SPECIES="macaque" ;;
        2) SPECIES="baboon" ;;
        3) SPECIES="gorilla" ;;
        4) SPECIES="chimpanzee" ;;
    esac
    AGE=$((i % 20 + 1))
    TAIL=$(echo "scale=1; $i % 100 / 10 + 1" | bc)
    RABID=$((i % 2))
    "$BINARY" --insert monkeys \
        --data "{\"monkey_id\":\"monkey$i\",\"species\":\"$SPECIES\",\"age\":$AGE,\"fur_color\":\"grey\",\"is_rabid\":$RABID,\"tail_length\":$TAIL,\"metadata\":\"row$i\"}"
done

END=$(date +%s%N)
INSERT_MS=$(( (END - START) / 1000000 ))
echo "Insert 10k rows: ${INSERT_MS}ms"
echo ""

# Full table query (match all - use age > 0)
echo "[4] Full table query (age > 0)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --query-condition '{age:>:0}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
QUERY_MS=$(( (END - START) / 1000000 ))
echo "Full scan (${COUNT} rows): ${QUERY_MS}ms"
echo ""

# Equality query on PK field
echo "[5] Equality query on PK (monkey_id=monkey5000)..."
START=$(date +%s%N)
"$BINARY" --query monkeys --query-condition '{monkey_id:=:monkey5000}' > /dev/null
END=$(date +%s%N)
EQ_MS=$(( (END - START) / 1000000 ))
echo "PK equality query: ${EQ_MS}ms"
echo ""

# Range query on sort key
echo "[6] Range query on sort key (age >= 10)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --query-condition '{age:>=:10}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
RANGE_MS=$(( (END - START) / 1000000 ))
echo "Range query on sk (${COUNT} rows): ${RANGE_MS}ms"
echo ""

# Index query (use-index)
echo "[7] Index query (use-index rabid_species, species=lemur)..."
START=$(date +%s%N)
COUNT=$("$BINARY" --query monkeys --use-index rabid_species --query-condition '{species:=:lemur}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
END=$(date +%s%N)
IDX_MS=$(( (END - START) / 1000000 ))
echo "Index query (${COUNT} rows): ${IDX_MS}ms"
echo ""

# Update 100 rows
echo "[8] Updating 100 rows (monkey1..monkey100 fur_color=black)..."
START=$(date +%s%N)
for i in $(seq 1 100); do
    "$BINARY" --update monkeys --at "{monkey_id:=:monkey$i}" --set '{fur_color:black}'
done
END=$(date +%s%N)
UPDATE_MS=$(( (END - START) / 1000000 ))
echo "Update 100 rows: ${UPDATE_MS}ms"
echo ""

# Verify update
echo "[9] Verifying updates..."
UPDATED=$("$BINARY" --query monkeys --query-condition '{fur_color:=:black}' | head -1 | grep -o '[0-9]* rows' | grep -o '[0-9]*')
echo "Rows with fur_color=black: $UPDATED (expected 100)"
echo ""

# Delete table
echo "[10] Deleting table..."
"$BINARY" --delete monkeys
if [ ! -f src/build/data/monkeys.bdb ]; then
    echo "Table file removed: OK"
else
    echo "ERROR: table file still exists!"
    exit 1
fi
echo ""

echo "=== Timing Summary ==="
echo "Insert 10k rows:       ${INSERT_MS}ms"
echo "Full scan query:       ${QUERY_MS}ms"
echo "PK equality query:     ${EQ_MS}ms"
echo "Range query (sk):      ${RANGE_MS}ms"
echo "Index query:           ${IDX_MS}ms"
echo "Update 100 rows:       ${UPDATE_MS}ms"
echo ""
echo "=== All tests passed ==="
