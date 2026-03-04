#pragma once
#include "args.h"
#include "types.h"
#include "storage.h"
#include "parser.h"
#include <iostream>

using namespace std;

inline void insert_row(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
        return;
    }

    auto data_it = args.flags.find("--data");
    if (data_it == args.flags.end() || data_it->second.empty()) {
        cerr << "Error: --insert requires --data" << endl;
        return;
    }

    // Load schema only — O(schema_size), not O(file_size)
    TableSchema schema = load_schema_only(name);

    string combined = join_tokens(data_it->second);
    Row row = parse_json_row(combined, schema);

    // Fill missing fields with defaults
    for (const auto& [fname, ftype] : schema.fields) {
        if (row.find(fname) == row.end()) {
            switch (ftype) {
                case FieldType::INT:    row[fname] = 0;          break;
                case FieldType::DOUBLE: row[fname] = 0.0;        break;
                case FieldType::STRING: row[fname] = string(""); break;
            }
        }
    }

    // Validate PK fields present
    for (const auto& pk : schema.pk_fields) {
        if (row.find(pk) == row.end()) {
            cerr << "Error: missing primary key field: " << pk << endl;
            return;
        }
    }

    // Append single row — O(1) I/O, no file rewrite
    try {
        append_row_to_file(name, row, schema);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return;
    }

    cout << "Inserted row into '" << name << "'" << endl;
}
