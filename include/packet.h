#ifndef PACKET_H
#define PACKET_H

#include <stdbool.h>
#define MAXPAYLOADSIZE 1200

//Only used for non-ACK packets
typedef struct packet{
    unsigned long long ts;
    unsigned int seq;
    unsigned int len;
    unsigned char data[MAXPAYLOADSIZE];
} packet;

//Client can send minimal data back in the ack
typedef struct packet_ack{
    unsigned int ack;
} packet_ack;


unsigned int H_SIZE = 4 * sizeof(unsigned int) + sizeof(bool);

#endif // PACKET_H
