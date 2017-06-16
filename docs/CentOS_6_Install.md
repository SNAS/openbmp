# CentOS 6 Install Instructions

Validated using CentOS 6.8 (CentOS-6-x86_64-GenericCloud-1612)


## 1) Install Steps for Collector and Kafka

```sh
#
# Install the dependencies  
#
sudo yum install -y wget  openssl cyrus-sasl-lib java-1.8.0-openjdk-headless unzip

#
# Install the collector
#
wget --no-check-certificate \
         https://build-jenkins.openbmp.org:8443/job/openbmp-server-centos6/lastSuccessfulBuild/artifact/*zip*/archive.zip

unzip  archive.zip *.rpm
rm -f archive.zip
sudo yum -y localinstall archive/build/rpm_package/*
rm -rf archive

#
# Install Kafka
#
mkdir /usr/local/kafka
cd /tmp
wget http://mirror.cc.columbia.edu/pub/software/apache/kafka/0.10.0.1/kafka_2.11-0.10.0.1.tgz
tar xzf kafka_2.11-0.10.0.1.tgz
cd kafka_2.11-0.10.0.1
mv * /usr/local/kafka/
cd /usr/local/kafka/
rm -rf /tmp/kafka_2.11-0.10.0.1*

```

## 2) Configure openbmpd.conf

```sh
vi /usr/etc/openbmp/openbmpd.conf
```

## 3) Configure/Start Kafka

### Config (only once)
```sh
# Update config
sed -i -r 's/^[#]*log.retention.hours=.*/log.retention.hours=24/' /usr/local/kafka/config/server.properties
sed -i -r 's/^[#]*num.partitions=.*/num.partitions=6/' /usr/local/kafka/config/server.properties
```

> #### Log directory
> By default the log directory is in /tmp.. You can change that via the config/server.properties

> #### Hostname for Kafka clients
> Kafka advertises a hostname to clients. Clients will connect to this hostname.  If this is not set correctly, this will cause clients to hang and not work.  The default uses the system hostname, which is not always correct.  You should update **advertised.listeners** to the hostname that works for this install. 


### Run

```sh
cd /usr/local/kafka

bin/zookeeper-server-start.sh config/zookeeper.properties > /var/log/zookeeper.log &

sleep 8

bin/kafka-server-start.sh config/server.properties > /var/log/kafka.log &
```


## 4) Run collector

```sh
cd /tmp
openbmpd -c /usr/etc/openbmp/openbmpd.conf -l /var/log/openbmpd.log
```

> #### NOTE:
> RPM does not include an init script, so you should create one.  Please submit a pull request if you want to add that.

### 5) Validate collector is producing to Kafka

A quick validation is to consume the collector messages.   This will be there even if there are no routers/peers.

```sh
/usr/local/kafka/bin/kafka-console-consumer.sh --new-consumer --bootstrap-server localhost:9092 \
             --topic openbmp.parsed.collector --from-beginning
```
