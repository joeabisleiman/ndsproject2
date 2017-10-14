cmake_minimum_required(VERSION 3.8)
project(P1_skeleton)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        gbn.c
        gbn.h
        Makefile
        receiver.c
        sender.c)

add_executable(P1_skeleton ${SOURCE_FILES})