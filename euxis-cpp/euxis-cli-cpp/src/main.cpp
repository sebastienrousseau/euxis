#include "euxis/cli/engine.hpp"

#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    const char* home = std::getenv("EUXIS_HOME");
    euxis::cli::Engine engine(home ? home : "");
    return engine.run(args);
}
