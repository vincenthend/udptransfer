#!/bin/bash

#make
time ./recvfile "out.png" 1500 4500 1337 & ./sendfile "data/test.png" 1500 4500 127.0.0.1 1337