#!/usr/bin/env fish
python compiler/main.py $argv[1] > build/out.c; or exit 1
cc -O2 -Iruntime build/out.c runtime/rt.c -lpthread -o build/prog; or exit
./build/prog
