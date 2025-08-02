set shell := ["powershell.exe"]

output := "build/netease-lyric-fetcher.exe"
res_output := "build/resource.res"

alias b := build

alias d := dev

alias c := clean

build:
    windres src/resource.rc -O coff -o {{res_output}}
    gcc -o {{output}} src/main.c src/utils.c src/tools.c src/weapi.c {{res_output}} -lwininet -lcomctl32 -lshell32 -lole32 -mwindows

dev:
    just b
    ./{{output}} debug

clean:
    rm -f {{output}}
