// a mini example for gromacs:
// mimic the use of map and indirect function pointers, the following links are references

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "gmx_ana.h"

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

// Function pointer type for a C main function.
// https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodulemanager.h#L98
typedef int (*CMainFunction)(int argc, char* argv[]);

// https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodulemanager.h#L65
typedef std::unique_ptr<ICommandLineModule> CommandLineModulePointer;

// inherent classes
class CMainCommand : public ICommandLineModule {
  // https://github.com/gromacs/gromacs/blob/c18c5072ef8d5a50da253d37b6ad33dc550e9514/src/gromacs/commandline/cmdlinemodulemanager.cpp#L89
 public:
  typedef CMainFunction CMainFunction;

  /*!
   * Creates a wrapper module for the given main function.
   * \param[in] name             Name for the module.
   * \param[in] mainFunction     Main function to wrap.
   */
  CMainCommand(const char* name, CMainFunction mainFunction) : name_(name), mainFunction_(mainFunction) {}

  const char* name() const override { return name_; }
  int run(int argc, char* argv[]) override { return mainFunction_(argc, argv); }

 private:
  const char* name_;
  CMainFunction mainFunction_;
};

typedef std::unique_ptr<ICommandLineModule>
    CommandLineModulePointer;  // https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodulemanager.h#L65
typedef std::map<std::string, CommandLineModulePointer>
    CommandLineModuleMap;  // https://github.com/gromacs/gromacs/blob/e0039f23b58a0ea3c15a5b3775525ad5b06f4e30/src/gromacs/commandline/cmdlinemodulemanager_impl.h#L64

class CommandLineModuleManager {
  // https://github.com/gromacs/gromacs/blob/6fff49f17a7a716451919691d689b0d621b981fe/src/gromacs/commandline/cmdlinemodulemanager.h#L94

  CommandLineModuleMap modules;

 public:
  CommandLineModuleManager();

  void addModuleMain(const char* name, CMainFunction mainFunction) {
    // https://github.com/gromacs/gromacs/blob/c18c5072ef8d5a50da253d37b6ad33dc550e9514/src/gromacs/commandline/cmdlinemodulemanager.cpp#L468
    CommandLineModulePointer module(new CMainCommand(name, mainFunction));
    modules.insert(std::make_pair(std::string(name), std::move(module)));
  }

  void runAsMain(int argc, char* argv[]) {
    // https://github.com/gromacs/gromacs/blob/c18c5072ef8d5a50da253d37b6ad33dc550e9514/src/gromacs/commandline/cmdlinemodulemanager.cpp#L600
    auto it = modules.find(argv[0]);
    if (it == modules.end()) return;

    it->second->run(argc, argv);
  }
};

int main(int argc, char* argv[]) {
  // https://github.com/gromacs/gromacs/blob/8dd21f717c82277c611b537d92c19348ded5b357/src/programs/gmx.cpp
  CommandLineModuleManager manager;

  manager.addModuleMain("chi", &gmx_chi);
  manager.addModuleMain("bar", &gmx_bar);
  manager.addModuleMain("bundle", &gmx_bundle);

  manager.runAsMain(argc, argv);
}