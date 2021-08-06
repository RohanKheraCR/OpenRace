// https://github.com/gromacs/gromacs/blob/df463c2cf9e1ee786b2f6e224e5168933c378ecb/src/gromacs/gmxana/gmx_chi.cpp

#include "gmx_ana.h"

int gmx_chi(int argc, char* argv[]) {
  std::cout << "This is running from gmx_chi." << argv;
  return argc;
}