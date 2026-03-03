#include "utils.h"

int main(int argc, char ** argv) {
    /*
    ./bronzedb --create monkeys
        --fields {monkey_id:string, species:string, age:int, fur_color:string, is_rabid:int, tail_length:double, metadata:string}
        --primary-key {monkey_id}                               # alternatively for composite pk {tail_length, species}
        --sort-key age                                          # optional, default none
        --add-index rabid_species
        --add-index-pk rabid_species {species, is_rabid}        # required if index provided, asserts pk exists
        --add-index-sk rabid_species {tail_length}              # optional, default none
        --add-index-projection rabid_species {monkey_id:string} # optional, default all fields

    ./bronzedb --delete monkeys

    ./bronzedb --update monkeys
        --add-fields {sex:string}                               # throws on field name collision
        --remove-fields {fur_color}                             # throws on primary key deletion, or index pk deletion
        --delete-index                                          # throws on non existent index
        --add-index ...                                         # throws on index name collision
    
    ./bronzedb --insert monkeys
        --data {
            "monkey_id": "monkey1",
            "species": "lemur",
            "age": 4,
            "fur_color": "grey",
            "is_rabid": 1,
            "tail_length": 4.2,
            "metadata": {
                "children": 8,
                "name": "gerald",
                "favorite_food": "biscuits and gravy"
            }
        }
    ./bronzedb --query monkeys
        --query-condition {monkey_id:=:"monkey1"}               # throws on non existent field
        --query-condition {age:>:2}
        --query-condition {age:<=:4}                        
        --use-index rabid_species                               # throws on non existent index
        --query-condition {species:=:"lemur", is_rabid:=:1}     # specify all fields in an index pk, if use-index is provided
        --output-fields {monkey_id, fur_color}                  # specify fields to output in query (throws on non existent field in database or index projection)
    */
    return 0;
}