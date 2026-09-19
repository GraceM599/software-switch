#pragma once
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string.h>

#define BUFFER_LEN 2000
class ringBuffer{
    
public:
   ringBuffer(){
      head = 0;
      tail = 0;

      std::string file_name = "logs/switch_log";
      log_file.open(file_name.c_str());

      if (!log_file){
         std::cerr << "Error opening log file: " << file_name << std::endl;
      }


      return;
   }
   bool addEntry(std::string log){
      if(tail + log.size() > BUFFER_LEN){
         //writing would overwrite other data
         //figure out a way to resolve this for now return after clearing
         writeEntries();
      }
      memcpy(buffer + tail, log.c_str(), log.size());
      tail += log.size();
      return true;
   }

   void writeEntries(){
      log_file.write(buffer, tail-head);
      head = 0;
      tail = 0;

   }
   ~ringBuffer(){
      writeEntries();
      log_file.close();
   }
private:
   size_t head;
   size_t tail;
   char buffer[BUFFER_LEN];
   std::ofstream log_file;
};