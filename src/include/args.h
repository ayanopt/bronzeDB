#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
using namespace std;
// 100% claude code generated I do not like flag logic
enum class Command { CREATE, DELETE, UPDATE, INSERT, QUERY, NONE };

struct ParsedArgs {
    Command command = Command::NONE;
    string tableName;
    unordered_map<string, vector<string>> flags;
};

const unordered_map<Command, unordered_set<string>> VALID_FLAGS = {
    {Command::CREATE, {"--fields", "--primary-key", "--sort-key", "--add-index", "--add-index-pk", "--add-index-sk", "--add-index-projection"}},
    {Command::DELETE, {}},
    {Command::UPDATE, {"--add-fields", "--remove-fields", "--delete-index", "--add-index", "--add-index-pk", "--add-index-sk", "--add-index-projection"}},
    {Command::INSERT, {"--data"}},
    {Command::QUERY,  {"--query-condition", "--use-index", "--output-fields"}},
};

Command parseCommand(const string& s) {
    if (s == "--create") return Command::CREATE;
    if (s == "--delete") return Command::DELETE;
    if (s == "--update") return Command::UPDATE;
    if (s == "--insert") return Command::INSERT;
    if (s == "--query")  return Command::QUERY;
    return Command::NONE;
}

ParsedArgs parseArgs(int argc, char** argv) {
    ParsedArgs args;

    if (argc < 2) {
        cerr << "Usage: ./bronzedb <command> <table_name> [flags...]" << endl;
        return args;
    }

    args.command = parseCommand(argv[1]);
    if (args.command == Command::NONE) {
        cerr << "Error: unknown command '" << argv[1] << "'" << endl;
        cerr << "Valid commands: --create, --delete, --update, --insert, --query" << endl;
        return args;
    }

    if (argc < 3) {
        cerr << "Error: missing table name" << endl;
        return {Command::NONE, "", {}};
    }

    args.tableName = argv[2];

    // Parse sub-flags and their values from argv[3..]
    string currentFlag;
    for (int i = 3; i < argc; i++) {
        string token = argv[i];

        // Check if this token is a new flag (starts with --)
        // but only if we're not inside a brace block
        bool isFlag = false;
        if (token.rfind("--", 0) == 0) {
            // Look back at collected values for the current flag
            // and count brace depth to see if we're inside a {...} block
            int braceDepth = 0;
            if (!currentFlag.empty()) {
                for (const string& val : args.flags[currentFlag]) {
                    for (char c : val) {
                        if (c == '{') braceDepth++;
                        else if (c == '}') braceDepth--;
                    }
                }
            }
            if (braceDepth <= 0) {
                isFlag = true;
            }
        }

        if (isFlag) {
            currentFlag = token;
            if (args.flags.find(currentFlag) == args.flags.end()) {
                args.flags[currentFlag] = {};
            }
        } else {
            if (currentFlag.empty()) {
                cerr << "Error: unexpected value '" << token << "' before any flag" << endl;
                return {Command::NONE, "", {}};
            }
            args.flags[currentFlag].push_back(token);
        }
    }

    // Validate that all provided flags are valid for this command
    const auto& allowed = VALID_FLAGS.at(args.command);
    for (const auto& [flag, _] : args.flags) {
        if (allowed.find(flag) == allowed.end()) {
            cerr << "Error: invalid flag '" << flag << "' for this command" << endl;
            return {Command::NONE, "", {}};
        }
    }

    return args;
}

bool validateArgs(const ParsedArgs& args) {
    switch (args.command) {
        case Command::CREATE: {
            if (args.flags.find("--fields") == args.flags.end()) {
                cerr << "Error: --create requires --fields" << endl;
                return false;
            }
            if (args.flags.find("--primary-key") == args.flags.end()) {
                cerr << "Error: --create requires --primary-key" << endl;
                return false;
            }
            break;
        }
        case Command::DELETE:
            break;
        case Command::UPDATE: {
            bool hasMod = args.flags.find("--add-fields") != args.flags.end()
                       || args.flags.find("--remove-fields") != args.flags.end()
                       || args.flags.find("--delete-index") != args.flags.end()
                       || args.flags.find("--add-index") != args.flags.end();
            if (!hasMod) {
                cerr << "Error: --update requires at least one modification flag (--add-fields, --remove-fields, --delete-index, --add-index)" << endl;
                return false;
            }
            break;
        }
        case Command::INSERT: {
            if (args.flags.find("--data") == args.flags.end()) {
                cerr << "Error: --insert requires --data" << endl;
                return false;
            }
            break;
        }
        case Command::QUERY: {
            if (args.flags.find("--query-condition") == args.flags.end()) {
                cerr << "Error: --query requires --query-condition" << endl;
                return false;
            }
            break;
        }
        case Command::NONE:
            return false;
    }

    // Cross-flag dependency: --add-index requires --add-index-pk
    if (args.flags.find("--add-index") != args.flags.end()
        && args.flags.find("--add-index-pk") == args.flags.end()) {
        cerr << "Error: --add-index requires --add-index-pk" << endl;
        return false;
    }

    return true;
}
