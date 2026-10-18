# ACE Engine for DOOM 2

This is a DOS Doom2 enhancement contained in a single WAD file.

## Version

This code only works in Doom 2 version 1.9.
That is, no The Ultimate Doom, no The Final Doom, no Anthology.
The same version is distributed with SHAREWARE.

(Why are there so many different 1.9 versions?)

## How to run this

Compile `loader` and `engine`.
Engine is the main thing. After compiling you get `code.lmp`.
Loader generates a WAD file with multiple entries. Replace `ACE_CODE` with generated `code.lmp` in any WAD editor.

Resulting WAD file is run just like any other WAD, using command `doom2 -file loader.wad`.

## Documentation

[read here](doc/main.md)

## Code

Code is split into two distinct parts.

### Loader

Two engine quirks are used. First is negative-size lump allocation in ZONE, second is a read past the stack region.
This lets a specially built WAD file load new code into the running game.
TODO: explain the load chain

### Engine

This is a source of the entire ACE Engine. Resulting binary `code.lmp` has to be placed into generated WAD as `ACE_CODE`.  
NOTE: Everytime you pull new version make sure you use command `make clean` before building.
