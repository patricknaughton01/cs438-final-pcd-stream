#include <cmath>
#include "point.h"


Point::Point(const Point &p){
    x = p.x; y = p.y; z = p.z; r = p.r; g = p.g; b = p.b;
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
