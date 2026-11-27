#!/bin/bash

g++ -std=c++2a -O0 -g -DNDEBUG -Wshadow src/*.cpp -pthread -o build/switch
