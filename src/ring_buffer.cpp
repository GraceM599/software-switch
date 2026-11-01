#include "ring_buffer.h"

ringBuffer::ringBuffer(std::string file_name = "logs/switch_log"){
      head = 0;
      tail = 0;

      log_file.open(file_name.c_str());

      if (!log_file){
         std::cerr << "Error opening log file: " << file_name << std::endl;
      }

      return;
}

bool ringBuffer::addEntry(std::string log){
    if(tail + log.size() > BUFFER_LEN){
        //writing would overwrite other data
        writeEntries();
    }
    memcpy(buffer + tail, log.c_str(), log.size());
    tail += log.size();
    return true;
}

void ringBuffer::writeEntries(){
    log_file.write(buffer, tail-head);
    log_file.flush();
    head = 0;
    tail = 0;

}
ringBuffer::~ringBuffer(){
    writeEntries();
    log_file.close();
}