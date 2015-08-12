Install Steps
=============

See the various requirements and suggested system configurations at [Requirements](REQUIREMENTS.md)

### Docker
Docker files will be created soon to automate the build/install process and to make it very easy to deploy. 

### Recommended Current Linux Distributions

  1. Ubuntu 14.04/Trusty
  1. CentOS 7/RHEL 7
  

Ubuntu 14.04
------------
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
ubuntu@demo:~# sudo dpkg -i openbmp-0.8.0-pre4.deb
Selecting previously unselected package openbmp.
(Reading database ... 51367 files and directories currently installed.)
Preparing to unpack openbmp-0.8.0-pre4.deb ...
Unpacking openbmp (0.8.0-pre) ...
Setting up openbmp (0.8.0-pre) ...
```

#### If installing from source

Follow the steps in [BUILD](BUILD.md) to install via source from github.

### Install openbmp mysql consumer

  1. Download the openbmp-mysql-consumer
      
      [Download Page](http://www.openbmp.org/#!download.md)
      
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

### On DB server install mysql

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
curl -o mysql-openbmp-current.db https://raw.githubusercontent.com/OpenBMP/openbmp/master/database/mysql-openbmp-current.db
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
    
     java -jar target/openbmp-mysql-consumer-0.1.0.jar -dh db.openbmp.org -dn openBMPdev -du openbmp -dp openbmpNow -zk bmp-dev.openbmp.org
     
