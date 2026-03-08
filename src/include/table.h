#include <map>
#include "field.h"
#include <vector>
#include <unordered_map>
#include <string>

using namespace std;
class Table {
    public:
        string name;
        bool isServerless;
        vector<Field> fields;
};