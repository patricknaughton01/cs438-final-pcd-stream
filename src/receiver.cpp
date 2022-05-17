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
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
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

Point deserialize(const unsigned char* buf) {
    float x, y, z;
    unsigned char r, g, b;
    float * d_start = (float*)buf;
    x = *d_start;
    y = *(d_start + 1);
    z = *(d_start + 2);

    unsigned char *c_start = (unsigned char*) (buf + 3 * sizeof(float));

    r = *c_start;
    g = *(c_start + 1);
    b = *(c_start + 2);

    return Point(x, y, z, r, g, b);
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
        unsigned char* start = pkt->data + (i * Point::s_buf_size);
        Point point = deserialize(start);
        point.ts = pkt->ts;
        Cell c = vg->get_cell_of_point(point);
        if(vg->grid.find(c) == vg->grid.end() || vg->grid.at(c).ts <= point.ts){
            vg->add_point(point);
        }
    }

}


void reliablyReceive(unsigned short int myUDPport, VoxelGrid *vg, std::mutex &vg_lock) {
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
        // std::cout << "SOCKET: " << s << std::endl;
        // std::cout << "PKT ARGS: " << &pkt << " " << sizeof(pkt) << std::endl;
        // std::cout << "ADDR ARGS: " << (struct sockaddr*) &si_other << " " <<  &from_len << std::endl;
        int n_r_bytes = recvfrom(s, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_other, &from_len);
        // std::cout << "Received " << n_r_bytes << std::endl;
        // std::cout << "Received from: " << inet_ntoa(si_other.sin_addr) << std::endl;
        // std::cout << "From len: " << from_len << std::endl;
        // std::cout << "Packet len: " << pkt.len << std::endl;
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
        vg_lock.lock();
        unpack_packet(&pkt, vg);
        vg_lock.unlock();

        // std::cout << "Send ack" << std::endl;
        if(in_len == 0){
            // End of transmission
            std::cout << "Breaking out of loop" << std::endl;
            break;
        }
    }
    close(s);
    return;
}


void _savePcd(const std::unordered_map<Cell, Point, CellHashFunction> &grid,
    int count, std::string dir)
{
    std::string path = dir + "/" + "pcd_" + std::to_string(count) + ".pcd";
    std::ofstream os(path);
    os << "VERSION 0.7" << std::endl << "FIELDS x y z rgb" << std::endl
        << "SIZE 4 4 4 4" << std::endl << "TYPE F F F U" << std::endl
        << "WIDTH " << grid.size() << std::endl << "HEIGHT 1" << std::endl
        << "POINTS " << grid.size() << std::endl
        << "DATA ascii" << std::endl;
    for(auto it = grid.begin(); it != grid.end(); it++){
        Point p = it->second;
        unsigned int color = (p.r << 16) | (p.g << 8) | p.b;
        os << p.x << " " << p.y << " " << p.z << " " << color << std::endl;
    }
    os.close();
    count++;
}

void savePcd(VoxelGrid *vg, size_t dt, std::mutex &vg_lock, bool &finished,
    std::string dir, int *count)
{
    *count = 0;
    while(!finished){
        std::this_thread::sleep_for(std::chrono::milliseconds(dt));
        vg_lock.lock();
        std::unordered_map<Cell, Point, CellHashFunction> grid = vg->grid;
        vg_lock.unlock();
        _savePcd(grid, *count, dir);
        (*count)++;
    }
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
    VoxelGrid vg(0.001);
    bool finished = false;

    udpPort = (unsigned short int) atoi(argv[1]);
    std::mutex vg_lock;
    int count = 0;
    std::thread save_thread(savePcd, &vg, 1000, std::ref(vg_lock),
        std::ref(finished), "saved_pcd", &count);
    reliablyReceive(udpPort, &vg, vg_lock);
    finished = true;
    save_thread.join();
    _savePcd(vg.grid, count, "saved_pcd");
    return 0;
}
