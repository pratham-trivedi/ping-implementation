# Ping

Made for Advanced Computer Network Course.  
This implementation of Ping uses ICMP (Internet Control Message Protocol) to send echo requests and receive echo replies, measuring the round-trip time and packet loss statistics.

## How to Run

> **Note**: This program works only on Unix-based systems due to the use of raw sockets.

```
gcc -o <output> ping.c
sudo ./<output> <siteaddress or ip>
```
Change "output" with any name you want.

Sudo is necessary as raw socket need root privilage. 
