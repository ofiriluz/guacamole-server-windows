Guacamole Server Windows
========================

Based on version 0.9.12-incubating

Basic installation:
All of the base dependencies are in the repo aswell, within the dependencies folder, and can be changed accordingly if needed

Build steps using command line with CMake

Dependencies:
- Note that msbuild needs to be installed in order to compile.
- Boost is required to be installed as a dependency on the enviroment or supplied on the CMake file in some way
```
mkdir build
cd build && cmake ../ -DCMAKE_CONFIGURATION_TYPE=RELEASE -G"Visual Studio 12 2013" -DCMAKE_INSTALL_PREFIX="../install"
cmake --build . --config Release --target install
```
Everything needed is installed within the given install folder

Usage:
- There is a configuration JSON file for the server, located within the config folder, which is mostly self explained, a couple of notes thou:
    - SSL can be enabled or disabled, and a DH backup pem file is used, also located within the config folder, so can be changed accordingly
    - The protocols library path is the path to the folder where the rdp protocol DLL is located, which should be under "lib" directory within the installation
    - There is no windows service support, so it runs on the foreground for now
- In order to run the actual service:
  ```
  guacservice.exe --config path/to/config.json
  ```
- Once the server is up, and there is a tomcat or some equvilant which can serve the java client, and it is configured to work with the guacamole server (guacd-hostname on guacamole.properties)
For more information on the client, refer to the docs:
https://guacamole.apache.org/doc/gug/configuring-guacamole.html


Couple of notes
- Most of the features of guacamole itself were removed and pretty much only the parts that are needed to work for windows have been taken
- The client part can be the same as guacamole itself or written with their API
- The project itself is open-source and based on the guacamole-server on the following repo: https://github.com/glyptodon/guacamole-server