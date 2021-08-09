#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

void fun1(void) { std::cout << "inside fun1\n"; }

int fun2() {
  std::cout << "inside fun2\n";
  return 2;
}

int fun3(int a) {
  std::cout << "inside fun3\n";
  return a;
}

std::vector<int> fun4() {
  std::cout << "inside fun4\n";
  std::vector<int> v(4, 100);
  return v;
}

// every function pointer will be stored as this type
typedef void (*voidFunctionType)(void);

struct Interface {
  std::map<std::string, std::pair<voidFunctionType, std::type_index>> m1;

  template <typename T>
  void insert(std::string s1, T f1) {
    auto tt = std::type_index(typeid(f1));
    m1.insert(std::make_pair(s1, std::make_pair((voidFunctionType)f1, tt)));
  }

  template <typename T, typename... Args>
  T searchAndCall(std::string s1, Args&&... args) {
    auto mapIter = m1.find(s1);
    /*chk if not end*/
    auto mapVal = mapIter->second;

    // auto typeCastedFun = reinterpret_cast<T(*)(Args ...)>(mapVal.first);
    auto typeCastedFun = (T(*)(Args...))(mapVal.first);

    // compare the types is equal or not
    assert(mapVal.second == std::type_index(typeid(typeCastedFun)));
    return typeCastedFun(std::forward<Args>(args)...);
  }
};

int main() {
  Interface a1;
  a1.insert("fun1", fun1);
  a1.insert("fun2", fun2);
  a1.insert("fun3", fun3);
  a1.insert("fun4", fun4);

  a1.searchAndCall<void>("fun1");
  int retVal = a1.searchAndCall<int>("fun3", 2);
  a1.searchAndCall<int>("fun2");
  auto temp = a1.searchAndCall<std::vector<int>>("fun4");

  return 0;
}