@echo off

REM SPIRV
..\..\tools\msvc\shaderc --platform windows -p spirv --type vertex -o spirv\basic.vs.bin -f basic_vs.sc
..\..\tools\msvc\shaderc --platform windows -p spirv --type fragment -o spirv\basic.fs.bin -f basic_fs.sc
..\..\tools\msvc\shaderc --platform windows -p spirv --type vertex -o spirv\terrain.vs.bin -f terrain.vs.sc
..\..\tools\msvc\shaderc --platform windows -p spirv --type fragment -o spirv\terrain.fs.bin -f terrain.fs.sc

REM DirectX 9
..\..\tools\msvc\shaderc --platform windows -p vs_3_0 --type vertex -o dx9\basic.vs.bin -f basic_vs.sc
..\..\tools\msvc\shaderc --platform windows -p ps_3_0 --type fragment -o dx9\basic.fs.bin -f basic_fs.sc
..\..\tools\msvc\shaderc --platform windows -p vs_3_0 --type vertex -o dx9\terrain.vs.bin -f terrain.vs.sc
..\..\tools\msvc\shaderc --platform windows -p ps_3_0 --type fragment -o dx9\terrain.fs.bin -f terrain.fs.sc

REM DirectX 11
..\..\tools\msvc\shaderc --platform windows -p vs_5_0 --type vertex -o dx11\basic.vs.bin -f basic_vs.sc
..\..\tools\msvc\shaderc --platform windows -p ps_5_0 --type fragment -o dx11\basic.fs.bin -f basic_fs.sc
..\..\tools\msvc\shaderc --platform windows -p vs_5_0 --type vertex -o dx11\terrain.vs.bin -f terrain.vs.sc
..\..\tools\msvc\shaderc --platform windows -p ps_5_0 --type fragment -o dx11\terrain.fs.bin -f terrain.fs.sc
