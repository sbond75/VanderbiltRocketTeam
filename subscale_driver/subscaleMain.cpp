#include <python3.7m/Python.h>
#include "VADL2022.hpp"

int main(int argc, char **argv) {
  VADL2022 vadl(argc, argv);
  PyRun_SimpleString("input('Done')");
  return 0;
}