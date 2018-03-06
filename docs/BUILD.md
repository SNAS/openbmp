Building from Source 
====================
See the various requirements and suggested system configurations at [Requirements](REQUIREMENTS.md)

Openbmp is built and installed using 'cmake' to build the makefiles. 


All Platforms (Ubuntu, CentOS, etc.)
------------------------------------

> #### YOU MUST INSTALL DEPENDS BEFORE BUILDING librdkafka and libyaml-cpp

### Install librdkafka development and runtime libraries

See [librdkafka](https://github.com/edenhill/librdkafka) for detailed instructions on how to install.  

```
git clone https://github.com/edenhill/librdkafka.git
cd librdkafka
./configure
make
sudo make install
```

### Install libyaml-cpp development and runtime libraries

See [yaml-cpp](https://github.com/jbeder/yaml-cpp) for detailed instructions on how to install.

```
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp


# git checkout yaml-cpp-0.6.2

# If on Centos6 
# git checkout yaml-cpp-0.5.3

# IF on CentOS6/RHEL6 - you might run into an issue about date_time boost lib. This issue
#    is specific to cmake on centos6/rhel6.   If you run into this issue, you can
#    safely run the below to resolve the issue. 
#sed -i '116,117 s/^/#/' ../CMakeLists.txt

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
sudo make install
```

Ubuntu 14.04
------------
### Install Ubuntu 14.04
Install standard Ubuntu 14.04/Trusty server image [Ubuntu Download](http://www.ubuntu.com/download)

### Install the dependancies

``` 
sudo apt-get install gcc g++ libboost-dev cmake zlib1g-dev libssl1.0.0 libsasl2-2 libssl-dev libsasl2-dev 
```

RHEL7/CentOS7
-------------

### Install RHEL7 or CentOS 7.  
We use CentOS 7 minimal.  [CentOS 7 Download](http://www.centos.org/download/)

### Install basic dependancies
```
sudo yum install -y gcc gcc-c++ libstdc++-devel boost-devel cmake git wget openssl-libs openssl-devel cyrus-sasl-devel cyrus-sasl-lib
```


RHEL6/CentOS6 (6.5) - Legacy support
------------------------------------

### Install RHEL6 or CentOS 6.  
We use CentOS 6 minimal.  [CentOS 6 Download](http://wiki.centos.org/Download)

### Install basic dependancies
```
sudo yum install -y gcc gcc-c++ libstdc++-devel boost-devel boost-static cmake git wget  openssl-libs openssl-devel cyrus-sasl-devel cyrus-sasl-devel cyrus-sasl-lib
```


Compiling Source (All Platforms)
-------------------------------------
> #### IMPORTANT
> Make sure you have installed librdkafka development and runtime libs as mentioned under all platforms above.

Do the following: 

    git clone https://github.com/OpenBMP/openbmp.git
    cd openbmp
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ../  
    make

### Example output
```
localadmin@toolServer:/ws/ws-openbmp/openbmp/build$ cmake ../
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
-- Found OpenSSL: /usr/lib/x86_64-linux-gnu/libssl.so;/usr/lib/x86_64-linux-gnu/libcrypto.so (found suitable version "1.0.1f", minimum required is "1")
-- Performing Test SUPPORTS_STD_CXX11
-- Performing Test SUPPORTS_STD_CXX11 - Success
-- Performing Test SUPPORTS_STD_CXX01
-- Performing Test SUPPORTS_STD_CXX01 - Success
-- Configuring done
-- Generating done
-- Build files have been written to: /ws/ws-openbmp/openbmp/build

localadmin@toolServer:/ws/ws-openbmp/openbmp/build$ make
Scanning dependencies of target openbmpd
[  5%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bmp/BMPListener.cpp.o
[ 10%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bmp/BMPReader.cpp.o
[ 15%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/kafka/MsgBusImpl_kafka.cpp.o
[ 20%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/kafka/KafkaEventCallback.cpp.o
[ 25%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/kafka/KafkaDeliveryReportCallback.cpp.o
[ 30%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/openbmp.cpp.o
[ 35%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bmp/parseBMP.cpp.o
[ 40%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/md5.cpp.o
[ 45%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/Logger.cpp.o
[ 50%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/client_thread.cpp.o
[ 55%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/parseBGP.cpp.o
[ 60%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/NotificationMsg.cpp.o
[ 65%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/OpenMsg.cpp.o
[ 70%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/UpdateMsg.cpp.o
[ 75%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/MPReachAttr.cpp.o
[ 80%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/MPUnReachAttr.cpp.o
[ 85%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/ExtCommunity.cpp.o
[ 90%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/linkstate/MPLinkState.cpp.o
[ 95%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/bgp/linkstate/MPLinkStateAttr.cpp.o
[100%] Building CXX object Server/CMakeFiles/openbmpd.dir/src/kafka/KafkaPeerPartitionerCallback.cpp.o
Linking CXX executable openbmpd
[100%] Built target openbmpd

```

Binary will be located under **Server/**

Install (All Platforms)
----------------------------------------------------

Run: **sudo** make install

> this will install openbmpd in /usr/bin/ by default.  You should be able to type **openbmpd** now to get the usage help. 
> 

```
ubuntu@bmp-dev:~/test/openbmp/build$ sudo make install
[100%] Built target openbmpd
Install the project...
-- Install configuration: ""
-- Installing: /usr/bin/openbmpd
-- Installing: /etc/init/openbmpd.conf
-- Installing: /etc/default/openbmpd
-- Installing: /etc/init.d/openbmpd
-- Installing: /etc/logrotate.d/openbmpd
```
