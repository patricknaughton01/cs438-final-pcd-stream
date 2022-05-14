#ifndef V_GRID_H
#define V_GRID_H

#define D_THRESH_DEN 2.0
#define C_THRESH 10.0

#include <utility>
#include <unordered_map>
#include "point.h"

typedef std::tuple<int, int, int> Cell;
struct CellHashFunction{
    size_t operator()(const Cell &c) const{
        return 8191 * std::hash<int>()(std::get<0>(c))
            + 127 * std::hash<int>()(std::get<1>(c))
            + std::hash<int>()(std::get<2>(c));
    }
};

class VoxelGrid{
public:
    double res;
    double d_thresh;
    double c_thresh;
    std::unordered_map<Cell, Point, CellHashFunction> grid;
    VoxelGrid(double _res): res(_res), d_thresh(_res / D_THRESH_DEN),
        c_thresh(C_THRESH){};
    bool add_point(const Point &p);
    Cell get_cell_of_point(const Point &p) const;
};

#endif // V_GRID_H
