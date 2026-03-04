#pragma once
#include "types.h"
#include "table.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <functional>

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

// ─── Serialization helpers ─────────────────────────────────────────────────

// Serialize a FieldValue to a JSON value string
static inline string fv_to_json(const FieldValue& v, FieldType ft) {
    if (holds_alternative<string>(v)) {
        const string& s = get<string>(v);
        string escaped;
        escaped.reserve(s.size() + 2);
        for (char c : s) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        return "\"" + escaped + "\"";
    }
    if (holds_alternative<int>(v))    return to_string(get<int>(v));
    if (holds_alternative<double>(v)) {
        ostringstream oss;
        oss << get<double>(v);
        return oss.str();
    }
    return "\"\"";
}

// Serialize a Row to a single JSON line (no trailing newline)
static inline string serialize_row(const Row& row, const TableSchema& schema) {
    string out;
    out.reserve(256);
    out += '{';
    bool first = true;
    for (const auto& [name, type] : schema.fields) {
        auto it = row.find(name);
        if (it == row.end()) continue;
        if (!first) out += ',';
        out += '"'; out += name; out += "\":";
        out += fv_to_json(it->second, type);
        first = false;
    }
    // Extra fields not in schema (safety net)
    for (const auto& [k, v] : row) {
        if (!schema.fields.count(k)) {
            if (!first) out += ',';
            out += '"'; out += k; out += "\":\"";
            out += field_value_to_str(v);
            out += '"';
            first = false;
        }
    }
    out += '}';
    return out;
}

