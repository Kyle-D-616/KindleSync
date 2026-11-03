#!/bin/bash
cc -Wall -Wextra -g -fsanitize=address,undefined src/main.c -o kindlesync
