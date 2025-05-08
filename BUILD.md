# "The Hacker's Sandbox" Build Documentation

THS has three external dependencies, which you will need before you can compile.

* Lua (tested with 5.2, though others should work)
* PCRE
* Boost (Thread and System modules)

On a Debian-based system, you can install these using the following:

```
$ sudo apt-get install liblua5.2-dev libpcre3-dev libboost-thread-dev
```

## Building

Once the libraries are in-place, you can run `make` to build your binaries. This will create a `sandbox` binary that you can run.

```
$ make
```

By default, it uses the name `lua` for the linker. On some systems, however, this name may be different. If you have used `apt-get install liblua5.2-dev` to install Lua on Debian, for example, you will need to specify the Lua version to `make` in order to build properly.

```
$ make LUA=5.2
```
