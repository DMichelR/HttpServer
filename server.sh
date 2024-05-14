#!/bin/sh

gcc -o ./out ./main.c

exec ./out "$@"