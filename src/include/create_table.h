#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
#include "storage.h"
#include "parser.h"
#include <iostream>

using namespace std;

inline void create_table(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (table_exists(name)) {
        cerr << "Error: table '" << name << "' already exists" << endl;
        return;
    }

    Table t;
    t.schema.name = name;

    // --fields '{monkey_id:string, species:string, ...}'
    auto fields_it = args.flags.find("--fields");
    if (fields_it != args.flags.end()) {
        string combined = join_tokens(fields_it->second);
        t.schema.fields = parse_field_defs(combined);
    }

    // --primary-key '{monkey_id}' or '{tail_length, species}'
    auto pk_it = args.flags.find("--primary-key");
    if (pk_it != args.flags.end()) {
        string combined = join_tokens(pk_it->second);
        t.schema.pk_fields = parse_field_list(combined);
        for (const auto& pk : t.schema.pk_fields) {
            if (!t.schema.fields.count(pk)) {
                cerr << "Error: primary key field '" << pk << "' not in --fields" << endl;
                return;
            }
        }
    }

    // --sort-key age
    auto sk_it = args.flags.find("--sort-key");
    if (sk_it != args.flags.end() && !sk_it->second.empty()) {
        t.schema.sk_field = sk_it->second[0];
        if (!t.schema.fields.count(t.schema.sk_field)) {
            cerr << "Error: sort key field '" << t.schema.sk_field << "' not in --fields" << endl;
            return;
        }
    }

    // Indexes: --add-index name --add-index-pk name {fields} --add-index-sk name {fields} --add-index-projection name {fields}
    auto idx_names_it = args.flags.find("--add-index");
    if (idx_names_it != args.flags.end()) {
        auto idx_pk_it   = args.flags.find("--add-index-pk");
        auto idx_sk_it   = args.flags.find("--add-index-sk");
        auto idx_proj_it = args.flags.find("--add-index-projection");

        // Each index: tokens come in pairs: name {fields}
        // --add-index-pk rabid_species '{species, is_rabid}'
        // We pair them: even index = name, odd index = {fields}
        auto parse_name_fields_pairs = [&](const vector<string>& tokens)
            -> vector<pair<string, string>> {
            vector<pair<string, string>> result;
            // Reconstruct full string and re-split by detecting name/brace pairs
            size_t i = 0;
            while (i < tokens.size()) {
                string name_token = tokens[i++];
                // Collect following brace-block tokens
                string fields_str;
                while (i < tokens.size()) {
                    fields_str += (fields_str.empty() ? "" : " ") + tokens[i];
                    int depth = 0;
                    for (char c : fields_str) {
                        if (c == '{') depth++;
                        else if (c == '}') depth--;
                    }
                    i++;
                    if (depth <= 0) break;
                }
                result.push_back({name_token, fields_str});
            }
            return result;
        };

        auto idx_pks   = idx_pk_it   != args.flags.end() ? parse_name_fields_pairs(idx_pk_it->second)   : vector<pair<string,string>>{};
        auto idx_sks   = idx_sk_it   != args.flags.end() ? parse_name_fields_pairs(idx_sk_it->second)   : vector<pair<string,string>>{};
        auto idx_projs = idx_proj_it != args.flags.end() ? parse_name_fields_pairs(idx_proj_it->second) : vector<pair<string,string>>{};

        // Build a map from index name to its parts
        unordered_map<string, IndexDef> idx_map;
        for (const auto& idx_name : idx_names_it->second) {
            idx_map[idx_name].name = idx_name;
        }

        for (const auto& [iname, fields] : idx_pks) {
            idx_map[iname].pk_fields = parse_field_list(fields);
        }
        for (const auto& [iname, fields] : idx_sks) {
            idx_map[iname].sk_fields = parse_field_list(fields);
        }
        for (const auto& [iname, fields] : idx_projs) {
            idx_map[iname].projection = parse_field_defs(fields);
        }

        for (const auto& idx_name : idx_names_it->second) {
            if (idx_map.count(idx_name)) {
                const IndexDef& idx = idx_map[idx_name];
                // Validate pk fields exist
                for (const auto& f : idx.pk_fields) {
                    if (!t.schema.fields.count(f)) {
                        cerr << "Error: index pk field '" << f << "' not in --fields" << endl;
                        return;
                    }
                }
                t.schema.indexes.push_back(idx);
            }
        }
    }

    save_table(t);
    cout << "Created table '" << name << "'" << endl;
}