// Write schema section to a stream
static inline void write_schema(ostream& f, const TableSchema& schema) {
    f << "SCHEMA\n";
    f << "name=" << schema.name << "\n";
    for (const auto& [name, type] : schema.fields)
        f << "field=" << name << ":" << field_type_to_str(type) << "\n";

    f << "pk=";
    for (size_t i = 0; i < schema.pk_fields.size(); i++) {
        if (i) f << ",";
        f << schema.pk_fields[i];
    }
    f << "\n";

    if (!schema.sk_field.empty())
        f << "sk=" << schema.sk_field << "\n";

    for (const auto& idx : schema.indexes) {
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
}

// ─── Schema parsing (shared by load_schema_only and load_table) ────────────

static inline void parse_schema_line(const string& line, TableSchema& schema) {
    if (line.rfind("name=", 0) == 0) {
        schema.name = line.substr(5);
    } else if (line.rfind("field=", 0) == 0) {
        string rest = line.substr(6);
        size_t colon = rest.find(':');
        if (colon != string::npos)
            schema.fields[rest.substr(0, colon)] = str_to_field_type(rest.substr(colon + 1));
    } else if (line.rfind("pk=", 0) == 0) {
        stringstream ss(line.substr(3));
        string tok;
        while (getline(ss, tok, ','))
            if (!tok.empty()) schema.pk_fields.push_back(tok);
    } else if (line.rfind("sk=", 0) == 0) {
        schema.sk_field = line.substr(3);
    } else if (line.rfind("index=", 0) == 0) {
        string rest = line.substr(6);
        IndexDef idx;
        vector<string> parts;
        stringstream ss(rest);
        string seg;
        while (getline(ss, seg, '|')) parts.push_back(seg);
        if (!parts.empty()) idx.name = parts[0];
        for (size_t i = 1; i < parts.size(); i++) {
            const string& s = parts[i];
            if (s.rfind("pk=", 0) == 0) {
                stringstream ss2(s.substr(3)); string tok;
                while (getline(ss2, tok, ',')) if (!tok.empty()) idx.pk_fields.push_back(tok);
            } else if (s.rfind("sk=", 0) == 0) {
                stringstream ss2(s.substr(3)); string tok;
                while (getline(ss2, tok, ',')) if (!tok.empty()) idx.sk_fields.push_back(tok);
            } else if (s.rfind("proj=", 0) == 0) {
                stringstream ss2(s.substr(5)); string tok;
                while (getline(ss2, tok, ',')) {
                    size_t colon = tok.find(':');
                    if (colon != string::npos)
                        idx.projection[tok.substr(0, colon)] = str_to_field_type(tok.substr(colon+1));
                }
            }
        }
        schema.indexes.push_back(idx);
    }
}

// ─── Row parsing ──────────────────────────────────────────────────────────

static inline Row parse_data_line(const string& line, const TableSchema& schema) {
    Row row;
    const char* p = line.c_str();
    const char* end = p + line.size();

    // Skip leading '{'
    if (p < end && *p == '{') p++;

    while (p < end) {
        // Skip whitespace + commas
        while (p < end && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (p >= end || *p == '}') break;

        // Parse key (quoted)
        if (*p != '"') { p++; continue; }
        p++; // skip '"'
        string key;
        while (p < end && *p != '"') {
            if (*p == '\\' && p+1 < end) { p++; key += *p; }
            else key += *p;
            p++;
        }
        if (p < end) p++; // skip '"'

        // Skip ':'
        while (p < end && (isspace((unsigned char)*p) || *p == ':')) p++;
        if (p >= end) break;

        FieldType ft = FieldType::STRING;
        auto fit = schema.fields.find(key);
        if (fit != schema.fields.end()) ft = fit->second;

        if (*p == '"') {
            // Quoted string value
            p++;
            string val;
            while (p < end && *p != '"') {
                if (*p == '\\' && p+1 < end) { p++; val += *p; }
                else val += *p;
                p++;
            }
            if (p < end) p++;
            row[key] = val;
        } else if (*p == '{') {
            // Nested object → keep as string
            int depth = 0;
            string val;
            while (p < end) {
                if (*p == '{') depth++;
                else if (*p == '}') { depth--; val += *p; p++; if (depth == 0) break; continue; }
                val += *p; p++;
            }
            row[key] = val;
        } else {
            // Numeric value
            const char* num_start = p;
            while (p < end && *p != ',' && *p != '}') p++;
            string val_str(num_start, p);
            // rtrim
            while (!val_str.empty() && isspace((unsigned char)val_str.back())) val_str.pop_back();

            if (ft == FieldType::INT) {
                try { row[key] = stoi(val_str); } catch (...) { row[key] = val_str; }
            } else if (ft == FieldType::DOUBLE) {
                try { row[key] = stod(val_str); } catch (...) { row[key] = val_str; }
            } else {
                try { row[key] = stoi(val_str); }
                catch (...) { try { row[key] = stod(val_str); } catch (...) { row[key] = val_str; } }
            }
        }
    }
    return row;
}

// ─── Full load/save (used by schema-mutation ops: CREATE, EDIT) ────────────

inline void save_table(const Table& t) {
    ensure_data_dir();
    // Use large write buffer
    const int BUFSIZE = 1 << 20; // 1 MB
    char buf[BUFSIZE];
    ofstream f(table_path(t.schema.name));
    if (!f) throw runtime_error("Cannot write file: " + table_path(t.schema.name));
    f.rdbuf()->pubsetbuf(buf, BUFSIZE);

    write_schema(f, t.schema);
    for (const auto& row : t.rows) {
        f << serialize_row(row, t.schema) << '\n';
    }
}

inline Table load_table(const string& name) {
    Table t;
    t.schema.name = name;

    const int BUFSIZE = 1 << 20;
    char buf[BUFSIZE];
    ifstream f(table_path(name));
    if (!f) throw runtime_error("Table not found: " + name);
    f.rdbuf()->pubsetbuf(buf, BUFSIZE);

    string line;
    bool in_data = false;
    while (getline(f, line)) {
        if (line == "SCHEMA") continue;
        if (line == "DATA")  { in_data = true; continue; }
        if (in_data) {
            if (!line.empty()) t.rows.push_back(parse_data_line(line, t.schema));
        } else {
            parse_schema_line(line, t.schema);
        }
    }
    return t;
}

// ─── Optimized: schema-only load (O(schema_size), not O(file_size)) ────────

inline TableSchema load_schema_only(const string& name) {
    TableSchema schema;
    schema.name = name;

    ifstream f(table_path(name));
    if (!f) throw runtime_error("Table not found: " + name);

    string line;
    while (getline(f, line)) {
        if (line == "SCHEMA" || line == "name=" + name) continue;
        if (line == "DATA") break; // stop here — skip all rows
        parse_schema_line(line, schema);
    }
    return schema;
}

// ─── Optimized: append single row without rewriting the file ───────────────
// O(1) I/O instead of O(N). Used by INSERT.

inline void append_row_to_file(const string& name, const Row& row, const TableSchema& schema) {
    // The file's last section is DATA. We just append a line.
    ofstream f(table_path(name), ios::app);
    if (!f) throw runtime_error("Cannot append to file: " + table_path(name));
    f << serialize_row(row, schema) << '\n';
}

// ─── Optimized: streaming QUERY ─────────────────────────────────────────────
// Reads rows one-at-a-time; O(1) peak memory, O(N) time.

inline size_t stream_query(
    const string& name,
    const TableSchema& schema,
    const function<bool(const Row&)>& filter,
    const function<void(const Row&)>& on_match)
{
    const int BUFSIZE = 1 << 20;
    char buf[BUFSIZE];
    ifstream f(table_path(name));
    if (!f) throw runtime_error("Table not found: " + name);
    f.rdbuf()->pubsetbuf(buf, BUFSIZE);

    string line;
    bool in_data = false;
    size_t count = 0;
    while (getline(f, line)) {
        if (!in_data) { if (line == "DATA") in_data = true; continue; }
        if (line.empty()) continue;
        Row row = parse_data_line(line, schema);
        if (filter(row)) { on_match(row); count++; }
    }
    return count;
}

// ─── Optimized: streaming UPDATE ─────────────────────────────────────────────
// Rewrites file via temp + rename; O(N) I/O, O(1) peak memory.

inline size_t stream_update(
    const string& name,
    const TableSchema& schema,
    const function<bool(const Row&)>& filter,
    const function<void(Row&)>& mutate)
{
    string src_path = table_path(name);
    string tmp_path = src_path + ".tmp";

    const int BUFSIZE = 1 << 20;
    char rbuf[BUFSIZE], wbuf[BUFSIZE];

    ifstream fin(src_path);
    if (!fin) throw runtime_error("Table not found: " + name);
    fin.rdbuf()->pubsetbuf(rbuf, BUFSIZE);

    ofstream fout(tmp_path);
    if (!fout) throw runtime_error("Cannot write temp file: " + tmp_path);
    fout.rdbuf()->pubsetbuf(wbuf, BUFSIZE);

    // Copy schema section verbatim
    string line;
    while (getline(fin, line)) {
        fout << line << '\n';
        if (line == "DATA") break;
    }

    size_t count = 0;
    while (getline(fin, line)) {
        if (line.empty()) { fout << '\n'; continue; }
        Row row = parse_data_line(line, schema);
        if (filter(row)) { mutate(row); count++; }
        fout << serialize_row(row, schema) << '\n';
    }

    fin.close();
    fout.close();

    // Atomic replace
    fs::rename(tmp_path, src_path);
    return count;
}
