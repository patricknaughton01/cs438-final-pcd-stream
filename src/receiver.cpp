/*
 * File:   receiver.cpp
 * Author: Patrick Naughton
 *
 * Initially copied from MP2. Modified May 8 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <chrono>
#include "packet.h"
#include "point.h"
#include "v_grid.h"

/**
 * @brief Need to update vg with whatever points come in from sender,
 * what to output at the end
 * 
 */

VoxelGrid * vg;


struct sockaddr_in si_me, si_other;
int s, slen;
unsigned int from_len;

void diep(char *s) {
    perror(s);
    exit(1);
}

Point * const deserialize(const char* buf) {
    float x, y, z;
    unsigned short r, g, b; 
    unsigned long long ts;

    float * d_start = (float*)buf;
    x = *d_start;
    y = *(d_start + 1);
    z = *(d_start + 1);

    unsigned int *c_start = (unsigned int*) (buf + 2 * sizeof(float));

    r = *c_start;
    g = *(c_start + 1);
    b = *(c_start + 2);

    return new Point(x, y, z, r, g, b);
}


/**
 * @brief Takes a packet with multiple points and adds
 * the points to the voxel grid
 * 
 * @param pkt: pointer to the packet with the points to unpack
 * @param vg: the voxel grid to update
 */
void unpack_packet(packet * pkt, VoxelGrid * vg) {
    float x, y, z;
    unsigned short r, g, b;

    int numPoints = pkt->len / Point::s_buf_size;
    for(int i = 0; i < numPoints; i++){
        
        char* start = pkt->data + (i * Point::s_buf_size);
        Point& point = *(deserialize(start));
        point.ts = pkt->ts;
        
        vg->add_point(point);
        
    }

}



void reliablyReceive(unsigned short int myUDPport, VoxelGrid * vg) {

    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");


    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */
    unsigned int exp_seq = 1;
    while(true){
        packet pkt;
        std::cout << "SOCKET: " << s << std::endl;
        std::cout << "PKT ARGS: " << &pkt << " " << sizeof(pkt) << std::endl;
        std::cout << "ADDR ARGS: " << (struct sockaddr*) &si_other << " " <<  &from_len << std::endl;
        int n_r_bytes = recvfrom(s, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_other, &from_len);
        std::cout << "Received " << n_r_bytes << std::endl;
        std::cout << "Received from: " << inet_ntoa(si_other.sin_addr) << std::endl;
        std::cout << "From len: " << from_len << std::endl;
        std::cout << "Packet len: " << pkt.len << std::endl;
        if(n_r_bytes < 0){
            std::cout << "Recv Error: " << strerror(errno) << std::endl;
            continue;
        }
        else if(n_r_bytes == 0){
            std::cout << "Connection was closed" << std::endl;
            break;
        }
        unsigned int in_len = pkt.len;
        std::cout << std::endl;
        std::cout << "Exp seq " << exp_seq << " Actual: " << pkt.seq << std::endl;
        packet_ack pkt_ack;
        pkt_ack.ack = pkt.seq;
        sendto(s, &pkt_ack, sizeof(pkt_ack), 0, (const struct sockaddr*) &si_other, sizeof(si_other));
        unpack_packet(&pkt, vg);

        std::cout << "Send ack" << std::endl;
        if(in_len == 0){
            // End of transmission
            std::cout << "Breaking out of loop" << std::endl;
            break;
        }
    }

    

    close(s);

    std::cout << vg->grid.size() << std::endl;
    vg->print_points();

    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 2) {
        fprintf(stderr, "usage: %s UDP_port\n\n", argv[0]);
        exit(1);
    }

    //Use default arg res = .1
    vg = new VoxelGrid();

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, vg);
}
