
Requirements
============
Various requirements and suggested system configurations for runtime and build installs. 

Runtime *(When running openbmpd and mysql)*
-----------------------------------------

### Database
* **MySQL 5.5.17 or MairaDB 5.5 Series or greater**

### Shared Libraries
* **MySQL/MariaDB C++ Connector version 1.1.0 or greater** 
* **MySQL/MariaDB Client libraries version 5.5 or greater**
* **libstdc++6 Version 4.6.x or greater**  (gcc version 4.6.x or greater)

> MariaDB should work, but this has not been tested yet.  Testing will be performed for CentOS7/RHEL7 installs.

Server Requirements
-------------------
It is recommended to use the following server configuration


### Openbmpd 
Openbmpd is not disk or memory heavy.  Therefore the VM itself doesn't have to have a lot of disk or memory.  The key is CPU, which normally 2 vCPU's are sufficient, but in large environments with many routers it is recommended to have more vCPU's. 

| Arch      | vCPU      | RAM        | DISK        | DISK TYPE |
| --------- | --------- | ---------- | ----------- | ----------| 
| VM x86_64 | 2 or more | 2G or more | 10G or more | Any       |

### MySQL/MariaDB DB
The database is CPU, memory, and disk intensive. It's recommended that the DB server be as large as you can afford.  The size is directly related to the number of prefixes/data being stored, not how many peers or routers are being monitored. If you are monitoring routers with full internet routing tables (many peers) then the below is the recommended minimum for the DB server. 

| Arch      | vCPU      | RAM         | DISK        | DISK TYPE  |
| --------- | --------- | ----------- | ----------- | -----------| 
| VM x86_64 | 8 or more | 16G or more | 60G or more | SSD or SAN |

>  Openbmpd can coexist on the same server as the DB.  If you run openbmpd and the DB on the same box, then it's okay to use the above for both.  


BUILD/Development - *(When compiling and building openbmpd from source)*
--------------------------------------------------------------------

### Database *(same as runtime requirement)*
* **MySQL 5.5.17 or MairaDB 5.5 Series or greater**

### Shared Libraries *(same as runtime plus extras for development)*
* **MySQL/MariaDB C++ Connector version 1.1.0 or greater** 
* **MySQL/MariaDB Client libraries version 5.5 or greater**
* **libstdc++6 Version 4.6.x or greater**  (gcc version 4.6.x or greater)

### Development Libraries and Headers
* **Boost Headers 0.41.0** or greater
* **Gcc/G++/STDC++ 4.6.x** or greater
* **MySQL Client development headers/lib 5.5.x** or greater
* **MySQL C++ connector development headers/lib 1.1.0** or greater
* **CMake 2.8.x** or greater

> MariaDB should work development but this has not been tested yet.  Testing will be done
> when CentOS 7/RHEL 7 builds are ready.  Please send **tim@openbmp.org** an email if you would like CentOS/RHEL support sooner.  

