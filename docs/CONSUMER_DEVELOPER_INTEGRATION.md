Consumer Developer Integration
------------------------------

Developers can now integrate with the real-time message stream of both **parsed** messages and **BMP raw** binary data.   The openbmp collector is an Apache Kafka producer that produces real-time. Any application wishing to access the live stream can do so by simply creating an Apache Kafka consumer.    


See [Message Bus API Specification](MESSAGE_BUS_API.md) for details on the API specification. 

Alternatively, the developer can interact with the MySQL database via the openbmp-mysql-consumer.  See [DB_SCHEMA](http://www.openbmp.org/#!docs/DB_SCHMEA.md) for more details. 

Reasons for using Apache Kafka
------------------------------

### Problem without Message Bus


* Direct MySQL integration into the collector is too specific for other products to integrate with
* Parsed data into MySQL didn't support Real-Time monitoring/alerting
* BMP binary streams were not well suited for MySQL
* Supporting multiple consuming destinations would require the collector to be aware of the multiple destinations and tracking them
* Supporting multiple destination formats, such as MySQL, MongoDB, Cassandra, flat files/logging, etc. would be code intrusive to the collector, requiring it to be rebuilt and restarted

### Base Requirements for a Message Bus
* UTF-8/binary
* Messages per second >= 20K per broker node
* Message sizes <= 256K bytes
* Clustering of broker nodes to support messages per second >= 500K
* Producer queue must support ability for one message to be delivered to multiple consumers (broadcast/fanout)
* Producer queue must support the ability to be "sticky" in terms of load balancing (cannot just do round robin per message)
* Must include a fully functional open source broker (cannot depend on vendor/specific product(s))
* Multiple client language support (C/C++, Java, Python, ...)
* Must support consumers of different consumption rates. Not all consumers are equal in terms of their ability to consume messages at the rate they are being produced


### Why Not use AMQP?

* RabbitMQ doesn't do a good job of supporting AMQP version 1.0, which includes lack of client API's
* Qpid does support AMQP 1.0 but the API's are a bit clumsy.  Qpid messaging API supports AMQP 0.10 and 0.9.1 but not 1.0.   Proton supports 0.10 and 1.0 but not 0.9.1
* Proton AMQP 1.0 does work with RabbitMQ experimental plugin, but RabbitMQ's implementation is very poor in terms of the management API (no tracking of connections since AMQP 1.0 does not define channels/exchanges like 0.9.1 did).  QPid has a better implementation of 1.0 in terms of tracking and management... but qpid is not as easy to install as RabbitMQ
* RabbitMQ-c  API only supports 0.9.1, which doesn't work with QPid java broker SASL authentication. Maintainer of RabbitMQ-c mentions he only validates/tests with RabbitMQ
* Throughput tests showed that RabbitMQ and Qpid were roughly the same.  A single node can handle about 20K messages per second (1 producer and 1 to 2 consumers)
* AMQP 1.0 is standard but Pivotal/RabbitMQ is causing confusion by keeping 0.9.1 alive, which is not compatible with 1.0
* RabbitMQ and QPid do not support consumers at different rates well

### Key Reasons to use Apache Kafka

* Open source and current
* Has multiple client language API’s (librdkafka works well for C/C++)* Supports over 100K messages per second (producer with consumer) on a single node and supports millions of messages per second with cluster
* Meets requirements plus more* Supports varying consumer rates by allowing the customer to track where it is in the queue/offset* Uses disk for persistence and message* Consumers can be restarted and resume where they left off* Producer can control load balancing by partition selection* Consumers can control if they should be load balanced or not (same group)* Easy install and good documentation
