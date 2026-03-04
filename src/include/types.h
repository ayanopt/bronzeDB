#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

using namespace std;

enum class FieldType { INT, DOUBLE, STRING };

using FieldValue = variant<int, double, string>;
using Row = unordered_map<string, FieldValue>;

struct Condition {
    string field;
    string op;
    FieldValue value;
};

struct IndexDef {
    string name;
    vector<string> pk_fields;
    vector<string> sk_fields;
    unordered_map<string, FieldType> projection; // empty = all fields
};

struct TableSchema {
    string name;
    unordered_map<string, FieldType> fields;
    vector<string> pk_fields;
    string sk_field;
    vector<IndexDef> indexes;
};

inline string field_type_to_str(FieldType t) {
    switch (t) {
        case FieldType::INT:    return "int";
        case FieldType::DOUBLE: return "double";
        case FieldType::STRING: return "string";
    }
    return "string";
}

inline FieldType str_to_field_type(const string& s) {
    if (s == "int")    return FieldType::INT;
    if (s == "double") return FieldType::DOUBLE;
    return FieldType::STRING;
}

inline string field_value_to_str(const FieldValue& v) {
    if (holds_alternative<int>(v))    return to_string(get<int>(v));
    if (holds_alternative<double>(v)) return to_string(get<double>(v));
    return get<string>(v);
}
