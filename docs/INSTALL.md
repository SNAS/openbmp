Ubuntu 14.04
------------

### Before using 'apt-get' do the following to make sure the repositories are up-to-date
```
sudo apt-get update
```

### On openbmpd server
Install the client libraries

```
sudo apt-get install mysql-client-5.6 mysql-common-5.6 libmysqlcppconn7
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


### Update the /etc/my.cnf file to enable InnoDB and tune memory
The below **MUST** but adjusted based on your memory available.  Ideally it should be set as high as possible. Below is for a system that has 6G of RAM. 

* sudo vi /etc/mysql/my.cnf

```
# Set this to roughly 20% of system memory
key_buffer_size         = 1G

# This value should be roughly 80% of system memory
innodb_buffer_pool_size = 4G

# This value should be innodb_buffer_pool_size in GB divide by 1
innodb_buffer_pool_instances = 4 

innodb_additional_mem_pool_size = 20M
innodb_log_file_size    = 384M
innodb_flush_log_at_trx_commit = 2
query_cache_size        = 512M
sort_buffer_size        = 512M
join_buffer_size        = 512M
innodb_use_sys_malloc = 0
read_rnd_buffer_size = 128M
```

Restart Mysql so that the changes to config take effect.

* sudo service mysql restart

### Load the schema
Load the openbmp DB schema

```
mysql -u openbmp -p openBMP < database/mysql-openbmp-current.db 
```

> Use the password for openbmp, which should be **openbmp** at this point.


### Run the openbmp server 
MySQL should be installed now and it should be running.   OpenBMP is ready to run. 

**openbmpd**

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
openbmpd -dburl tcp://127.0.0.1:3306 -dbu openbmp -dbp openbmp -p 5555 -pid /var/run/openbmpd.pid
```


## User Interface

### On openbmp, db server, or new instance install user interface

```
sudo apt-get install tomcat7
```

Tomcat should be running and it should be setup to start when the system boots.  You can 
manually stop/start tomcat using the below.

```
sudo service tomcat7 stop
sudo service tomcat7 start
```

#### Change the root application index.html 
By default, tomcat will install a default root index.html, which normally you do not want. 
Simply truncate this file using the below to render it useless.  You can also remove the ROOT dir. 

```
cp /dev/null /var/lib/tomcat7/webapps/ROOT/index.html
```
