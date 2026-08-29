#ifndef PORT_H
#define PORT_H
//#include "packet_struct.h"
#include <fstream>

class Port{
public:
    Port(){

    }
    uint8_t* get(){
        readFile();
        return buffer;
    }
    void send(uint8_t* pkt){
        writeFile(pkt);
    }
private:
    int fd;
    uint8_t buffer[1518];
    
    uint8_t* readFile(){
//fd apparently not in c++? maybe make this file c?
    }
    void writeFile(uint8_t* pkt){

    }

};

#endif