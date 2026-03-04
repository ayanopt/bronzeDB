#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
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

    Table t = load_table(name);

    string combined = join_tokens(data_it->second);
    Row row = parse_json_row(combined, t.schema);

    // Validate all required fields
    for (const auto& [fname, ftype] : t.schema.fields) {
        if (row.find(fname) == row.end()) {
            // Missing field - insert default
            switch (ftype) {
                case FieldType::INT:    row[fname] = 0;        break;
                case FieldType::DOUBLE: row[fname] = 0.0;      break;
                case FieldType::STRING: row[fname] = string(""); break;
            }
        }
    }

    try {
        t.insert(row);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return;
    }

    save_table(t);
    cout << "Inserted row into '" << name << "'" << endl;
}
