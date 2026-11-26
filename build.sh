#!/bin/bash
g++ -std=c++2a -O3 -g -DNDEBUG -Wshadow src/*.cpp -pthread -o build/switch
