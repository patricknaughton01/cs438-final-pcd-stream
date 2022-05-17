#include <cmath>
#include <stdlib.h>
#include <iostream>
#include "point.h"


Point::Point(const Point &p){
    x = p.x; y = p.y; z = p.z; r = p.r; g = p.g; b = p.b;
    has_serialized = false;
    if(p.has_serialized){
        serialize();
    }
}


Point::~Point(){
    if(has_serialized){
        free(m_s_buf);
    }
}


float Point::d_dist(const Point &p) const{
    return std::sqrt(std::pow(x - p.x, 2)
        + std::pow(y - p.y, 2)
        + std::pow(z - p.z, 2));
}


float Point::c_dist(const Point &p) const{
    return std::sqrt(std::pow(r - p.r, 2)
        + std::pow(g - p.g, 2)
        + std::pow(b - p.b, 2));
}


const unsigned char * const Point::serialize(){
    if(!has_serialized){
        m_s_buf = (unsigned char*)malloc(s_buf_size);
    }
    // std::cout << "Has serialized " << has_serialized << std::endl;
    // std::cout << "Writing numbers" << std::endl;
    // printf("%p\n", m_s_buf);
    // std::cout << m_s_buf << std::endl;
    // std::cout << "Buff size " << s_buf_size << std::endl;
    float *d_start = (float*)m_s_buf;
    // std::cout << m_s_buf << std::endl;
    // std::cout << d_start << std::endl;
    // std::cout << d_start + 1 << std::endl;
    // std::cout << d_start + 2 << std::endl;
    *d_start = x;
    *(d_start + 1) = y;
    *(d_start + 2) = z;

    std::cout << "Writing color" << std::endl;
    unsigned char *c_start = (unsigned char*) (m_s_buf + 3 * sizeof(float));
    *c_start = r;
    *(c_start + 1) = g;
    *(c_start + 2) = b;

    has_serialized = true;
    return m_s_buf;
}


const unsigned char * const Point::s_buf() const{
    return m_s_buf;
}
