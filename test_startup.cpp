#include "src/engine.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <iostream>
#include <cassert>
int main(int argc, char** argv) {
    if(argc != 2) { std::cerr << "usage: test_startup uncompressed.vgm\n"; return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if(!f) return 2;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    MegaDriveEngine e; std::string error;
    if(!e.load(b,error)) { std::cerr << error << "\n"; return 1; }
    std::vector<int16_t> block(2*512);
    e.render(block.data(),512);
    std::cout << "time-zero YM writes: " << e.initialPrimeWrites() << "\n";
    std::cout << "YM prime clock cycles: " << e.initialPrimeClocks() << "\n";
    std::cout << "VGM position after render: " << e.position() << " (expected 512)\n";
    if(!e.initialPrimed() || e.position()!=512 || e.initialPrimeClocks()>100000) return 1;
    // A file with a sizeable time-zero burst must be initialized at start.
    if(e.initialPrimeWrites() < 100 || e.initialPrimeClocks() < 1000) return 1;
    std::cout << "STARTUP TEST PASSED\n";
}
