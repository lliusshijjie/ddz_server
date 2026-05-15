#include <iostream>
#include <string>

#include "gateway/gateway_server.h"

int main(int argc, char** argv) {
    std::string config_path = "config/dev/gateway.yaml";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const std::string prefix = "--config=";
        if (arg.rfind(prefix, 0) == 0) {
            config_path = arg.substr(prefix.size());
        }
    }

    ddz::GatewayServer server;
    std::string err;
    if (!server.Start(config_path, &err)) {
        std::cerr << "failed to start gateway: " << err << std::endl;
        return 1;
    }

    std::cout << "ddz_gateway running with config: " << config_path << std::endl;
    std::cout << "press ENTER to stop..." << std::endl;
    std::string line;
    std::getline(std::cin, line);

    server.Stop();
    return 0;
}
