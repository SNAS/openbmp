# Getting Started

You can start using OpenBMP without any BGP feeds of your own. We provide you an application that downloads publicly available bgp data from RouteViews which you can immediately visualize using OpenBMP UI.



## Step 1: Install AIO Container

Go to OpenBMP github repository: **https://github.com/OpenBMP**

Click on docker directory: **https://github.com/OpenBMP/docker**

Click on aio directory: **https://github.com/OpenBMP/docker/tree/master/aio**

aio is the all-in-one container that includes all the main components of OpenBMP: bmp collector, database, consumer that reads the data from the message bus and writes to the database, message bus, database api service, and RPKI validator.

Follow directions in this directory.

## Step 2: Install OpenBMP UI


Go to OpenBMP github repository: **https://github.com/OpenBMP**

Click on docker directory: **https://github.com/OpenBMP/docker**

Click on aio directory: **https://github.com/OpenBMP/docker/tree/master/ui**

OpenBMP UI is installed in its own container. 

Follow directions in this directory.

## Step 3: Install MRT2BMP Application

Go to OpenBMP github repository: **https://github.com/OpenBMP**

Click on openbmp-mrt2bmp directory: **https://github.com/OpenBMP/openbmp-mrt2bmp**

mrt2bmp application allows you to download BGP data from RouteViews and send it to openbmp collector. The application downloads data from RouteViews every 15 minutes (the frequency in which RouteViews make the data available for download). 

### Happy Browsing!!!

Login to OpenBMP UI and start browsing!



