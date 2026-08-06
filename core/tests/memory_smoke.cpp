#define WIN32_LEAN_AND_MEAN
#include "../src/memory/game_memory.h"
#include "../src/memory/hl_runtime.h"

#include <cstring>
#include <iostream>

int main() {
    int source = 0x13579;
    int copy = 0;
    if (!fmk::mem_read(&source, &copy, sizeof(copy)) || copy != source) {
        std::cerr << "validated local read failed\n";
        return 1;
    }
    if (fmk::mem_read(nullptr, &copy, sizeof(copy))) {
        std::cerr << "invalid address was accepted\n";
        return 1;
    }

    fmk::GameMemory memory;
    if (memory.available()) {
        std::cerr << "memory unexpectedly available outside the game\n";
        return 1;
    }
    if (memory.configure_build_hash("not-a-real-build-hash")) {
        std::cerr << "invalid build hash was accepted\\n";
        return 1;
    }
    if (!memory.configure_build_hash(fmk::GameMemory::expected_build_hash())) {
        std::cerr << "known build hash was rejected\\n";
        return 1;
    }
    memory.reset();

    if (memory.probe(false)) {
        std::cerr << "unvalidated probe was accepted\\n";
        return 1;
    }

    memory.reset();
    const auto status = memory.status();
    if (status.app_found || status.hero_found || status.in_world) {
        std::cerr << "reset status is not empty\n";
        return 1;
    }

    std::cout << "Memory core smoke test passed (read-only, no game scan)\n";
    return 0;
}
