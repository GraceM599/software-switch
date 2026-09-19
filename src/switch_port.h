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
#include <cerrno>
#include "ring_buffer.h"
class Port{
public:
    Port(){
        //empty
    }
    Port(std::string interface_name){
        interface = interface_name;
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
            std::cerr << "Error running ioctl TUNSETIFF on "
              << interface_name
              << ": "
              << std::strerror(errno)
              << std::endl;
            close(fd);
            fd = -1;
        }

    }
    ssize_t get(uint8_t* out_buffer){
        ssize_t bytes_read = read(fd, out_buffer, 1518);
        std::string log = interface + " recieved packet" + "\n";
        logger.addEntry(log); 

        return bytes_read;
    }
    void send(uint8_t* pkt, size_t length){
        std::string log = interface + "sent packet with length " + std::to_string(length) + "\n";
        logger.addEntry(log); 
        write(fd, pkt, length);
        return;
    }
    int get_fd(){
        return fd;
    }
    ~Port(){
        close(fd);
    }
private:
    int fd;
    ringBuffer logger;
    std::string interface;

};

#endif