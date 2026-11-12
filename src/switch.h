#pragma once
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




class Switch{
public:
Switch(std::vector<std::unique_ptr<Port>> portList){
    //basically to make a switch all that it needs is a config filename and it will set everything else up
    //makes it easy to make multipel switches

}
void start();
void stop();

void show_ports();
void show_vlans();


private:
std::vector<std::unique_ptr<Port>> ports;
MacTable mac_table;
Monitor monitor;

};
//config
//dataplane -> function ptr to processPackets function in use