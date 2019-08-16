OpenBMP V2

# Build Instructions for Ubuntu 18.04

## install dependancies
```
sudo apt-get install gcc g++ libboost-dev cmake zlib1g-dev libssl1.0.0 libsasl2-2 libssl-dev libsasl2-dev
``` 

## install librdkafka
```
git clone https://github.com/edenhill/librdkafka.git
cd librdkafka
./configure
make
sudo make install
```

## install yaml-cpp@0.6.2
```
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
git checkout yaml-cpp-0.6.2
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
sudo make install
```

## install libparsebgp
```
git clone https://github.com/CAIDA/libparsebgp.git
cd libparsebgp
./autogen.sh
./configure
make
make install
```

## build openbmp v2
```
git clone https://github.com/OpenBMP/openbmp.git
cd openbmp
git checkout v2
mkdir build
cd build
cmake ..
make
```

# How to Run
You should now see the openbmp binary, e.g., `openbmp_main`.
Run the program by passing the openbmp.conf file.

`./openbmp_main -c openbmp.conf`
