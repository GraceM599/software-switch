#ifndef RING_LOGGER_H
#define RING_LOGGER_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <array>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <string.h>
#include "ring_buffer.h"
#include <thread>
#include <memory>
#define BUFFER_SIZE 1024


struct log_msg{
   std::array<char, 256> msg{};
   size_t len = 0;
};


class ringLogger {
    
public:
   explicit ringLogger(const std::string& file_name = "logs/switch_log") : ring(200){
      log_file.open(file_name);
      if (!log_file) {
         std::cerr << "Error opening log file: " << file_name << std::endl;
      }
      writer = std::thread(&ringLogger::writerLoop, this);
   }
   ~ringLogger(){
      running.store(false);
      ring.notify_all();
      if (writer.joinable()) {
         writer.join();
      }
      log_msg m{};
      while (ring.try_pop(m)) {
         log_file.write(m.msg.data(), m.len);
      }
      log_file.flush();
      log_file.close();
   }
  ringLogger(const ringLogger&) = delete;
  ringLogger& operator=(const ringLogger&) = delete;
   bool addEntry(const std::string& log){
      log_msg m{};
      m.len = std::min(log.size(), m.msg.size());
      std::memcpy(m.msg.data(), log.data(), m.len);
      return ring.push(m);
   }

private:
   ringBuffer<log_msg, BUFFER_SIZE> ring;
   std::ofstream log_file;
   std::atomic<bool> running{true};
   std::thread writer;
   void writerLoop(){
      log_msg m{};
      while (running.load()) {
         if (ring.wait_and_pop(m, running, std::chrono::milliseconds(500))) {
               log_file.write(m.msg.data(), m.len);
               while (ring.try_pop(m)) {
                  log_file.write(m.msg.data(), m.len);
               }
               log_file.flush();
         }
      }
   }


};
#endif