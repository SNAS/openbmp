CENTOS_VERSION=`rpm -q --queryformat '%{VERSION}' centos-release`


# Installing dependencies

if [ -f /etc/redhat-release ]; then
    yum install -y gcc gcc-c++ libstdc++-devel boost-devel make cmake git wget openssl-libs openssl-devel cyrus-sasl-devel cyrus-sasl-lib
else
    sudo apt-get install gcc g++ libboost-dev cmake zlib1g-dev libssl1.0.0 libsasl2-2 libssl-dev libsasl2-dev
fi

# Installing librdkafka

git clone https://github.com/edenhill/librdkafka.git
cd librdkafka
./configure
make
make install
cd ..

# Installing yaml-cpp

git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp

if [ "$CENTOS_VERSION" == 6 ]; then
    git checkout release-0.5.3;
    sed -i '116,117 s/^/#/' ../CMakeLists.txt;
fi

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
make install
cd ..
cd ..

# Building OpenBMP

mkdir -p build;
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ./
make ./
