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

    Table t = load_table(name);

    // --use-index (optional)
    string index_name;
    auto idx_it = args.flags.find("--use-index");
    if (idx_it != args.flags.end() && !idx_it->second.empty()) {
        index_name = idx_it->second[0];
        if (!t.find_index(index_name)) {
            cerr << "Error: index '" << index_name << "' does not exist" << endl;
            return;
        }
    }

    // --at '{field:op:val, ...}'
    vector<Condition> conditions;
    auto at_it = args.flags.find("--at");
    if (at_it != args.flags.end()) {
        string combined = join_tokens(at_it->second);
        conditions = parse_conditions(combined, t.schema.fields);

        // Validate conditions use pk fields (or index pk fields)
        if (index_name.empty()) {
            for (const auto& cond : conditions) {
                bool is_pk = false;
                for (const auto& pk : t.schema.pk_fields)
                    if (pk == cond.field) { is_pk = true; break; }
                if (!is_pk) {
                    cerr << "Error: --at field '" << cond.field << "' must be a primary key field" << endl;
                    return;
                }
            }
        } else {
            const IndexDef* idx = t.find_index(index_name);
            for (const auto& cond : conditions) {
                bool is_idx_pk = false;
                for (const auto& pk : idx->pk_fields)
                    if (pk == cond.field) { is_idx_pk = true; break; }
                if (!is_idx_pk) {
                    cerr << "Error: --at field '" << cond.field << "' must be an index pk field" << endl;
                    return;
                }
            }
        }
    }

    // --set '{field:value, ...}'
    auto set_it = args.flags.find("--set");
    string set_combined = join_tokens(set_it->second);
    Row updates = parse_set_clause(set_combined, t.schema);

    if (updates.empty()) {
        cerr << "Error: --set produced no updates" << endl;
        return;
    }

    // Prevent updating pk fields
    for (const auto& [k, v] : updates) {
        for (const auto& pk : t.schema.pk_fields) {
            if (k == pk) {
                cerr << "Error: cannot update primary key field '" << k << "'" << endl;
                return;
            }
        }
    }

    t.update(conditions, updates, index_name);
    save_table(t);

    // Count matched rows (approximate: rows still in table after update)
    cout << "Updated table '" << name << "'" << endl;
}
