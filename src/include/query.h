#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
#include "storage.h"
#include "parser.h"
#include <iostream>

using namespace std;

static inline void print_row(const Row& row, const TableSchema& schema) {
    cout << '{';
    bool first = true;
    for (const auto& [fname, ftype] : schema.fields) {
        auto it = row.find(fname);
        if (it == row.end()) continue;
        if (!first) cout << ", ";
        cout << '"' << fname << "\": ";
        if (holds_alternative<string>(it->second))
            cout << '"' << get<string>(it->second) << '"';
        else if (holds_alternative<int>(it->second))
            cout << get<int>(it->second);
        else
            cout << get<double>(it->second);
        first = false;
    }
    for (const auto& [k, v] : row) {
        if (!schema.fields.count(k)) {
            if (!first) cout << ", ";
            cout << '"' << k << "\": \"" << field_value_to_str(v) << '"';
            first = false;
        }
    }
    cout << "}\n";
}

inline void query_rows(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
        return;
    }

    // Load schema only for validation — rows streamed below
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

    // --query-condition (accumulate all conditions from all tokens)
    vector<Condition> all_conditions;
    auto cond_it = args.flags.find("--query-condition");
    if (cond_it != args.flags.end()) {
        string combined = join_tokens(cond_it->second);
        size_t i = 0;
        while (i < combined.size()) {
            while (i < combined.size() && isspace((unsigned char)combined[i])) i++;
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
                auto conds = parse_conditions(block, schema.fields);
                for (auto& c : conds) all_conditions.push_back(std::move(c));
            } else { i++; }
        }
    }

    // --output-fields
    vector<string> out_fields;
    auto out_it = args.flags.find("--output-fields");
    if (out_it != args.flags.end()) {
        string combined = join_tokens(out_it->second);
        out_fields = parse_field_list(combined);
        for (const auto& f : out_fields) {
            if (!schema.fields.count(f)) {
                cerr << "Error: output field '" << f << "' does not exist" << endl;
                return;
            }
        }
    }

    // Validate condition fields exist
    for (const auto& cond : all_conditions) {
        if (!schema.fields.count(cond.field)) {
            cerr << "Error: condition field '" << cond.field << "' does not exist" << endl;
            return;
        }
    }

    // Build a temporary schema for output projection
    TableSchema out_schema = schema;
    if (!out_fields.empty()) {
        unordered_map<string, FieldType> proj;
        for (const auto& f : out_fields)
            if (schema.fields.count(f)) proj[f] = schema.fields.at(f);
        out_schema.fields = proj;
    }

    // Build Table for condition matching (no rows loaded)
    Table t;
    t.schema = schema;

    // Stream: read rows one-at-a-time, print matches immediately
    // Collect count first via two-pass? No — buffer count line.
    // We'll do a single pass and buffer output with row count at end.
    // For extreme I/O, print count last to avoid two-pass.
    vector<string> output_lines;
    size_t count = stream_query(
        name, schema,
        [&](const Row& row) { return t.matches_conditions(row, all_conditions); },
        [&](const Row& row) {
            // Project if needed
            if (out_fields.empty()) {
                // serialize to string for later output
                string line;
                // Build projected row string inline
                line += '{';
                bool first = true;
                for (const auto& [fname, ftype] : schema.fields) {
                    auto it = row.find(fname);
                    if (it == row.end()) continue;
                    if (!first) line += ", ";
                    line += '"'; line += fname; line += "\": ";
                    if (holds_alternative<string>(it->second))
                        line += '"' + get<string>(it->second) + '"';
                    else if (holds_alternative<int>(it->second))
                        line += to_string(get<int>(it->second));
                    else {
                        ostringstream oss;
                        oss << get<double>(it->second);
                        line += oss.str();
                    }
                    first = false;
                }
                line += '}';
                output_lines.push_back(std::move(line));
            } else {
                string line;
                line += '{';
                bool first = true;
                for (const auto& f : out_fields) {
                    auto it = row.find(f);
                    if (it == row.end()) continue;
                    if (!first) line += ", ";
                    line += '"'; line += f; line += "\": ";
                    if (holds_alternative<string>(it->second))
                        line += '"' + get<string>(it->second) + '"';
                    else if (holds_alternative<int>(it->second))
                        line += to_string(get<int>(it->second));
                    else {
                        ostringstream oss;
                        oss << get<double>(it->second);
                        line += oss.str();
                    }
                    first = false;
                }
                line += '}';
                output_lines.push_back(std::move(line));
            }
        }
    );

    cout << "Results (" << count << " rows):\n";
    for (const auto& line : output_lines)
        cout << line << '\n';
}
