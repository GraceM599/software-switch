#ifndef PORT_H
#define PORT_H
//#include "packet_struct.h"
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>    
#include <linux/if.h>     
#include <linux/if_tun.h> 
#include <cstring> 
class Port{
public:
    Port(std::string interface_name){
        fd = open("/dev/net/tun", O_RDWR);
        if(fd < 0){
            std::cerr << "Error opening /dev/net/tun." << std::endl;
            return;
        }
        struct ifreq ifr;
        std::memset(&ifr, 0,  sizeof(ifr));
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

        std::strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ);

        if (ioctl(fd, TUNSETIFF, (void*)&ifr) < 0) {
            std::cerr << "Error running ioctl TUNSETIFF on " << interface_name << std::endl;
            close(fd);
            fd = -1;
        }

    }
    ssize_t get(uint8_t* out_buffer){
        ssize_t bytes_read = read(fd, out_buffer, 1518);
        return bytes_read;
    }
    void send(uint8_t* pkt, size_t length){
        std::cout << "Port successfully transmitted packet starting with byte: " 
              << std::hex << (int)pkt[0] << std::endl;
        write(fd, pkt, length);
        return;
    }
    ~Port(){
        close(fd);
    }
private:
    int fd;
    uint8_t buffer[1518];

};

#endif