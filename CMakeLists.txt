cmake_minimum_required(VERSION 3.10)
project(CLION)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(CLION
        config.h
        dplist.c
        dplist.h
        tcpsock.c
        tcpsock.h
        connmgr.c main.c connmgr.h)
