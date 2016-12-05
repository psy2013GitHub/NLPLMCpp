
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

uint64_t ReadCount(const std::string &from) {
  std::stringstream stream(from);
  uint64_t ret;
  stream >> ret;
  return ret;
}


int main(int argc, char** argv) {
   assert(argc==2);
   std::cout << argv[1] << " " << ReadCount(argv[1]) << std::endl;
}
