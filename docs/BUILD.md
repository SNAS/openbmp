Building from Source 
====================
See the various requirements and suggested system configurations at [Requirements](REQUIREMENTS.md)

Openbmp is built and installed using 'cmake' to build the makefiles. 

Install the dependancies
------------------------

### Ubuntu 14.04

``` 
sudo apt-get install gcc g++ libboost-dev mysql-client-5.6 mysql-common-5.6 cmake libmysqlcppconn-dev libmysqlcppconn7
```

### RHEL7/CentOS7
tbd


Compiling
---------

Do the following: 

1. git clone https://github.com/OpenBMP/openbmp.git
1. cd openbmp
1. cmake **-DCMAKE_INSTALL_PREFIX:PATH=/usr** **.**  
   *(don't forget the dot '.')*
1. make

### Example output
```
localadmin@toolServer:/ws/ws-openbmp/openbmp$ cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
-- The C compiler identification is GNU 4.8.2
-- The CXX compiler identification is GNU 4.8.2
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++
-- Check for working CXX compiler: /usr/bin/c++ -- works
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Boost version: 1.54.0
-- Performing Test SUPPORTS_STD_CXX11
-- Performing Test SUPPORTS_STD_CXX11 - Success
-- Performing Test SUPPORTS_STD_CXX01
-- Performing Test SUPPORTS_STD_CXX01 - Success
-- Configuring done
-- Generating done
-- Build files have been written to: /ws/ws-openbmp/openbmp

localadmin@toolServer:/ws/ws-openbmp/openbmp$ make
Scanning dependencies of target openbmpd
[  7%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/BMPListener.cpp.o
[ 14%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/BMPReader.cpp.o
[ 21%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/DbImpl_mysql.cpp.o
[ 28%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/openbmp.cpp.o
[ 35%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/parseBMP.cpp.o
[ 42%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/md5.cpp.o
[ 50%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/Logger.cpp.o
[ 57%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/client_thread.cpp.o
[ 64%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/parseBGP.cpp.o
[ 71%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/NotificationMsg.cpp.o
[ 78%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/OpenMsg.cpp.o
[ 85%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/UpdateMsg.cpp.o
[ 92%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/MPReachAttr.cpp.o
[100%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/MPUnReachAttr.cpp.o
Linking CXX executable openbmpd
[100%] Built target openbmpd
```


Installing the binary & Configs
-------------------------------

Run: **sudo** make install

> this will install openbmpd in /usr/bin/ by default.  You should be able to type **openbmpd** now to get the usage help. 
> 

```
localadmin@toolServer:/ws/ws-openbmp/openbmp$ sudo make install
[100%] Built target openbmpd
Install the project...
-- Install configuration: ""
-- Installing: /usr/bin/openbmpd
```
