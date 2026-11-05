#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>


class Monitor{

public:
    Monitor(){};
    Monitor(const char* inf){
        fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (fd < 0){
            std::cerr << "Error creating socket in Monitor";
            return;
        }
        int intf_index = if_nametoindex(inf);
        if (intf_index == 0){
            std::cerr << "Error resolving monitor interface name";
            close(fd);
            return;
        }

        sockaddr_ll sa{};
        sa.sll_family = AF_PACKET;
        sa.sll_protocol = htons(ETH_P_ALL);
        sa.sll_ifindex = intf_index;

        int err = bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
        if (err == -1){
            //error
            close(fd);
            std::cerr << "Error binding to monitor interface";
        }
        std::cout << "Monitor up and running\n";

        return;
    }
    ssize_t write(uint8_t* pkt, size_t len){
        ssize_t bytes_sent = send(fd, pkt, len, 0);
        if (bytes_sent < 0){
            return -1;
        }

        if (bytes_sent != len){
            errno = EIO;
            return -1;
        }
        return bytes_sent;
    }


    ~Monitor(){
        close(fd);
    }
private:
    int fd = -1;
};