#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

using namespace std;

enum class FieldType { INT, DOUBLE, STRING };

struct FieldDef {
    string name;
    FieldType type;
    bool nullable = false; // true when declared as type|null
};

template <typename T>
class Field {
    private:
        string name;
        bool nullable;
        vector<optional<T>> entries;
    public:
        Field(string &fieldName, bool isNullable = false)
            : name(fieldName), nullable(isNullable) {};
        string get_name() { return name; }
        bool is_nullable() { return nullable; }
        void add_entry(optional<T> value) { entries.push_back(value); }
};