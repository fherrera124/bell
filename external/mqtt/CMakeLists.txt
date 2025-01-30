cmake_minimum_required(VERSION 3.16)
project(mqtt)

# Add the library target
add_library(mqtt STATIC mqtt.c mqtt_pal.c)
target_include_directories(mqtt PUBLIC "include")