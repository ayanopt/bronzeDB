#include <iostream>
#include <any>
#include <vector>
using namespace std;

void print(string s) {
    cout << s << endl;
}

void print(vector<any> v) {
    for (auto &elt: v) {
        if (elt.type() == typeid(int))         cout << any_cast<int>(elt);
          else if (elt.type() == typeid(double))  cout <<
  any_cast<double>(elt);
          else if (elt.type() == typeid(string)) cout <<
  any_cast<string>(elt);
          else if (elt.type() == typeid(char))    cout << any_cast<char>(elt);
          else if (elt.type() == typeid(size_t))  cout <<
  any_cast<size_t>(elt);
    }
    cout << endl;
}

