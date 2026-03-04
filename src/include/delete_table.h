#pragma once
#include "args.h"
#include "storage.h"
#include <iostream>

using namespace std;

inline void delete_table(const ParsedArgs& args) {
    const string& name = args.table_name;

    if (!table_exists(name)) {
        cerr << "Error: table '" << name << "' does not exist" << endl;
        return;
    }

    delete_table_file(name);
    cout << "Deleted table '" << name << "'" << endl;
}
