# socket tag
A two–player terminal-based tag game built in C using network sockets and ncurses.

## Overview
Player 1's (the host) goal is to catch Player 2 (the client) in the grid area.
Player 2's objective is to run for as long as possible!

## Prerequisites
* C compiler (e.g., gcc or clang)
* Make

## Install & Run
```bash
git clone https://github.com/chanr335/socket-tag.git 
cd socket-tag
make
```
On one machine/terminal instance:
```bash
./taghost
```
and on another:
```bash
./tagclient <hostname> 
```
