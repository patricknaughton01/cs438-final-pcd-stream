#ifndef POINT_H
#define POINT_H

class Point{
    float x, y, z;
    unsigned short r, g, b;
public:
    Point(float _x, float _y, float _z,
        unsigned short _r, unsigned short _g, unsigned short _b):
        x(_x), y(_y), z(_z), r(_r), g(_g), b(_b){};
};

#endif // POINT_H
