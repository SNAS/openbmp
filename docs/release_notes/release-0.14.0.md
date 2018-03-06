Release Notes - Version 0.14.0
==============================
Stability fixes and enhancements to include loc-rib, adj-rib-out, and large communities. Added message
bus type header to support topic multiplexing.  


Change Log
----------------

#### New Features

* Message Bus Schema now version 1.7
* Added large community [RFC8092](https://tools.ietf.org/html/rfc8092) support
* Added librdkafka queue.buffering.max.kbytes config
* Enhanced detection of peer encoding of ASN as 2 octet verses 4 octet
* Updated librdkafka configuration to request api version for 
  support of new/old kafka versions
* Added support for [draft-ietf-grow-bmp-local-rib](https://tools.ietf.org/html/draft-ietf-grow-bmp-local-rib-01)
* Added support for [draft-ietf-grow-bmp-adj-rib-out](https://tools.ietf.org/html/draft-ietf-grow-bmp-adj-rib-out-01)
* Added T (object type) header field for msgbus messages
* Added support to disable topics by configuring the topic as either null or ''
* 

#### Fixes

* Fixed issue with handling extended attribute length
* Fixed complier issues with changes with librdkafka
* When in v4v6 mode, resolved issue with polling resulting in many POLLERR log messages
* Adjusted kafka default config to resolve too large of message issue
* Fixed race condition causing segfault during router disconnect
* Changed unsupported attributes log warn to debug
* Added error handling of BMP header microsecond values
* Fixed issue with segfault caused by empty sysName with JunOS


