Building from Source 
====================
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
2. cd openbmp
2. cmake .
2. make

### Example output
```
localadmin@toolServer:/ws/openbmp$ cmake .
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
-- Configuring done
-- Generating done
-- Build files have been written to: /ws/openbmp

localadmin@toolServer:/ws/openbmp$ make
Scanning dependencies of target openbmpd
make[2]: Warning: File `Server/CMakeFiles/openbmpd.dir/depend.make' has modification time 3.9 s in the future
[ 12%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/BMPServer.cpp.o
[ 25%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/DbImpl_mysql.cpp.o
[ 37%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/openbmp.cpp.o
[ 50%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/parseBGP.cpp.o
[ 62%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/parseBMP.cpp.o
[ 75%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/md5.cpp.o
[ 87%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/Logger.cpp.o
[100%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/BitByteUtils.cpp.o
Linking CXX executable openbmpd
[100%] Built target openbmpd

localadmin@toolServer:/ws/openbmp$ sudo make install
[100%] Built target openbmpd
Install the project...
-- Install configuration: ""
-- Installing: /usr/bin/openbmpd
```



Installing the binary & Configs
-------------------------------

Run: sudo make install

> this will install openbmpd in /usr/bin/ by default.  You should be able to type **openbmpd** now to get the usage help. 
> 

