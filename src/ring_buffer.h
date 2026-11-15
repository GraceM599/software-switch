#pragma once

#ifndef RING_BUFFER
#define RING_BUFFER
#include <array>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <atomic>

template <typename T, size_t N> 
class ringBuffer{
    
public:
   ringBuffer(size_t t = 1){
        threshold = t;
   };
   bool push(T item){
        std::lock_guard<std::mutex> lock(mtx);
        if (count == N){
            return false;
        }
        buffer[tail] = std::move(item);
        tail = (tail + 1) % N;
        ++count;
        if (count >= threshold){
            cv.notify_one();
        }
        return true;

   }
   bool try_pop(T& out){
        std::lock_guard<std::mutex> lock(mtx);
        if (count == 0){
            return false;
        }
        out = std::move(buffer[head]);
        head = (head + 1) % N;
        count--;
        return true;

   }
   bool wait_and_pop(T& out, const std::atomic<bool>& running, 
            std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, timeout, [&] {return count > 0|| !running.load();});

        if (count == 0){
            return false;
        }
        out = std::move(buffer[head]);
        head = (head + 1) % N;
        count--;
        return true;

    }
    void notify_all(){
        cv.notify_all();
    }

    size_t size() const{
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }
    bool empty() const{
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0;
    }
   ~ringBuffer() = default;
private:
   size_t head = 0;
   size_t tail = 0;
   std::array<T, N> buffer;
   size_t count = 0;
   mutable std::mutex mtx;
   size_t threshold;
   std::condition_variable cv;

};
#endif