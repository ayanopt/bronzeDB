#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <iostream>

using namespace std;

// Trim whitespace from both ends
static inline string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Strip outer braces: "{...}" -> "..."
static inline string strip_braces(const string& s) {
    string t = trim(s);
    if (t.size() >= 2 && t.front() == '{' && t.back() == '}')
        return t.substr(1, t.size() - 2);
    return t;
}

// Split by a delimiter character, respecting nested braces and quotes
static inline vector<string> split_top_level(const string& s, char delim) {
    vector<string> parts;
    string cur;
    int depth = 0;
    bool in_quote = false;
    char quote_char = 0;

    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (in_quote) {
            cur += c;
            if (c == quote_char && (i == 0 || s[i-1] != '\\')) {
                in_quote = false;
            }
        } else if (c == '"' || c == '\'') {
            in_quote = true;
            quote_char = c;
            cur += c;
        } else if (c == '{') {
            depth++;
            cur += c;
        } else if (c == '}') {
            depth--;
            cur += c;
        } else if (c == delim && depth == 0) {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || !parts.empty())
        parts.push_back(trim(cur));
    return parts;
}

// Parse "{field:type, field:type}" -> map<name, FieldType>
inline unordered_map<string, FieldType> parse_field_defs(const string& s) {
    unordered_map<string, FieldType> result;
    string inner = strip_braces(s);
    if (inner.empty()) return result;

    auto parts = split_top_level(inner, ',');
    for (auto& part : parts) {
        auto kv = split_top_level(part, ':');
        if (kv.size() < 2) {
            cerr << "Warning: invalid field def '" << part << "'" << endl;
            continue;
        }
        string name = trim(kv[0]);
        string type_str = trim(kv[1]);
        result[name] = str_to_field_type(type_str);
    }
    return result;
}

// Parse "{f1, f2, f3}" -> vector<string>
inline vector<string> parse_field_list(const string& s) {
    vector<string> result;
    string inner = strip_braces(s);
    if (inner.empty()) return result;

    auto parts = split_top_level(inner, ',');
    for (auto& p : parts) {
        string t = trim(p);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

// Parse "{field:op:val, field:op:val}" -> vector<Condition>
// val can be quoted string "monkey1", int 4, or double 4.2
inline vector<Condition> parse_conditions(const string& s, const unordered_map<string, FieldType>& schema_fields) {
    vector<Condition> result;
    string inner = strip_braces(s);
    if (inner.empty()) return result;

    auto parts = split_top_level(inner, ',');
    for (auto& part : parts) {
        // Split on ':' but at most 3 parts (field, op, value which may contain colons in strings)
        // Find first colon, second colon
        string p = trim(part);
        size_t c1 = p.find(':');
        if (c1 == string::npos) { cerr << "Warning: invalid condition '" << p << "'" << endl; continue; }
        size_t c2 = p.find(':', c1 + 1);
        if (c2 == string::npos) { cerr << "Warning: invalid condition '" << p << "'" << endl; continue; }

        string field = trim(p.substr(0, c1));
        string op = trim(p.substr(c1 + 1, c2 - c1 - 1));
        string val_str = trim(p.substr(c2 + 1));

        // Determine type from schema if available
        FieldType ft = FieldType::STRING;
        if (schema_fields.count(field)) ft = schema_fields.at(field);

        FieldValue val;
        if (!val_str.empty() && val_str.front() == '"' && val_str.back() == '"') {
            val = val_str.substr(1, val_str.size() - 2);
            ft = FieldType::STRING;
        } else if (ft == FieldType::INT) {
            try { val = stoi(val_str); } catch (...) { val = val_str; }
        } else if (ft == FieldType::DOUBLE) {
            try { val = stod(val_str); } catch (...) { val = val_str; }
        } else {
            // Try to auto-detect
            try { val = stoi(val_str); ft = FieldType::INT; }
            catch (...) {
                try { val = stod(val_str); ft = FieldType::DOUBLE; }
                catch (...) { val = val_str; }
            }
        }

        result.push_back({field, op, val});
    }
    return result;
}

// Parse a JSON-like value string into FieldValue given a type
inline FieldValue parse_value_string(const string& s, FieldType ft) {
    string v = trim(s);
    if (!v.empty() && v.front() == '"' && v.back() == '"') {
        return v.substr(1, v.size() - 2);
    }
    switch (ft) {
        case FieldType::INT:
            try { return stoi(v); } catch (...) { return v; }
        case FieldType::DOUBLE:
            try { return stod(v); } catch (...) { return v; }
        case FieldType::STRING:
            return v;
    }
    return v;
}

// Parse JSON row: {"field": value, ...} -> Row
// Supports string values (quoted), int, double
inline Row parse_json_row(const string& s, const TableSchema& schema) {
    Row row;
    string inner = strip_braces(s);
    if (inner.empty()) return row;

    // Simple JSON parser: split on comma, handle "key": value
    // We need to handle nested {} for metadata string values
    auto parts = split_top_level(inner, ',');
    for (auto& part : parts) {
        string p = trim(part);
        // Find the colon after the key
        // Key should be quoted: "field_name"
        size_t key_end = p.find('"', 1);
        if (p.empty() || p.front() != '"' || key_end == string::npos) {
            // Try unquoted key
            size_t colon_pos = p.find(':');
            if (colon_pos == string::npos) continue;
            string key = trim(p.substr(0, colon_pos));
            string val_str = trim(p.substr(colon_pos + 1));

            FieldType ft = FieldType::STRING;
            if (schema.fields.count(key)) ft = schema.fields.at(key);
            row[key] = parse_value_string(val_str, ft);
            continue;
        }

        string key = p.substr(1, key_end - 1);
        // Find ':' after closing quote
        size_t colon_pos = p.find(':', key_end + 1);
        if (colon_pos == string::npos) continue;
        string val_str = trim(p.substr(colon_pos + 1));

        FieldType ft = FieldType::STRING;
        if (schema.fields.count(key)) ft = schema.fields.at(key);
        row[key] = parse_value_string(val_str, ft);
    }
    return row;
}

// Parse "--set" value: {field:value, field:value} with schema types
inline Row parse_set_clause(const string& s, const TableSchema& schema) {
    Row updates;
    string inner = strip_braces(s);
    if (inner.empty()) return updates;

    auto parts = split_top_level(inner, ',');
    for (auto& part : parts) {
        string p = trim(part);
        size_t colon = p.find(':');
        if (colon == string::npos) continue;
        string key = trim(p.substr(0, colon));
        string val_str = trim(p.substr(colon + 1));

        FieldType ft = FieldType::STRING;
        if (schema.fields.count(key)) ft = schema.fields.at(key);
        updates[key] = parse_value_string(val_str, ft);
    }
    return updates;
}

// Join tokens that may have been split by the shell parser
// (for multi-token flag values like --data '{ "a": 1 }' split across spaces)
inline string join_tokens(const vector<string>& tokens) {
    string result;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0) result += " ";
        result += tokens[i];
    }
    return result;
}
