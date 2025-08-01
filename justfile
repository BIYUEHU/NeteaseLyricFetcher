set shell := ["powershell.exe"]

output := "build/netease_lyric.exe"

alias b := build

alias c := clean

build:
    gcc -o {{output}} src/main.c src/utils.c src/weapi.c -lwininet -lcomctl32 -lshell32 -lole32 -mwindows
    ./{{output}} debug

clean:
    rm -f {{output}}
