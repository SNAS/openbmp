echo "Hello";
ls
pwd

yum install -y gcc gcc-c++ libstdc++-devel boost-devel make cmake git wget openssl-libs openssl-devel cyrus-sasl-devel cyrus-sasl-lib

git clone https://github.com/edenhill/librdkafka.git
cd librdkafka
./configure
make
make install
cd ..

git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
make install
cd ..
cd ..

echo "Hello";
ls
pwd

mkdir -p build;
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ./
make ./
