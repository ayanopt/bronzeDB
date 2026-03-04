#pragma once
#include "types.h"
#include "table.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

static const string DATA_DIR = "./data/";

static inline void ensure_data_dir() {
    fs::create_directories(DATA_DIR);
}

static inline string table_path(const string& name) {
    return DATA_DIR + name + ".bdb";
}

inline bool table_exists(const string& name) {
    return fs::exists(table_path(name));
}

inline bool delete_table_file(const string& name) {
    return fs::remove(table_path(name));
}

// Serialize a FieldValue to a JSON value string
static inline string fv_to_json(const FieldValue& v, FieldType ft) {
    if (holds_alternative<string>(v)) {
        string s = get<string>(v);
        // Escape quotes inside
        string escaped;
        for (char c : s) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        return "\"" + escaped + "\"";
    }
    if (holds_alternative<int>(v))    return to_string(get<int>(v));
    if (holds_alternative<double>(v)) {
        // Use enough precision
        ostringstream oss;
        oss << get<double>(v);
        return oss.str();
    }
    return "\"\"";
}

// Save table to disk
inline void save_table(const Table& t) {
    ensure_data_dir();
    ofstream f(table_path(t.schema.name));
    if (!f) throw runtime_error("Cannot write file: " + table_path(t.schema.name));

    f << "SCHEMA\n";
    f << "name=" << t.schema.name << "\n";

    // Write fields in consistent order
    for (const auto& [name, type] : t.schema.fields) {
        f << "field=" << name << ":" << field_type_to_str(type) << "\n";
    }

    // pk (may be composite)
    f << "pk=";
    for (size_t i = 0; i < t.schema.pk_fields.size(); i++) {
        if (i) f << ",";
        f << t.schema.pk_fields[i];
    }
    f << "\n";

    // sk (optional)
    if (!t.schema.sk_field.empty()) {
        f << "sk=" << t.schema.sk_field << "\n";
    }

    // indexes
    for (const auto& idx : t.schema.indexes) {
        // index=name|pk=f1,f2|sk=f1|proj=f1:type,f2:type
        f << "index=" << idx.name;
        f << "|pk=";
        for (size_t i = 0; i < idx.pk_fields.size(); i++) {
            if (i) f << ",";
            f << idx.pk_fields[i];
        }
        if (!idx.sk_fields.empty()) {
            f << "|sk=";
            for (size_t i = 0; i < idx.sk_fields.size(); i++) {
                if (i) f << ",";
                f << idx.sk_fields[i];
            }
        }
        if (!idx.projection.empty()) {
            f << "|proj=";
            bool first = true;
            for (const auto& [fn, ft] : idx.projection) {
                if (!first) f << ",";
                f << fn << ":" << field_type_to_str(ft);
                first = false;
            }
        }
        f << "\n";
    }

    f << "DATA\n";

    // Write each row as JSON line
    for (const auto& row : t.rows) {
        f << "{";
        bool first = true;
        // Write fields in schema order for consistency
        for (const auto& [name, type] : t.schema.fields) {
            auto it = row.find(name);
            if (it == row.end()) continue;
            if (!first) f << ",";
            f << "\"" << name << "\":" << fv_to_json(it->second, type);
            first = false;
        }
        // Write any extra fields not in schema (shouldn't happen but safe)
        for (const auto& [k, v] : row) {
            if (!t.schema.fields.count(k)) {
                if (!first) f << ",";
                f << "\"" << k << "\":\"" << field_value_to_str(v) << "\"";
                first = false;
            }
        }
        f << "}\n";
    }
}

