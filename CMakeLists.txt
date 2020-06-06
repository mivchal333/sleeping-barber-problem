cmake_minimum_required(VERSION 3.16)
project(sleeping_barber_problem)

set(CMAKE_CXX_STANDARD 14)

add_executable(sleeping_barber_problem
    condition_variables/main.c
    condition_variables/Makefile
    mutex_semafor/main.c
    mutex_semafor/Makefile)
