Install Steps
=============
See the various requirements and suggested system configurations at [Requirements](REQUIREMENTS.md)

Install Using Docker
--------------------

### docker hub: openbmp/aio
All-in-one (aio) includes everything needed to run the collector and store the data in MySQL.   You can use this container to test/evaluate OpenBMP as well as run smaller deployments.  Production deployments normally would have distributed collectors and a redundant pair of MySQL/MariaDB servers. 

#### Container Includes
* **Openbmpd** - Latest collector (listening port is TCP 5000)
* **MariaDB 10.0** - MySQL server (listening port TCP 3306)
* **Apache Kafka 0.8.x** - High performing message bus (listening ports are TCP 2181 and 9092)
* **Tomcat/DB_REST** - Latest Rest interface into MySQL/MariaDB (listening port TCP 8001)
* **Openbmp MySQL Consumer** - Latest Consumer that puts all data into MySQL

### Recommended Current Linux Distributions

  1. Ubuntu 14.04/Trusty
  1. CentOS 7/RHEL 7

- - -
### 1) Install docker
Docker host should be **Linux x86_64**.   Follow the [Docker Instructions](https://docs.docker.com/installation/) to install docker.  

- - -

### 2) Download the docker image

    docker pull openbmp/aio

- - -
### 3) Create MySQL volumes
MySQL/MariaDB uses a shared container (host) volume so that if you upgrade, restart, change the container it doesn't loose the DB contents.  **The DB will be initialized if the volume is empty.**  If the volume is not empty, the DB will be left unchanged.  This can be an issue when the schemas need to change. Therefore, to reinit the DB and apply the latest schema use docker run with the ```-e REINIT_DB=1```

When starting the container you will need to map a host file system to **/data/mysql** for the container.  You do this using the ```-v <host path>:/data/mysql```.  The below examples default to the host path of ```/var/openbmp/mysql```

#### (Optional) MySQL Temporary Table Space
Large queries or queries that involve sorting/counting/... will use a temporary table on disk.   It is recommended that
you use a **tmpfs** memory mount point for this.  Docker will not allow the container to mount tmpfs without having
CAP\_SYS\_ADMIN capability (``-privileged``).  To work around this limitation, the container will not create the tmpfs.  Instead
the container will use ``/var/mysqltmp`` which can be a volume to the host system tmpfs mount point.   The host system 
should create a tmpfs and then map that as a volume in docker using ``-v /var/openbmp/mysqltmp:/var/mysqltmp``

#### On host create mysql shared dir
    mkdir -p /var/openbmp/mysql
    chmod 777 /var/openbmp/mysql 

> The mode of 777 can be changed to chown <user> but you'll have to get that ID 
> by looking at the file owner after starting the container. 
    

#### On host create tmpfs (as root)

    mkdir -p /var/openbmp/mysqltmp
    echo "tmpfs /var/openbmp/mysqltmp tmpfs defaults,gid=nogroup,uid=nobody,size=2400M,mode=0777 0 0" >> /etc/fstab
    mount /var/openbmp/mysqltmp

### 4) Run docker container

> #### Memory for MySQL
> Mysql requires a lot of memory in order to run well.   Currently there is not a  consistent way to check on the container memory limit. The ```-e MEM=size_in_GB`` should be specified in gigabytes (e.g. 16 for 16GB of RAM).   If you fail to supply this variable, the default will use **/proc/meminfo** .  In other words, the default is to assume no memory limit. 

#### Environment Variables
Below table lists the environment variables that can be used with ``docker -e <name=value>``

NAME | Value | Details
:---- | ----- |:-------
**API\_FQDN** | hostname | **required** Fully qualified hostname for the docker host of this container, will be used for API and Kafka. It is also the collector Admin Id
MEM | RAM in GB | The size of RAM allowed for container in gigabytes. (e.g. ```-e MEM=15```)
OPENBMP_BUFFER | Size in MB | Defines the openbmpd buffer per router for BMP messages. Default is 16 MB.  
REINIT_DB | 1 | If set to 1 the DB will be reinitialized, which is needed to load the new schema sometimes.  This will wipe out the old data and start from scratch.  When this is not set, the old DB is reused.   (e.g. ```-e REINIT_DB=1```)
MYSQL\_ROOT\_PASSWORD | password | MySQL root user password.  The default is **OpenBMP**.  The root password can be changed using [standard MySQL instructions](https://dev.mysql.com/doc/refman/5.6/en/resetting-permissions.html).  If you do change the password, you will need to run the container with this env set.
MYSQL\_OPENBMP\_PASSWORD | password | MySQL openbmp user password.  The default is **openbmp**.  You can change the default openbmp user password using [standard mysql instructions](https://dev.mysql.com/doc/refman/5.6/en/set-password.html).  If you change the openbmp user password you MUST use this env.  

#### Run normally
> ##### IMPORTANT
> Make sure to define **API_FQDN** as a hostname (or fqdn) and not by IP.  The hostname should
> resolve to the docker host (*host that runs docker containers*) IP address, which is normally
> eth0.
> 
> If you do not plan to connect to the docker container via Kafka consumers, then you can use any 
> hostname, such as *openbmp.localdomain*

    docker run -d -e API_FQDN=<hostname> --name=openbmp_aio -e MEM=15 \
         -v /var/openbmp/mysql:/data/mysql \
         -p 3306:3306 -p 2181:2181 -p 9092:9092 -p 5000:5000 -p 8001:8001 \
         openbmp/aio

> ### Allow at least a few minutes for mysql to init the DB on first start



### Monitoring/Troubleshooting
Once the container is running you can run a **HTTP GET http://docker_host:8001/db_rest/v1/routers** to test that the API interface is working. 

You can use standard docker exec commands to monitor the log files.  To monitor 
openbmp, use ```docker exec openbmp_aio tail -f /var/log/openbmpd.log```

Alternatively, it can be easier at times to navigate all the log files from within the container. You can do so using:
    
    docker exec -it openbmp_aio bash

#### System Start/Restart Config (ubuntu 14.04)
By default, the containers will not start automatically on system boot/startup.  You can use the below example to instruct the openbmp/aio container to start automatically. 

You can read more at [Docker Host Integration](https://docs.docker.com/articles/host_integration/) on how to start containers automatically. 

> #### IMPORTANT
> The ```--name=openbmp_aio``` parameter given to the ```docker run``` command is used with the ```-a openbmp_aio``` parameter below to start the container by name instead of container ID.  You can use whatever name you want, but make sure to use the same name used in docker run.

    cat <<END > /etc/init/aio-openbmp.conf
    description "OpenBMP All-In-One container"
    author "tim@openbmp.org"
    start on filesystem and started docker
    stop on runlevel [!2345]
    respawn
    script
      /usr/bin/docker start -a openbmp_aio
    end script
    END
     
- - -     
  

Install on Ubuntu 14.04
-----------------------
> Use these instructions if you are not using Docker. 

First install either the **Server** or **Cloud** standard Ubuntu image available from [Ubuntu Download](http://www.ubuntu.com/download)

### Required Steps

  1. Update the apt get repo
  1. Install openbmpd using package or from source
  1. Install openbmp MySQL consumer
  1. Install Apache Kafka 
  1. Install mysql DB server
  1. Create mysql database and user
  1. Configure mysql settings
  1. Restart Mysql
  1. Create/update the database schema
  1. Run openbmpd
  1. Run openbmp-mysql-consumer


> #### NOTE
> Our builds of openbmpd statically links librdkafka so that you do not have to compile/install that. If needed, see [BUILD](BUILD.md) for details on how to build/install librdkafka.


### Before using 'apt-get' do the following to make sure the repositories are up-to-date

```
sudo apt-get update
```

### Install openbmp via package

  1. Download the openbmp
      
      [package for Ubuntu 14.04](http://www.openbmp.org/#!download.md)
      
  1. Install the package  - *You should have the depends already installed if you applied the above step.*
      
      **dpkg -i openbmp-VERSION.deb**
  
```
ubuntu@demo:~# sudo dpkg -i openbmp-0.11.0-pre3.deb 
(Reading database ... 57165 files and directories currently installed.)
Preparing to unpack openbmp-0.11.0-pre3.deb ...
Unpacking openbmp (0.11.0-pre3-pre3) over (0.10.0-pre3-pre3) ...
Setting up openbmp (0.11.0-pre3-pre3) ...
Processing triggers for ureadahead (0.100.0-16) ...
```

#### If installing from source

Follow the steps in [BUILD](BUILD.md) to install via source from github.

### Install openbmp mysql consumer

  1. Download the openbmp-mysql-consumer
      
      [Download Page](http://www.openbmp.org/#!download.md)
      
      ```
      wget http://www.openbmp.org/packages/openbmp-mysql-consumer-0.1.0-081315.jar
      ```  
      
> #### NOTE
> The consumer is a JAR file that is runnable using java -jar [filename].  In the near future this will be packaged in a DEB package so that you start it using 'service openbmp-mysql-consumer start'.  For now, you will need to run this JAR file via shell command.  See the last step regarding running the consumer for how to run it. 

### Install Apache Kafka

Follow the [Kafka Quick Start](http://kafka.apache.org/documentation.html#quickstart) guide in order to install and configure Apache Kafka.  You should have it up in running within minutes. 

note: Edit the **config/server.properties** file and make sure to define a valid FQDN  for the variable **advertised.host.name** .  

The collector (producer) and consumers will connect to Kafka and receive the **advertised.host.name** as where it should contact the server.  If this is set to localhost the producer/consumer will not be able to connect successfully to Kafka, unless of course everything is running on a single node.

```
# Hostname the broker will advertise to producers and consumers. If not set, it uses the
# value for "host.name" if configured.  Otherwise, it will use the value returned from
# java.net.InetAddress.getCanonicalHostName().

advertised.host.name=bmp-dev.openbmp.org
```

#### Example Install Steps

```
# Install JRE
sudo apt-get install openjdk-7-jre-headless

sudo mkdir /usr/local/kafka
sudo chown $(id -u) /usr/local/kafka

wget http://supergsego.com/apache/kafka/0.8.2.1/kafka_2.10-0.8.2.1.tgz
tar xzf kafka_2.10-0.8.2.1.tgz
cd kafka_2.10-0.8.2.1
sudo mv * /usr/local/kafka/
cd /usr/local/kafka/

# Update the Kafka config
#    USE FQDN for this host that is reachable by the collectors
sed -i -r 's/^[#]*advertised.host.name=.*/advertised.host.name=collector.openbmp.org/' \
 config/server.properties
sed -i -r 's/^[#]*log.dirs=.*/log.dirs=\/var\/kafka/' config/server.properties

# Create the logs dir for Kafka topics
sudo mkdir -m 0750 /var/kafka
sudo chown $(id -u) /var/kafka

nohup bin/zookeeper-server-start.sh config/zookeeper.properties > zookeeper.log &
sleep 1
nohup bin/kafka-server-start.sh config/server.properties > kafka.log &
```

### On DB server install mysql

> ### Note on MariaDB
> You can use MariaDB 10.0 or greater as well.  We have tested and validated that
> the schema and settings work with MariaDB >= 10.0
> 
> You can get instructions for installing MariaDB at [MariaDB Repositories](https://downloads.mariadb.org/mariadb/repositories/#mirror=digitalocean-nyc).  Make sure you **select 10.0**


```
sudo apt-get install mysql-server-5.6
```

* Install will prompt for a mysql root password, use one for the primary mysql user 'root'

> **NOTE**
> The root password in mysql is the MYSQL root user account.  This can be any password you would 
> like to use. It is not related to the platform/system root user or any other user.  This password is only the primary "root" password used when using '**mysql -u root -p**'

* After install, mysql should be running


### Login to mysql and create the openbmp database and user account

Apply the below to create the database and user that will be used by the openbmp daemon

> **NOTE**
> The below defaults the openbmp username to use '**openbmp**' as the password.  Change the  
> identified by '**openbmp'** to something else.

```
mysql -u root -p

   create database openBMP;
   create user 'openbmp'@'localhost' identified by 'openbmp';
   create user 'openbmp'@'%' identified by 'openbmp';
   grant all on openBMP.* to 'openbmp'@'localhost';
   grant all on openBMP.* to 'openbmp'@'%';
```

### MySQL Temporary Table Space

Large queries or queries that involve sorting/counting/... will use a temporary table on disk.   We have found that using a **tmpfs** will improve performance. 


#### Create tmpfs (as root)

The below will also configure the tmpfs to be mounted upon restart/boot.

    mkdir -p /var/mysqltmp
    echo "tmpfs /var/mysqltmp tmpfs defaults,gid=mysql,uid=mysql,size=2400M,mode=0777 0 0" >> /etc/fstab
    mount /var/mysqltmp


### Update the /etc/my.cnf file to enable InnoDB and tune memory

The below **MUST** but adjusted based on your memory available.  Ideally it should be set as high as possible. Below is for a system that has 16G of RAM and 8vCPU.

> #### IMPORTANT
> You must define **max\_allowed\_packet** to **384M** or greater to support
> the bulk inserts/updates, otherwise you will get errors that indicate packet is
> is too large or that the server connection has gone away. 

* sudo vi /etc/mysql/my.cnf

Under **[mysqld]** section

```
# use the tmpfs mount point
tmpdir      = /var/mysqltmp

key_buffer_size     = 128M

# This is very IMPORTANT, must be high to handle bulk inserts/updates
max_allowed_packet  = 384M

net_read_timeout    = 45
thread_stack        = 192K
thread_cache_size   = 8

# This value should be roughly 80% of system memory
innodb_buffer_pool_size = 12G

# This value should be the GB value of the innodb_buffer_pool_size
#   Ideally one instance per GB
innodb_buffer_pool_instances =  12

transaction-isolation        = READ-UNCOMMITTED
innodb_flush_log_at_trx_commit  = 0
innodb_random_read_ahead        = 1
innodb_read_ahead_threshold     = 10
innodb_log_buffer_size    = 16M
innodb_log_file_size      = 2G
query_cache_limit         = 1G
query_cache_size          = 1G
query_cache_type          = ON
join_buffer_size          = 128M
sort_buffer_size          = 128M
innodb_sort_buffer_size   = 16M
myisam_sort_buffer_size   = 128M
read_rnd_buffer_size      = 128M
innodb_thread_concurrency = 0

max_heap_table_size       = 2M
tmp_table_size            = 2M
innodb_file_per_table     = ON
innodb_doublewrite        = OFF
innodb_spin_wait_delay    = 24
innodb_io_capacity        = 2000

# Adjust the below to roughly the number of vCPU's times 2
innodb_read_io_threads    = 16
innodb_write_io_threads   = 16
```

### Restart Mysql so that the changes to config take effect

* sudo service mysql restart

### Load the schema

Load the openbmp DB schema by downloading it from www.openbmp.org.  You can also get the 
latest from [GitHub OpenBMP](https://github.com/OpenBMP/openbmp)

> #### Choose the right schema
> See the [download](http://www.openbmp.org/#!download.md)
 page for details on which schema to use for which package.

**Latest/Current DEB package uses the current schema as below**

```
curl -O https://raw.githubusercontent.com/OpenBMP/openbmp-mysql-consumer/master/database/mysql-openbmp-current.db

mysql -u root -p openBMP < mysql-openbmp-current.db 
```

> Use the password for root user that was created when mysql was installed. 


### Run the openbmp server

MySQL should be installed now and it should be running.   OpenBMP is ready to run. 

**openbmpd**   *(normally installed in /usr/bin)*

```
  REQUIRED OPTIONS:
     -a <string>       Admin ID for collector, this must be unique for this collector.  hostname or IP is good to use


  OPTIONAL OPTIONS:
     -k <host:port>    Kafka broker list format: host:port[,...]
                       Default is 127.0.0.1:9092
     -m <mode>         Mode can be 'v4, v6, or v4v6'
                       Default is v4.  Enables IPv4 and/or IPv6 BMP listening port

     -p <port>         BMP listening port (default is 5000)

     -c <filename>     Config filename, default is /etc/openbmp/openbmpd.conf
     -l <filename>     Log filename, default is STDOUT
     -d <filename>     Debug filename, default is log filename
     -pid <filename>   PID filename, default is no pid file
     -b <MB>           BMP read buffer per router size in MB (default is 15), range is 2 - 128
     -hi <minutes>     Collector message heartbeat interval in minutes (default is 240 (4 hrs)

  OTHER OPTIONS:
     -v                   Version

  DEBUG OPTIONS:
     -dbgp             Debug BGP parser
     -dbmp             Debug BMP parser
     -dmsgbus          Debug message bus
```

Below starts openbmp on port 5555 for inbound BMP connections using Kafka server localhost:9092 and buffer of 16MB per router. 

```
sudo openbmpd -a $(uname -n) -k localhost -b 16 -p 5555 -l /var/log/openbmpd.log -pid /var/run/openbmpd.pid
```

> **NOTE** 
> The above command uses 'sudo' because openbmp is creating the log file /var/log/openbmp.log and updating the pid file /var/run/openbmp.pid, which normally are not writable to normal users.  If the log and pid files are writable by the user running openbmpd, then sudo is not required. 


### Run openbmp-mysql-consumer

You can unpack the JAR file if you want to modify the logging config.  Otherwise,  you can run as follows:

> Consumer can run on any platform
    
      nohup java -Xmx512M -Xms512M -XX:+UseParNewGC -XX:+UseConcMarkSweepGC -XX:+DisableExplicitGC \
            -jar openbmp-mysql-consumer-0.1.0-081315.jar  -dh db.openbmp.org \
            -dn openBMP -du openbmp -dp openbmpNow -zk localhost > mysql-consumer.log &     
