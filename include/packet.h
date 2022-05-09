#ifndef PACKET_H
#define PACKET_H

#include <stdbool.h>
#define MAXPAYLOADSIZE 1350

typedef struct packet{
    unsigned int seq;
    unsigned int ack;
    unsigned int len;
    bool is_ack;
    char data[MAXPAYLOADSIZE];
} packet;

unsigned int H_SIZE = 4 * sizeof(unsigned int) + sizeof(bool);

#endif // PACKET_H