// Parse a single JSON line into a Row given schema
static inline Row parse_data_line(const string& line, const TableSchema& schema) {
    Row row;
    // Strip outer braces
    string inner = line;
    if (!inner.empty() && inner.front() == '{') inner = inner.substr(1);
    if (!inner.empty() && inner.back() == '}')  inner = inner.substr(0, inner.size() - 1);

    // Parse key:value pairs respecting quotes and nested braces
    size_t i = 0;
    while (i < inner.size()) {
        // Skip whitespace
        while (i < inner.size() && isspace(inner[i])) i++;
        if (i >= inner.size()) break;

        // Expect '"key"'
        if (inner[i] != '"') { i++; continue; }
        i++; // skip opening quote
        string key;
        while (i < inner.size() && inner[i] != '"') {
            if (inner[i] == '\\' && i+1 < inner.size()) { i++; key += inner[i]; }
            else key += inner[i];
            i++;
        }
        if (i < inner.size()) i++; // skip closing quote

        // Skip ':'
        while (i < inner.size() && (isspace(inner[i]) || inner[i] == ':')) i++;

        // Read value
        string val_str;
        if (i >= inner.size()) break;

        if (inner[i] == '"') {
            // Quoted string
            i++; // skip opening quote
            while (i < inner.size() && inner[i] != '"') {
                if (inner[i] == '\\' && i+1 < inner.size()) { i++; val_str += inner[i]; }
                else val_str += inner[i];
                i++;
            }
            if (i < inner.size()) i++; // skip closing quote
            row[key] = val_str;
        } else if (inner[i] == '{') {
            // Nested object - treat as string
            int depth = 0;
            while (i < inner.size()) {
                if (inner[i] == '{') depth++;
                else if (inner[i] == '}') { depth--; if (depth == 0) { val_str += inner[i]; i++; break; } }
                val_str += inner[i];
                i++;
            }
            row[key] = val_str;
        } else {
            // Number
            while (i < inner.size() && inner[i] != ',' && inner[i] != '}') {
                val_str += inner[i];
                i++;
            }
            val_str = [&]() {
                string s = val_str;
                size_t s_start = s.find_first_not_of(" \t");
                size_t s_end = s.find_last_not_of(" \t");
                if (s_start == string::npos) return string("");
                return s.substr(s_start, s_end - s_start + 1);
            }();

            FieldType ft = FieldType::STRING;
            if (schema.fields.count(key)) ft = schema.fields.at(key);

            if (ft == FieldType::INT) {
                try { row[key] = stoi(val_str); } catch (...) { row[key] = val_str; }
            } else if (ft == FieldType::DOUBLE) {
                try { row[key] = stod(val_str); } catch (...) { row[key] = val_str; }
            } else {
                // Auto-detect
                try { row[key] = stoi(val_str); }
                catch (...) {
                    try { row[key] = stod(val_str); }
                    catch (...) { row[key] = val_str; }
                }
            }
        }

        // Skip comma
        while (i < inner.size() && (isspace(inner[i]) || inner[i] == ',')) i++;
    }
    return row;
}

// Load table from disk
inline Table load_table(const string& name) {
    Table t;
    t.schema.name = name;

    ifstream f(table_path(name));
    if (!f) throw runtime_error("Table not found: " + name);

    string line;
    bool in_data = false;

    while (getline(f, line)) {
        if (line == "SCHEMA") continue;
        if (line == "DATA") { in_data = true; continue; }

        if (in_data) {
            if (!line.empty()) {
                t.rows.push_back(parse_data_line(line, t.schema));
            }
            continue;
        }

        // Parse schema lines
        if (line.rfind("name=", 0) == 0) {
            t.schema.name = line.substr(5);
        } else if (line.rfind("field=", 0) == 0) {
            string rest = line.substr(6);
            size_t colon = rest.find(':');
            if (colon != string::npos) {
                string fname = rest.substr(0, colon);
                string ftype = rest.substr(colon + 1);
                t.schema.fields[fname] = str_to_field_type(ftype);
            }
        } else if (line.rfind("pk=", 0) == 0) {
            string rest = line.substr(3);
            // Split by comma
            stringstream ss(rest);
            string token;
            while (getline(ss, token, ',')) {
                if (!token.empty()) t.schema.pk_fields.push_back(token);
            }
        } else if (line.rfind("sk=", 0) == 0) {
            t.schema.sk_field = line.substr(3);
        } else if (line.rfind("index=", 0) == 0) {
            // index=name|pk=f1,f2|sk=f1|proj=f1:type,f2:type
            string rest = line.substr(6);
            IndexDef idx;

            // Split by '|'
            vector<string> parts;
            stringstream ss(rest);
            string segment;
            while (getline(ss, segment, '|')) parts.push_back(segment);

            if (!parts.empty()) idx.name = parts[0];
            for (size_t i = 1; i < parts.size(); i++) {
                const string& seg = parts[i];
                if (seg.rfind("pk=", 0) == 0) {
                    stringstream ss2(seg.substr(3));
                    string tok;
                    while (getline(ss2, tok, ',')) if (!tok.empty()) idx.pk_fields.push_back(tok);
                } else if (seg.rfind("sk=", 0) == 0) {
                    stringstream ss2(seg.substr(3));
                    string tok;
                    while (getline(ss2, tok, ',')) if (!tok.empty()) idx.sk_fields.push_back(tok);
                } else if (seg.rfind("proj=", 0) == 0) {
                    string proj_str = seg.substr(5);
                    stringstream ss2(proj_str);
                    string tok;
                    while (getline(ss2, tok, ',')) {
                        size_t colon = tok.find(':');
                        if (colon != string::npos) {
                            idx.projection[tok.substr(0, colon)] = str_to_field_type(tok.substr(colon+1));
                        }
                    }
                }
            }
            t.schema.indexes.push_back(idx);
        }
    }
    return t;
}
