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

    //TODO: assumes that there's no Point spread across 2 packets, correct?
    int numPoints = pkt->len / Point::s_buf_size;
    for(int i = 0; i < numPoints; i++){
        
        char* start = pkt->data + (i * Point::s_buf_size);
        Point * point = Point::deserialize(start);
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
    //FILE *fp = fopen(destinationFile, "wb");
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
        for(int i = 0; i < pkt.len; i++){
            std::cout << pkt.data[i];
        }
        std::cout << std::endl;
        std::cout << "Exp seq " << exp_seq << " Actual: " << pkt.seq << std::endl;
        packet_ack pkt_ack;
        if(pkt.seq == exp_seq){
            pkt_ack.ack = pkt.seq;
            exp_seq++;
            unpack_packet(&pkt, vg);
        }
        else{
            pkt_ack.ack = exp_seq - 1;
        }

        sendto(s, &pkt_ack, sizeof(pkt_ack), 0, (const struct sockaddr*) &si_other, sizeof(si_other));
        std::cout << "Send ack" << std::endl;
        if(in_len == 0){
            // End of transmission
            std::cout << "Breaking out of loop" << std::endl;
            break;
        }
    }

    // Wait time in ms after receiving end of file
    unsigned long end_timeout = 100;
    unsigned long end_start = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;
    // Set timeout on receiving - not actually expecting any more packets.
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    // Ack any remaining incoming packets
    while(true){
        unsigned long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        if(now - end_start > end_timeout){
            break;
        }
        packet pkt;
        packet_ack pkt_ack;
        unsigned int from_len;
        int n_r_bytes = recvfrom(s, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_other, &from_len);
        //std::cout << "Received " << n_r_bytes << std::endl;
        if(n_r_bytes < 0){
            //std::cout << "Error: " << strerror(errno) << std::endl;
            continue;
        }
        else if(n_r_bytes == 0){
            //std::cout << "Connection was closed" << std::endl;
            break;
        }
        pkt_ack.ack = pkt.seq;

        sendto(s, &pkt_ack, sizeof(pkt_ack), 0, (const struct sockaddr*) &si_other, sizeof(si_other));
    }

    close(s);

    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port\n\n", argv[0]);
        exit(1);
    }

    //Use default arg res = .1
    vg = new VoxelGrid();

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, &vg);
}
