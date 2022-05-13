#include <cmath>
#include "point.h"


Point::Point(const Point &p){
    x = p.x; y = p.y; z = p.z; r = p.r; g = p.g; b = p.b;
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


const char * const Point::serialize(){
    if(!has_serialized){
        m_s_buf = (char*)malloc(s_buf_size);
    }
    float *d_start = (float*)m_s_buf;
    *d_start = x;
    *(d_start + 1) = y;
    *(d_start + 2) = z;
    unsigned int f_size = sizeof(float), i_size = sizeof(unsigned int);
    unsigned int *c_start = ((unsigned int*)m_s_buf) + 3 * f_size / i_size;
    *c_start = r;
    *(c_start + 1) = g;
    *(c_start + 2) = b;
    has_serialized = true;
    return m_s_buf;
}


const char * const Point::s_buf() const{
    return m_s_buf;
}
