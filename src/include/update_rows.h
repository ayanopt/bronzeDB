#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
#include "storage.h"
#include "parser.h"
#include <iostream>

using namespace std;

inline void update_rows(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
        return;
    }

    if (args.flags.find("--set") == args.flags.end()) {
        cerr << "Error: --update requires --set '{field:value, ...}'" << endl;
        return;
    }

    // Load schema only for validation
    TableSchema schema = load_schema_only(name);

    // --use-index (optional)
    string index_name;
    auto idx_it = args.flags.find("--use-index");
    if (idx_it != args.flags.end() && !idx_it->second.empty()) {
        index_name = idx_it->second[0];
        bool found = false;
        for (const auto& idx : schema.indexes)
            if (idx.name == index_name) { found = true; break; }
        if (!found) {
            cerr << "Error: index '" << index_name << "' does not exist" << endl;
            return;
        }
    }

    // --at conditions
    vector<Condition> conditions;
    auto at_it = args.flags.find("--at");
    if (at_it != args.flags.end()) {
        string combined = join_tokens(at_it->second);
        conditions = parse_conditions(combined, schema.fields);

        if (index_name.empty()) {
            // Must be PK fields
            for (const auto& cond : conditions) {
                bool is_pk = false;
                for (const auto& pk : schema.pk_fields)
                    if (pk == cond.field) { is_pk = true; break; }
                if (!is_pk) {
                    cerr << "Error: --at field '" << cond.field << "' must be a primary key field" << endl;
                    return;
                }
            }
        } else {
            // Must be index PK fields
            for (const auto& idx : schema.indexes) {
                if (idx.name != index_name) continue;
                for (const auto& cond : conditions) {
                    bool found = false;
                    for (const auto& pk : idx.pk_fields)
                        if (pk == cond.field) { found = true; break; }
                    if (!found) {
                        cerr << "Error: --at field '" << cond.field << "' must be an index pk field" << endl;
                        return;
                    }
                }
            }
        }
    }

    // --set clause
    auto set_it = args.flags.find("--set");
    string set_combined = join_tokens(set_it->second);
    Row updates = parse_set_clause(set_combined, schema);

    if (updates.empty()) {
        cerr << "Error: --set produced no updates" << endl;
        return;
    }

    // Prevent PK updates
    for (const auto& [k, v] : updates) {
        for (const auto& pk : schema.pk_fields) {
            if (k == pk) {
                cerr << "Error: cannot update primary key field '" << k << "'" << endl;
                return;
            }
        }
    }

    // Build Table for condition matching (no rows loaded)
    Table t;
    t.schema = schema;

    // Stream update — O(N) I/O, O(1) peak memory, no full load
    size_t updated = stream_update(
        name, schema,
        [&](const Row& row) { return t.matches_conditions(row, conditions); },
        [&](Row& row) {
            for (const auto& [k, v] : updates)
                row[k] = v;
        }
    );

    cout << "Updated " << updated << " row(s) in '" << name << "'" << endl;
}
