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
#include <string>
#include <sys/time.h>
#include <iostream>
#include <list>
#include <chrono>
#include <deque>
#include <thread>
#include <mutex>
#include <fstream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "packet.h"
#include "point.h"
#include "v_grid.h"

#define TF_FN "transforms.txt"
#define INTRINSICS_FN "intrinsics.txt"
#define NUM_FN "num.txt"
#define IMG_DIR "imgs"
#define CN "realsense_overhead_5_l515"
#define D_SUF "_depth"
#define C_SUF "_color"

struct sockaddr_in si_other;
int s, slen;
unsigned int from_len;
unsigned int MAXSNUM = -1;
unsigned int MAXW = 1 << 20;
void pack_packet(std::deque<Point> &points, unsigned int *seq, packet* pkt);
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
    std::deque<Point> &points, std::mutex &points_lock, bool &finished)
{
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
    std::list<unsigned long long> time_stamps;
    std::list<unsigned long long> orig_time_stamps;
    bool added_end_pkt = false;
    while(true){
        points_lock.lock();
        if(points.empty() && !finished){
            points_lock.unlock();
            std::cout << "SLEEPING" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if(finished && points.empty() && pkt_buf.empty()){
            std::cout << "Breaking out of loop" << std::endl;
            points_lock.unlock();
            std::cout << "Hitting break" << std::endl;
            break;
        }
        packet pkt;
        // Try to send available packets in valid window
        if(pkt_buf.size() < window_size && !points.empty()){
            pack_packet(points, &next_seq, &pkt);
            std::cout << "Packet len: " << pkt.len << std::endl;
            points_lock.unlock();
            sendto(s, &pkt, sizeof(packet), 0,
                (const struct sockaddr*) &si_other, sizeof(si_other));
            // Track in-flight packets
            pkt_buf.push_back(pkt);
            unsigned long long now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            time_stamps.push_back(now);
            orig_time_stamps.push_back(now);
        }else{
            points_lock.unlock();
        }
        // Receive any incoming acks
        packet_ack pkt_ack;
        if((recv_bytes = recvfrom(s, &pkt_ack, sizeof(packet_ack), 0,
            (struct sockaddr*) &si_other, &from_len)) > 0)
        {
            // Handle incoming acks

            // while(pkt_buf.begin() != pkt_buf.end() && ack_applies_seq(pkt_buf, pkt.ack)){
            auto it = pkt_buf.begin();
            auto ts_it = time_stamps.begin();
            auto orig_ts_it = orig_time_stamps.begin();
            while(it != pkt_buf.end()){
                unsigned int seq = pkt_buf.begin()->seq;
                std::cout << "Looking at seq " << seq << std::endl;
                if(seq == pkt_ack.ack){
                    it = pkt_buf.erase(it);
                    unsigned long ts = *orig_ts_it;
                    ts_it = time_stamps.erase(ts_it);
                    orig_ts_it = orig_time_stamps.erase(orig_ts_it);

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
                }else{
                    it++; ts_it++; orig_ts_it++;
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
        if(time_stamps.size() > 0 && (now - time_stamps.front()) > timeout){
            // Resend
            std::cout << "Hit timeout, resending" << std::endl;
            ss_thresh = ((unsigned int) frac_window_size / 2) + 1;
            frac_window_size = 1.0;
            window_size = (unsigned int) frac_window_size;
            std::cout << "Reset window, new thresh: " << ss_thresh << std::endl;
            auto ts_it = time_stamps.begin();
            for(auto it = pkt_buf.begin(); it != pkt_buf.end(); it++){
                packet pkt = *it;
                sendto(s, &pkt, sizeof(packet), 0,
                    (const struct sockaddr*) &si_other, sizeof(si_other));
                *ts_it = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                ts_it++;
            }
        }
    }

    // Transmit end packet and listen for ack
    size_t remaining_tries = 10;
    unsigned long long last_sent = 0;
    packet end_pkt;
    while(true){
        if(remaining_tries == 0){
            std::cerr << "Ran out of tries to send end packet" << std::endl;
            break;
        }
        unsigned long long now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        if(now - last_sent >= get_timeout(est_rtt, dev_rtt, gamma)){
            end_pkt.seq = next_seq;
            end_pkt.len = 0;
            sendto(s, &end_pkt, sizeof(packet), 0,
                (const struct sockaddr*) &si_other, sizeof(si_other));
            last_sent = now;
            remaining_tries--;
        }
        packet_ack pkt_ack;
        if(recvfrom(s, &pkt_ack, sizeof(packet_ack), 0,
            (struct sockaddr*) &si_other, &from_len) > 0)
        {
            if(pkt_ack.ack == next_seq){
                break;
            }
        }
    }

    printf("Closing the socket\n");
    close(s);
    return;
}


/**
 * @brief Packs data from points into a packet. Will stop when the packet is
 * full (MAXPAYLOADSIZE reached) or runs out of points to send. Assumes no
 * other thread will interfere with `points`.
 *
 * @param points Points to send.
 * @param seq Pointer to sequence number to use, automatically incremented.
 * @param pkt Pointer to packet to update.
 */
void pack_packet(std::deque<Point> &points, unsigned int *seq, packet* pkt)
{
    pkt->seq = *seq;
    *seq += 1;
    pkt->ts = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    unsigned int len = 0;
    while(len < MAXPAYLOADSIZE && !points.empty()){

        points.front().serialize();
        for(size_t i = 0; i < Point::s_buf_size; i++){
            pkt->data[len] = points.front().s_buf()[i];
            len += 1;
        }
        points.pop_front();
    }
    pkt->len = len;
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


void load_intrinsics(std::vector<double> &intrinsics, std::ifstream &i_stream){
    for(size_t i = 0; i < 4; i++){
        double _int;
        i_stream >> _int;
        intrinsics.push_back(_int);
    }
}


void load_tf(std::vector<double> &R, std::vector<double> &p, std::ifstream &tf_stream){
    for(size_t i = 0; i < 9; i++){
        double _r;
        tf_stream >> _r;
        R.push_back(_r);
    }
    for(size_t i = 0; i < 3; i++){
        double _p;
        tf_stream >> _p;
        p.push_back(_p);
    }
}


Point load_point(cv::Vec3b color, unsigned int depth, double x, double y,
    std::vector<double> intrinsics, std::vector<double> R,
    std::vector<double> p)
{
    double fx = intrinsics[0], fy = intrinsics[1], cx = intrinsics[2],
        cy = intrinsics[3];
    double z_c = (double) depth;
    double x_c = z_c * (x - cx) / fx;
    double y_c = z_c * (y - cy) / fy;
    double x_w = R[0] * x_c + R[3] * y_c + R[6] * z_c + p[0];
    double y_w = R[1] * x_c + R[4] * y_c + R[7] * z_c + p[1];
    double z_w = R[2] * x_c + R[5] * y_c + R[8] * z_c + p[2];
    Point point(x_w, y_w, z_w, color[0], color[1], color[2]);
    return point;
}


void load_points(const std::string &dir, std::deque<Point> &points,
    std::mutex &points_lock)
{
    VoxelGrid vg(0.1);
    std::string int_path = dir + "/" + IMG_DIR + "/" + INTRINSICS_FN;
    std::vector<double> intrinsics;
    std::ifstream int_stream(int_path);
    load_intrinsics(intrinsics, int_stream);
    std::string tf_path = dir + "/" + TF_FN;
    std::ifstream tf_stream(tf_path);
    std::string num_path = dir + "/" + NUM_FN;
    std::ifstream num_stream(num_path);
    size_t num;
    num_stream >> num;
    std::string color_pref = dir + "/" + IMG_DIR + "/" + CN + C_SUF + "/";
    std::string depth_pref = dir + "/" + IMG_DIR + "/" + CN + D_SUF + "/";
    for(size_t i = 0; i < num; i++){
        std::string c_fn = std::to_string(i) + ".jpg";
        std::string d_fn = std::to_string(i) + ".png";
        std::string c_path = color_pref + c_fn;
        std::string d_path = depth_pref + d_fn;
        cv::Mat c_image = cv::imread(c_path);
        cv::Mat d_image = cv::imread(d_path, cv::IMREAD_ANYDEPTH | cv::IMREAD_GRAYSCALE);
        std::vector<double> R, p;
        load_tf(R, p, tf_stream);
        for(size_t r = 0; r < c_image.rows; r++){
            for(size_t c = 0; c < c_image.cols; c++){
                unsigned int depth = d_image.at<unsigned int>(r, c);
                if(depth > 0){
                    Point point = load_point(c_image.at<cv::Vec3b>(r, c),
                        depth, (double) c, (double) r, intrinsics, R, p);
                    if(vg.add_point(point)){
                        points_lock.lock();
                        points.push_back(point);
                        points_lock.unlock();
                    }
                }
            }
        }
    }
}

void test(std::deque<Point> &points, std::mutex &points_lock) {
    VoxelGrid vg(0.1);
    for (int i = 0; i < 10; i++) {

        Point point(i,1,1,2,2,2); 
        vg.add_point(point);
        points_lock.lock();
        points.push_back(point);
        points_lock.unlock();
    } 
    for (int i = 0; i < 10; i++) {
        Point point(i,1,1,20,2,2);
        vg.add_point(point);
        points_lock.lock();
        points.push_back(point);
        points_lock.unlock();
    }
    for (int i = 0; i < 10; i++) {
        Point point(i,1,1,40,2,2);
        vg.add_point(point);
        points_lock.lock();
        points.push_back(point);
        points_lock.unlock();
    }
    
}


int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 4) {
        fprintf(stderr,
            "usage: %s receiver_hostname receiver_port dir_name\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);

    std::mutex points_lock;
    std::deque<Point> points;
    bool finished_flag = false;
    std::thread transfer_thread(reliablyTransfer, argv[1], udpPort,
        std::ref(points), std::ref(points_lock), std::ref(finished_flag));
    test(points, points_lock);
    //load_points(argv[3], points, points_lock);
    finished_flag = true;
    transfer_thread.join();

    return (EXIT_SUCCESS);
}
