// a mini example for gromacs:
// mimic the use of map and indirect function pointers, the following links are references

#include <iostream>
#include <map>
#include <memory>
#include <string>

// base class
class ICommandLineModule {
  // https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodule.h#L93
 public:
  virtual ~ICommandLineModule() {}

  // Returns the name of the module.
  virtual const char* name() const = 0;
  // Runs the module with the given arguments
  virtual int run(int argc, char* argv[]) = 0;
};

class Impl {
  const char* name_;

 public:
  Impl(const char* name);

  void doSomething(int num);
};

Impl::Impl(const char* name) : name_(name) {}

void Impl::doSomething(int num) { std::cout << "This is running from CommandAImpl: " << num; }

// inherent classes
class CommandA : public ICommandLineModule {
  // https://github.com/gromacs/gromacs/blob/e0039f23b58a0ea3c15a5b3775525ad5b06f4e30/src/gromacs/commandline/cmdlinehelpmodule.cpp#L883
  // https://github.com/gromacs/gromacs/blob/e0039f23b58a0ea3c15a5b3775525ad5b06f4e30/src/gromacs/commandline/cmdlinehelpmodule.cpp#L848
  const char* name_;

 public:
  CommandA(const char* name);
  ~CommandA() override;

  const char* name() const override { return name_; }
  int run(int argc, char* argv[]) override;

 private:
  std::unique_ptr<Impl> impl_;
};

CommandA::CommandA(const char* name) : impl_(new Impl(name)) {}

int CommandA::run(int argc, char* argv[]) {
  impl_->doSomething(argc);
  return 0;
}

class CommandB : public ICommandLineModule {
  // https://github.com/gromacs/gromacs/blob/e0039f23b58a0ea3c15a5b3775525ad5b06f4e30/src/gromacs/commandline/cmdlineoptionsmodule.cpp#L107
  const char* name_;

 public:
  explicit CommandB(const char* name) : name_(name) {}

  const char* name() const override { return name_; }
  int run(int argc, char* argv[]) override {
    std::cout << "This is running from CommandB." << argv;
    parseOption(argc, argv);
    return 0;
  }

 private:
  void parseOption(int argc, char* argv[]) { std::cout << "This is option: " << argv; }
};

class CommandC : public ICommandLineModule {
  // https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/programs/legacymodules.cpp#L85
  const char* name_;

 public:
  explicit CommandC(const char* name) : name_(name) {}

  const char* name() const override { return name_; }
  int run(int argc, char* argv[]) override {
    printMsg(argc, argv);
    return 0;
  }

 private:
  void printMsg(int argc, char* argv[]) { std::cout << "This is running from CommandC." << argv; }
};

typedef std::unique_ptr<ICommandLineModule>
    CommandLineModulePointer;  // https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodulemanager.h#L65
typedef std::map<std::string, CommandLineModulePointer>
    CommandLineModuleMap;  // https://github.com/gromacs/gromacs/blob/e0039f23b58a0ea3c15a5b3775525ad5b06f4e30/src/gromacs/commandline/cmdlinemodulemanager_impl.h#L64

int main(int argc, char* argv[]) {
  // https://github.com/gromacs/gromacs/blob/8dd21f717c82277c611b537d92c19348ded5b357/src/programs/gmx.cpp
  CommandLineModuleMap modules;
  auto module_a = std::make_unique<CommandA>("CommandA");
  auto module_b = std::make_unique<CommandB>("CommandB");
  auto module_c = std::make_unique<CommandC>("CommandC");

  // https://github.com/gromacs/gromacs/blob/c18c5072ef8d5a50da253d37b6ad33dc550e9514/src/gromacs/commandline/cmdlinemodulemanager.cpp#L326
  modules.insert(std::make_pair(std::string(module_a->name()), std::move(module_a)));
  modules.insert(std::make_pair(std::string(module_b->name()), std::move(module_b)));
  modules.insert(std::make_pair(std::string(module_c->name()), std::move(module_c)));

  auto it = modules.find(argv[0]);
  if (it == modules.end()) return -1;

  it->second->run(argc, argv);
}