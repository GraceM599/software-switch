#ifndef RING_BUFFER
#define RING_BUFFER

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string.h>

#define BUFFER_LEN 400000
class ringBuffer{
    
public:
   ringBuffer(std::string file_name);
   bool addEntry(std::string log);

   void writeEntries();
   ~ringBuffer();
private:
   size_t head;
   size_t tail;
   char buffer[BUFFER_LEN];
   std::ofstream log_file;
};
#endif