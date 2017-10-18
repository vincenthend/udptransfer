#!/bin/bash

make
time ./sendfile "tc2.txt" 1500 4500 127.0.0.1 1337 & ./recvfile out.txt 1500 4500 1337