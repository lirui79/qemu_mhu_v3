/*
 *  device_memleak_detection.cpp
 *  Author: zhangbo
 *  Generated: 2024-12-18 14:48:29.332000
 *  ----------------------------------------------------------
 *  This file is used to detect memory leak in device side for every test case.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "catch2/catch.hpp"

void runDetection(bool isBefore);
