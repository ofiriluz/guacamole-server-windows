Guacamole Server Windows
========================

Based on version 0.9.12-incubating

Basic installation:
All of the base dependencies are in the repo aswell, within the dependencies folder, and can be changed accordingly if needed

Build steps using command line with CMake
```
mkdir build
cd build && cmake ../ -DCMAKE_CONFIGURATION_TYPE=RELEASE -G"Visual Studio 12 2013" -DCMAKE_INSTALL_PREFIX="../install"
```

Couple of notes
- Most of the features of guacamole itself were removed and pretty much only the parts that are needed to work for windows have been taken
- The client part can be the same as guacamole itself or written with their API

Usage:
- There is a configuration JSON file for the server, located within the config folder, which is mostly self explained, a couple of notes thou:
    - SSL can be enabled or disabled, and a DH backup pem file is used, also located within the config folder, so can be changed accordingly
    - The protocols library path is the path to the folder where the rdp protocol DLL is located, which should be under "lib" directory within the installation
    - There is no windows service support, so it runs on the foreground for