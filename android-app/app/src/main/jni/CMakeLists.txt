cmake_minimum_required(VERSION 3.18)
project("rs485helper")

add_library(rs485helper SHARED rs485_helper.c)
target_link_libraries(rs485helper log)
