#include <cstdint>
#include <cstddef>


class Monitor{

public:
    Monitor(char* inf){
        return;
    }
    int write(uint8_t* pkt, size_t len){

    }


    ~Monitor(){

    }
private:
    int fd = -1;
};