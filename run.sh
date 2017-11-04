#!/bin/bash

make
time ./recvfile "out.txt" 1500 4500 1337 & ./sendfile "data/tc4.txt" 1500 4500 127.0.0.1 1337