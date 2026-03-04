#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

using namespace std;

class Table {
public:
    TableSchema schema;
    vector<Row> rows;

    // Compare two FieldValues with an operator
    bool compare(const FieldValue& a, const string& op, const FieldValue& b) const {
        // Promote to double for numeric comparisons
        auto to_double = [](const FieldValue& v) -> double {
            if (holds_alternative<int>(v))    return (double)get<int>(v);
            if (holds_alternative<double>(v)) return get<double>(v);
            return 0.0;
        };

        bool a_str = holds_alternative<string>(a);
        bool b_str = holds_alternative<string>(b);

        if (a_str || b_str) {
            string sa = a_str ? get<string>(a) : field_value_to_str(a);
            string sb = b_str ? get<string>(b) : field_value_to_str(b);
            if (op == "=")  return sa == sb;
            if (op == ">")  return sa > sb;
            if (op == "<")  return sa < sb;
            if (op == ">=") return sa >= sb;
            if (op == "<=") return sa <= sb;
            return false;
        }

        double da = to_double(a);
        double db = to_double(b);
        if (op == "=")  return da == db;
        if (op == ">")  return da > db;
        if (op == "<")  return da < db;
        if (op == ">=") return da >= db;
        if (op == "<=") return da <= db;
        return false;
    }

    bool matches_conditions(const Row& row, const vector<Condition>& conds) const {
        for (const auto& cond : conds) {
            auto it = row.find(cond.field);
            if (it == row.end()) return false;
            if (!compare(it->second, cond.op, cond.value)) return false;
        }
        return true;
    }

    // Find index by name
    const IndexDef* find_index(const string& name) const {
        for (const auto& idx : schema.indexes)
            if (idx.name == name) return &idx;
        return nullptr;
    }

    vector<Row> query(const vector<Condition>& conds,
                      const string& index_name,
                      const vector<string>& out_fields) const {
        vector<Row> results;

        // If index_name provided, verify conditions match index pk fields
        if (!index_name.empty()) {
            const IndexDef* idx = find_index(index_name);
            if (!idx) {
                cerr << "Error: index '" << index_name << "' does not exist" << endl;
                return results;
            }
        }

        for (const auto& row : rows) {
            if (!matches_conditions(row, conds)) continue;

            if (out_fields.empty()) {
                results.push_back(row);
            } else {
                Row projected;
                for (const auto& f : out_fields) {
                    auto it = row.find(f);
                    if (it != row.end()) projected[f] = it->second;
                }
                results.push_back(projected);
            }
        }
        return results;
    }

    void insert(const Row& row) {
        // Validate required fields exist
        for (const auto& pk : schema.pk_fields) {
            if (row.find(pk) == row.end()) {
                throw runtime_error("Missing primary key field: " + pk);
            }
        }
        rows.push_back(row);
    }

    void update(const vector<Condition>& at, const Row& updates, const string& index_name) {
        if (!index_name.empty() && !find_index(index_name)) {
            cerr << "Error: index '" << index_name << "' does not exist" << endl;
            return;
        }
        for (auto& row : rows) {
            if (!matches_conditions(row, at)) continue;
            for (const auto& [k, v] : updates) {
                row[k] = v;
            }
        }
    }

    void add_fields(const unordered_map<string, FieldType>& new_fields) {
        for (const auto& [name, type] : new_fields) {
            if (schema.fields.count(name)) {
                throw runtime_error("Field already exists: " + name);
            }
            schema.fields[name] = type;
        }
    }

    void remove_fields(const vector<string>& field_names) {
        for (const auto& name : field_names) {
            // Check not a PK field
            for (const auto& pk : schema.pk_fields) {
                if (pk == name) throw runtime_error("Cannot remove primary key field: " + name);
            }
            if (name == schema.sk_field)
                throw runtime_error("Cannot remove sort key field: " + name);
            // Check not an index pk field
            for (const auto& idx : schema.indexes) {
                for (const auto& pk : idx.pk_fields) {
                    if (pk == name) throw runtime_error("Cannot remove index pk field: " + name);
                }
            }
            schema.fields.erase(name);
            // Remove from all rows
            for (auto& row : rows) row.erase(name);
        }
    }

    void add_index(const IndexDef& idx) {
        if (find_index(idx.name)) {
            throw runtime_error("Index already exists: " + idx.name);
        }
        // Validate pk fields exist
        for (const auto& f : idx.pk_fields) {
            if (!schema.fields.count(f))
                throw runtime_error("Index pk field does not exist: " + f);
        }
        schema.indexes.push_back(idx);
    }

    void delete_index(const string& name) {
        auto it = schema.indexes.begin();
        while (it != schema.indexes.end()) {
            if (it->name == name) { schema.indexes.erase(it); return; }
            ++it;
        }
        throw runtime_error("Index does not exist: " + name);
    }
};
