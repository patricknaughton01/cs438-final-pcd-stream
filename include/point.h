#ifndef POINT_H
#define POINT_H

class Point{
private:
    bool has_serialized;
    unsigned char *m_s_buf;
public:
    float x, y, z;
    unsigned char r, g, b;
    unsigned long long ts;
    static const unsigned int s_buf_size =
        3 * sizeof(float) + 3 * sizeof(unsigned char);
    Point(float _x, float _y, float _z,
        unsigned char _r, unsigned char _g, unsigned char _b):
        x(_x), y(_y), z(_z), r(_r), g(_g), b(_b), ts(0),
        has_serialized(false){};
    Point(const Point &p);
    ~Point();
    float d_dist(const Point &p) const;
    float c_dist(const Point &p) const;
    const unsigned char * const serialize();
    const unsigned char * const s_buf() const;
};

#endif // POINT_H
