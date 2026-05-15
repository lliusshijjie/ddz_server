#include <iostream>
#include <string>

#include "server/game_server.h"

int main(int argc, char** argv) {
    std::string config_path = "config/dev/server.yaml";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const std::string prefix = "--config=";
        if (arg.rfind(prefix, 0) == 0) {
            config_path = arg.substr(prefix.size());
        }
    }

    ddz::GameServer server;
    std::string err;
    if (!server.Start(config_path, &err)) {
        std::cerr << "failed to start server: " << err << std::endl;
        return 1;
    }

    std::cout << "ddz_server running with config: " << config_path << std::endl;
    std::cout << "press ENTER to stop..." << std::endl;
    std::string line;
    std::getline(std::cin, line);

    server.Stop();
    return 0;
}

