#!/bin/bash

g++ -std=c++2a -fsanitize=thread -g -O1 -DNDEBUG src/*.cpp -o build/switch
