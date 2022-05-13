/*
 * File:   sender.cpp
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <list>
#include <chrono>
#include <deque>
#include "packet.h"
#include "point.h"
#include "v_grid.h"

struct sockaddr_in si_other;
int s, slen;
unsigned int from_len;
unsigned int MAXSNUM = -1;
unsigned int MAXW = 1 << 20;
void pack_packet(FILE *fp, unsigned long long int *start,
    unsigned long long int numbytes, unsigned int *seq, packet* pkt);
template <class T>
T min(T a, T b);
double get_timeout(double est_rtt, double dev_rtt, double gamma);
bool ack_applies_seq(const std::list<packet> &pkt_buf, unsigned int ack);
template <class T>
void print(T s, T e);

void diep(char *s) {
    perror(s);
    exit(1);
}



void reliablyTransfer(char* hostname, unsigned short int hostUDPport,
    char* filename, unsigned long long int bytesToTransfer)
{
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    int recv_bytes;
    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }


	/* Send data and receive acknowledgements on s*/
    double frac_window_size = 128.0;
    unsigned int window_size = (unsigned int) frac_window_size;
    unsigned int ss_thresh = 128;
    unsigned int next_seq = 1;
    unsigned long long int byte_start = 0;
    double est_rtt = 50; // us
    double dev_rtt = 0; // us
    double alpha = 0.125;
    double beta = 0.25;
    double gamma = 4;
    double timeout = get_timeout(est_rtt, dev_rtt, gamma); // in us
    std::list<packet> pkt_buf;
    std::deque<unsigned long long> time_stamps;
    std::deque<unsigned long long> orig_time_stamps;
    bool added_end_pkt = false;
    while(true){
        if(added_end_pkt && pkt_buf.size() == 0){
            break;
        }
        packet pkt;
        // Try to send available packets in valid window
        if(pkt_buf.size() < window_size && !added_end_pkt){
            // The packet we're about to create and send is the end packet
            if(byte_start >= bytesToTransfer){
                added_end_pkt = true;
            }
            pack_packet(fp, &byte_start, bytesToTransfer, &next_seq, &pkt);
            sendto(s, &pkt, sizeof(packet), 0,
                (const struct sockaddr*) &si_other, sizeof(si_other));
            // Track in-flight packets
            pkt_buf.push_back(pkt);
            unsigned long long now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            time_stamps.push_back(now);
            orig_time_stamps.push_back(now);
        }
        // Receive any incoming acks
        if((recv_bytes = recvfrom(s, &pkt, sizeof(packet), 0,
            (struct sockaddr*) &si_other, &from_len)) > 0)
        {
            // Handle incoming acks
            while(pkt_buf.begin() != pkt_buf.end() && ack_applies_seq(pkt_buf, pkt.ack)){
                unsigned int seq = pkt_buf.begin()->seq;
                std::cout << "Looking at seq " << seq << std::endl;
                pkt_buf.pop_front();
                unsigned long ts = orig_time_stamps[0];
                time_stamps.pop_front();
                orig_time_stamps.pop_front();
                if(seq == pkt.ack){
                    unsigned long long now = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    double dt = (double)(now - ts);
                    dev_rtt = (1 - beta) * dev_rtt + beta * std::abs(dt - est_rtt);
                    est_rtt = (1 - alpha) * est_rtt + alpha * dt;
                    std::cout << "est rtt: " << est_rtt << std::endl;
                    std::cout << "dev rtt: " << dev_rtt << std::endl;
                    timeout = get_timeout(est_rtt, dev_rtt, gamma);
                    std::cout << "New timeout " << timeout << std::endl;
                    // Also update congestion window
                    double cand_w = frac_window_size + 1;
                    if(cand_w > ss_thresh){
                        cand_w = frac_window_size + 1 / frac_window_size;
                    }
                    frac_window_size = cand_w;
                    window_size = min((unsigned int) frac_window_size, MAXW);
                    std::cout << "Window size: " << frac_window_size << " " << window_size << std::endl;
                }
            }
        }
        else{
            std::cout << "Error: " << strerror(errno) << std::endl;
        }
        // Check for timeouts
        unsigned long long now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        if(time_stamps.size() > 0 && (now - time_stamps[0]) > timeout){
            // Resend
            std::cout << "Hit timeout, resending" << std::endl;
            ss_thresh = ((unsigned int) frac_window_size / 2) + 1;
            frac_window_size = 1.0;
            window_size = (unsigned int) frac_window_size;
            std::cout << "Reset window, new thresh: " << ss_thresh << std::endl;
            unsigned int ind = 0;
            for(auto it = pkt_buf.begin(); it != pkt_buf.end(); it++){
                packet pkt = *it;
                sendto(s, &pkt, sizeof(packet), 0,
                    (const struct sockaddr*) &si_other, sizeof(si_other));
                time_stamps[ind] = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                ind++;
            }
        }
    }

    printf("Closing the socket\n");
    close(s);
    return;

}

void pack_packet(FILE *fp, unsigned long long *start,
    unsigned long long numbytes, unsigned int *seq, packet* pkt)
{
    pkt->seq = *seq;
    *seq += 1;
    pkt->len = min(numbytes - *start, (unsigned long long) MAXPAYLOADSIZE);
    pkt->is_ack = false;
    fread(&(pkt->data), sizeof(char), pkt->len, fp);
    *start += pkt->len;
}

template <class T>
T min(T a, T b){
    if(a < b){
        return a;
    }
    return b;
}

double get_timeout(double est_rtt, double dev_rtt, double gamma){
    return est_rtt + gamma * dev_rtt;
}

bool ack_applies_seq(const std::list<packet> &pkt_buf, unsigned int ack){
    unsigned int seq = pkt_buf.begin()->seq;
    unsigned int w = pkt_buf.size();
    if (MAXSNUM - seq < w){
        if(ack < w - (MAXSNUM - seq) || seq <= ack){
            return true;
        }
    }
    else if(seq <= ack && ack < seq + w){
        return true;
    }
    return false;
}

template <class T>
void print(T s, T e){
    for(auto iter = s; iter != e; iter++){
        std::cout << *iter << " ";
    }
    std::cout << std::endl;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}
