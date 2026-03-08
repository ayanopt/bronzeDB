#include "utils.h"
#include "args.h"
#include "create_table.h"
#include "delete_table.h"
#include "edit_table.h"
#include "insert_row.h"
#include "query_table.h"
#include "update_table.h"
int main(int argc, char ** argv) {
    /*
    ./bronzedb --create monkeys
        --serverless false
        --fields '{monkey_id:string, species:string, age:int, fur_color:string, is_rabid:int, tail_length:double|null, metadata:string, contract_expiry:int|null}'
        --ttl contract_expiry
        --primary-key '{monkey_id}'                               # alternatively for composite pk '{tail_length, species}'
        --sort-key age                                            # optional, default none
        --add-index rabid_species
        --add-index-pk rabid_species '{species, is_rabid}'        # required if index provided, asserts pk exists
        --add-index-sk rabid_species '{tail_length}'              # optional, default none
        --add-index-projection rabid_species '{monkey_id:string}' # optional, default all fields

    ./bronzedb --delete monkeys

    ./bronzedb --edit monkeys
        --add-fields '{sex:string}'                               # throws on field name collision
        --remove-fields '{fur_color}'                             # throws on primary key deletion, or index pk deletion
        --delete-index                                            # throws on non existent index
        --add-index ...                                           # throws on index name collision
        --serverless true
    
    ./bronzedb --update monkeys
        --at '{monkey_id:=:"monkey1"}'                            # throws if not pk
        --use-index rabid_species
        --at '{species:=:"lemur", is_rabid:=:1}'                  # throws if not index pk

    ./bronzedb --insert monkeys
        --data '{
            "monkey_id": "monkey1",
            "species": "lemur",
            "age": 4,
            "fur_color": "grey",
            "is_rabid": 1,
            "tail_length": 4.2,
            "metadata": '{
                "children": 8,
                "name": "gerald",
                "favorite_food": "biscuits and gravy"
            }'
        }'
    ./bronzedb --query monkeys
        --query-condition '{monkey_id:=:"monkey1"}'               # throws on non existent field
        --query-condition '{age:>:2}'
        --query-condition '{age:<=:4}'
        --use-index rabid_species                                 # throws on non existent index
        --query-condition '{species:=:"lemur", is_rabid:=:1}'     # specify all fields in an index pk, if use-index is provided
        --output-fields '{monkey_id, fur_color}'                  # default all, specify fields to output in query (throws on non existent field in database or index projection)
    */

    ParsedArgs args = parse_args(argc, argv);
    if (args.command == Command::NONE) return 1;
    if (!validate_args(args)) return 1;

    if (args.command == Command::CREATE) create_table(args);
    if (args.command == Command::DELETE) delete_table(args);
    if (args.command == Command::EDIT) edit_table(args);
    if (args.command == Command::INSERT) insert_row(args);
    if (args.command == Command::QUERY) query_table(args);
    if (args.command == Command::UPDATE) update_table(args);


    return 0;
}