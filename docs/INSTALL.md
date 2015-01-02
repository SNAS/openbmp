Install Steps
=============
See the various requirements and suggested system configurations at [Requirements](REQUIREMENTS.md)

### Recommended Current Linux Distributions
  1. Ubuntu 14.04/Trusty
  1. CentOS 7/RHEL 7 - Coming soon
  
### Older Linux Distributions - but verified/tested
  1. Ubuntu 12.04/Precise - Coming soon
  1. Centos 6/RHEL 6 - Coming soon


Ubuntu 14.04
------------
First install either the **Server** or **Cloud** standard Ubuntu image available from [Ubuntu Download](http://www.ubuntu.com/download)

### Required Steps
  1. Update the apt get repo
  1. Install mysql client and client libraries
  1. Install openbmpd using package or from source
  1. Install mysql DB server
  1. Create mysql database and user
  1. Configure mysql settings
  1. Restart Mysql
  1. Create/update the database schema
  1. Run openbmpd
  
  

### Before using 'apt-get' do the following to make sure the repositories are up-to-date
```
sudo apt-get update
```

### On openbmpd server Install the client libraries

```
sudo apt-get install mysql-client-5.6 mysql-common-5.6 libmysqlcppconn7
```

### Install openbmp via package
  1. Download the openbmp [package for Ubuntu 14.04](/packages/openbmp-0.8.0-pre4.deb)
  1. Install the package  - *You should have the depends already installed if you applied the above step.*
      
      **dpkg -i openbmp-0.8.0-pre4.deb**
  
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

### On DB server install mysql
```
sudo apt-get install mysql-server-5.6
```

* Install will prompt for a mysql root password, use one for the primary mysql user 'root'

> **NOTE**
> The root password in mysql is the MYSQL root user account.  This can be any password you would 
> like to use. It is not related to the platform/system root user or any other user.  This password is only the primary "root" password used when using '**mysql -u root -p**'

* After install, mysql should be running


You might benefit from tuning the read-ahead setting for the disk.  Normally this defaults to 256, but it's suggested to use 4096. 

Below is an example of how to set the read-ahead on the root disk /dev/vda1

```
sudo blockdev --setra 4096 /dev/vda1
```


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


### Update the /etc/my.cnf file to enable InnoDB and tune memory
The below **MUST** but adjusted based on your memory available.  Ideally it should be set as high as possible. Below is for a system that has 16G of RAM and 8vCPU.

* sudo vi /etc/mysql/my.cnf

Under **[mysqld]** section

```
# Set this to roughly 20% of system memory
key_buffer_size         = 3G

# This value should be roughly 80% of system memory
innodb_buffer_pool_size = 12G

# This value should be innodb_buffer_pool_size in GB divide by 1
innodb_buffer_pool_instances =  10

#innodb_additional_mem_pool_size = 50M
innodb_flush_log_at_trx_commit  = 2
innodb_random_read_ahead        = 1
innodb_read_ahead_threshold     = 10
innodb_log_file_size      = 384M
query_cache_size          = 1G
sort_buffer_size          = 1G
join_buffer_size          = 1G
read_rnd_buffer_size      = 256M
innodb_thread_concurrency = 0
innodb_read_io_threads    = 24
innodb_write_io_threads   = 24
max_heap_table_size       = 256000000
tmp_table_size            = 256000000
innodb_file_per_table     = ON
```

### Restart Mysql so that the changes to config take effect

* sudo service mysql restart

### Load the schema
Load the openbmp DB schema by downloading it from www.openbmp.org.  You can also get the 
latest from [GitHub OpenBMP](https://github.com/OpenBMP/openbmp)

```
curl -o mysql-openbmp-current.db http://www.openbmp.org/docs/mysql-openbmp-current.db
mysql -u root -p openBMP < mysql-openbmp-current.db 
```

> Use the password for root user that was created when mysql was installed. 


### Run the openbmp server
MySQL should be installed now and it should be running.   OpenBMP is ready to run. 

**openbmpd**   *(normally installed in /usr/bin)*

```
  REQUIRED OPTIONS:
     -dburl <url>      DB url, must be in url format
                       Example: tcp://127.0.0.1:3306
     -dbu <name>       DB Username
     -dbp <pw>         DB Password

  OPTIONAL OPTIONS:
     -p <port>         BMP listening port (default is 5000)
     -dbn <name>       DB name (default is openBMP)

     -c <filename>        Config filename, default is /etc/openbmp/openbmpd.conf
     -l <filename>        Log filename, default is STDOUT
     -d <filename>        Debug filename, default is log filename
     -pid <filename>      PID filename, default is no pid file

  DEBUG OPTIONS:
     -dbgp             Debug BGP parser
     -dbmp             Debug BMP parser
     -dmysql           Debug mysql
```

Below starts openbmp on port 5555 for inbound BMP connections. 
```
sudo openbmpd -dburl tcp://127.0.0.1:3306 -dbu openbmp -dbp openbmp -p 5555 -l /var/log/openbmpd.log -pid /var/run/openbmpd.pid
```
> **NOTE** 
> The above command uses 'sudo' because openbmp is creating the log file /var/log/openbmp.log and updating the pid file /var/run/openbmp.pid, which normally are not writable to normal users.  If the log and pid files are writable by the user running openbmpd, then sudo is not required. 



## User Interface

The interface is being refactored to use ODL.  Please stay tuned for an update. 
