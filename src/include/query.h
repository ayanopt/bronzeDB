#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
#include "storage.h"
#include "parser.h"
#include <iostream>
#include <iomanip>

using namespace std;

static inline void print_row(const Row& row, const TableSchema& schema) {
    cout << "{";
    bool first = true;
    // Print in schema field order
    for (const auto& [fname, ftype] : schema.fields) {
        auto it = row.find(fname);
        if (it == row.end()) continue;
        if (!first) cout << ", ";
        cout << "\"" << fname << "\": ";
        if (holds_alternative<string>(it->second))
            cout << "\"" << get<string>(it->second) << "\"";
        else if (holds_alternative<int>(it->second))
            cout << get<int>(it->second);
        else if (holds_alternative<double>(it->second))
            cout << get<double>(it->second);
        first = false;
    }
    // Any extra fields not in schema
    for (const auto& [k, v] : row) {
        if (!schema.fields.count(k)) {
            if (!first) cout << ", ";
            cout << "\"" << k << "\": \"" << field_value_to_str(v) << "\"";
            first = false;
        }
    }
    cout << "}" << endl;
}

inline void query_rows(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
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

    // --query-condition (may appear multiple times, each accumulating tokens)
    vector<Condition> all_conditions;
    auto cond_it = args.flags.find("--query-condition");
    if (cond_it != args.flags.end()) {
        // Each value in the vector is a token; reconstruct the brace-block(s)
        // The tokens may represent one or more conditions: {f:op:v} {f:op:v}
        // We need to split them back into individual brace-blocks
        string combined = join_tokens(cond_it->second);

        // Find all top-level brace blocks in combined
        size_t i = 0;
        while (i < combined.size()) {
            while (i < combined.size() && isspace(combined[i])) i++;
            if (i >= combined.size()) break;
            if (combined[i] == '{') {
                int depth = 0;
                size_t start = i;
                while (i < combined.size()) {
                    if (combined[i] == '{') depth++;
                    else if (combined[i] == '}') { depth--; if (depth == 0) { i++; break; } }
                    i++;
                }
                string block = combined.substr(start, i - start);
                auto conds = parse_conditions(block, t.schema.fields);
                for (auto& c : conds) all_conditions.push_back(c);
            } else {
                i++;
            }
        }
    }

    // --output-fields '{monkey_id, fur_color}'
    vector<string> out_fields;
    auto out_it = args.flags.find("--output-fields");
    if (out_it != args.flags.end()) {
        string combined = join_tokens(out_it->second);
        out_fields = parse_field_list(combined);
        // Validate fields exist
        for (const auto& f : out_fields) {
            bool found = t.schema.fields.count(f) > 0;
            if (!found && !index_name.empty()) {
                const IndexDef* idx = t.find_index(index_name);
                if (idx && !idx->projection.empty()) {
                    found = idx->projection.count(f) > 0;
                }
            }
            if (!t.schema.fields.count(f)) {
                cerr << "Error: output field '" << f << "' does not exist" << endl;
                return;
            }
        }
    }

    // Validate conditions reference existing fields
    for (const auto& cond : all_conditions) {
        if (!t.schema.fields.count(cond.field)) {
            cerr << "Error: condition field '" << cond.field << "' does not exist" << endl;
            return;
        }
    }

    auto results = t.query(all_conditions, index_name, out_fields);

    cout << "Results (" << results.size() << " rows):" << endl;
    for (const auto& row : results) {
        print_row(row, t.schema);
    }
}
