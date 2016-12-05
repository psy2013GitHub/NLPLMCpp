
#include <iostream>
#include <cassert>
#include <cstdlib>

#define align8(x) ((std::ptrdiff_t(((x)-1)/8)+1)*8) 

int main(int argc, char** argv) {
    assert(argc==2);
    int x = atoi(argv[1]);
    std::cout << x << " " << align8(x) << std::endl; 
}
