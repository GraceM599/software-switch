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
#include <array>
#include "packet_struct.cpp"
#include <atomic>

enum class AcceptedFrames{
    None, Ethernet, ARP, IPv4, IPv6, All
};

class Monitor{

public:
    
    Monitor(){};
    Monitor(const char* inf, AcceptedFrames frames, int sample_int){
        protocol = frames;
        this->sample_int = sample_int;
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
    ssize_t mirror_pkt(uint8_t* pkt, size_t len){
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
    ssize_t write(uint8_t* pkt, size_t len){
        if (protocol == AcceptedFrames::None){
            return 0;
        }
        if (sample_int == 0){
            return 0;
        }
        const EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(pkt);
        uint16_t ether_type = ntohs(frame->eth_type);
        bool match = false;
        switch (protocol){
            case AcceptedFrames::None:
                return 0;
            case AcceptedFrames::Ethernet:
                match = true;
            case AcceptedFrames::IPv4:
                match = (ether_type == ETH_P_IP);
            case AcceptedFrames::IPv6:
                match = (ether_type == ETH_P_IPV6);
            case AcceptedFrames::ARP:
                match = (ether_type == ETH_P_ARP);
        }
        if (!match){
            return 0;
        }

        int s_count = pkt_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (s_count != 0){
            return 0;
        }
        return mirror_pkt(pkt, len);

    }


    ~Monitor(){
        close(fd);
    }
private:
    int fd = -1;
    AcceptedFrames protocol = AcceptedFrames::None; //default none
    int sample_int = 0; //default none
    std::atomic<int> pkt_count {0};
};