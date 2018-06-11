cmake_minimum_required(VERSION 3.9.6)
project(zad1 C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -pthread")

add_executable(zad1 structures2.h main2.c)
#add_executable(zad1 main.c structures.h client.c main2.c client2.c structures2.h)