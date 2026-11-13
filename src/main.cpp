#include <iostream>
#include <cstring>
#include <cerrno>
#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 
#include "switch_monitor.h"
#include <poll.h>
#include <vector>
#include <memory>
#include <functional>
#include <csignal>
#include <thread>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "dp.h"
#include "switch.h"
#include "manager.h"
//TODO: finish switch.h
//start management plane?
//link state stuff or at least make ways to add it
//look into unessesary copies

std::atomic<bool> continue_running{true};

void handle_signal(int signal_num){
    if (signal_num == SIGTERM || signal_num == SIGINT){
        continue_running = 0;
    }
    return;
}

int main(int argc, char** argv) {
    std::cout << "Switch main() starting" << std::endl;

    if (argc < 2){
        std::cout << "Need to specify switch config file\n";
        return 1;
    }
    std::string config = argv[1];
    Switch s{config, continue_running, argv};

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    
    Manager m{s};
    
    

    return 0;

    
}