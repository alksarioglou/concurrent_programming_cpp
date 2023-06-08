# Concurrent Multi-thread Programming Project in C++

This project was concerned with simulating some aspects of a simple embedded computer system. 

The system designed is responsible for gathering environmental data via a single
analogue-to-digital converter (ADC), and transmitting it to another computer system via one of 
three communications links. The software in the embedded system consists initially of six
threads, each of which is responsible for gathering data on different input channels, temporarily 
storing it, and then transmitting it to the other computer system in blocks. In order to acquire 
data, each thread must gain sole access to the ADC, configure it to sample from a specific 
channel, and sample the channel. In order to transmit a block of data, a thread must gain access to 
one of the communications links. Having gained access, it sends its block of data, and then 
releases the link. Each thread may use any communications link, but once it has gained access to 
a particular link, no other thread should be allowed to use that link until it is released. 

