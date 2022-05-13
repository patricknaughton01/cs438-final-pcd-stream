#ifndef POINT_H
#define POINT_H

class Point{
public:
    float x, y, z;
    unsigned short r, g, b;
    Point(float _x, float _y, float _z,
        unsigned short _r, unsigned short _g, unsigned short _b):
        x(_x), y(_y), z(_z), r(_r), g(_g), b(_b){};
    Point(const Point &p);
    float d_dist(const Point &p) const;
    float c_dist(const Point &p) const;
};

#endif // POINT_H
