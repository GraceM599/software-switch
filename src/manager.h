#include "switch.h"

class Manager{
    public:
    Manager(Switch& s){
        this->s = &s;
    }
    void start_terminal(){
        int pid = fork();
        if (pid == 0){
            //child

        }
    }

private:
    Switch* s;
};