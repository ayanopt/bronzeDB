#pragma once
#include "args.h"
#include "types.h"
#include "table.h"
#include "storage.h"
#include "parser.h"
#include <iostream>

using namespace std;

inline void edit_table(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
        return;
    }

    Table t = load_table(name);

    // --add-fields '{sex:string}'
    auto add_it = args.flags.find("--add-fields");
    if (add_it != args.flags.end()) {
        string combined = join_tokens(add_it->second);
        auto new_fields = parse_field_defs(combined);
        try {
            t.add_fields(new_fields);
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            return;
        }
    }

    // --remove-fields '{fur_color}'
    auto rm_it = args.flags.find("--remove-fields");
    if (rm_it != args.flags.end()) {
        string combined = join_tokens(rm_it->second);
        auto fields = parse_field_list(combined);
        try {
            t.remove_fields(fields);
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            return;
        }
    }

    // --delete-index index_name
    auto del_idx_it = args.flags.find("--delete-index");
    if (del_idx_it != args.flags.end()) {
        for (const auto& idx_name : del_idx_it->second) {
            try {
                t.delete_index(idx_name);
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << endl;
                return;
            }
        }
    }

    // --add-index name --add-index-pk name {fields} --add-index-sk name {fields} --add-index-projection name {fields}
    auto idx_names_it = args.flags.find("--add-index");
    if (idx_names_it != args.flags.end()) {
        auto idx_pk_it   = args.flags.find("--add-index-pk");
        auto idx_sk_it   = args.flags.find("--add-index-sk");
        auto idx_proj_it = args.flags.find("--add-index-projection");

        auto parse_name_fields_pairs = [](const vector<string>& tokens)
            -> vector<pair<string, string>> {
            vector<pair<string, string>> result;
            size_t i = 0;
            while (i < tokens.size()) {
                string name_token = tokens[i++];
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

        unordered_map<string, IndexDef> idx_map;
        for (const auto& idx_name : idx_names_it->second) {
            idx_map[idx_name].name = idx_name;
        }
        for (const auto& [iname, fields] : idx_pks)   idx_map[iname].pk_fields = parse_field_list(fields);
        for (const auto& [iname, fields] : idx_sks)   idx_map[iname].sk_fields = parse_field_list(fields);
        for (const auto& [iname, fields] : idx_projs) idx_map[iname].projection = parse_field_defs(fields);

        for (const auto& idx_name : idx_names_it->second) {
            if (idx_map.count(idx_name)) {
                try {
                    t.add_index(idx_map[idx_name]);
                } catch (const exception& e) {
                    cerr << "Error: " << e.what() << endl;
                    return;
                }
            }
        }
    }

    save_table(t);
    cout << "Edited table '" << name << "'" << endl;
}
