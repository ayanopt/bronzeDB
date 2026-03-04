#include "utils.h"
#include "args.h"
#include "create_table.h"
#include "delete_table.h"
#include "edit_table.h"
#include "update_rows.h"
#include "insert.h"
#include "query.h"

int main(int argc, char ** argv) {
    ParsedArgs args = parse_args(argc, argv);
    if (args.command == Command::NONE) return 1;
    if (!validate_args(args)) return 1;

    if      (args.command == Command::CREATE) create_table(args);
    else if (args.command == Command::DELETE) delete_table(args);
    else if (args.command == Command::EDIT)   edit_table(args);
    else if (args.command == Command::UPDATE) update_rows(args);
    else if (args.command == Command::INSERT) insert_row(args);
    else if (args.command == Command::QUERY)  query_rows(args);

    return 0;
}
