#ifndef POINT_H
#define POINT_H

class Point{
private:
    bool has_serialized;
    char *m_s_buf;
public:
    float x, y, z;
    unsigned short r, g, b;
    static const unsigned int s_buf_size =
        3 * sizeof(float) + 3 * sizeof(unsigned short);
    Point(float _x, float _y, float _z,
        unsigned short _r, unsigned short _g, unsigned short _b):
        x(_x), y(_y), z(_z), r(_r), g(_g), b(_b),
        has_serialized(false){};
    Point(const Point &p);
    ~Point();
    float d_dist(const Point &p) const;
    float c_dist(const Point &p) const;
    const char * const serialize();
    const char * const s_buf() const;
    static Point * const deserialize(const char *);
};

#endif // POINT_H
