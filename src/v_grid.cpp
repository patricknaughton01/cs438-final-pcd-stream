#include <cmath>
#include <iostream>
#include "v_grid.h"


/**
 * @brief Adds a point to the voxel grid if the point's cell is empty or
 * or if the timestamp is more recent and it is sufficiently 
 * different from the current point in its cell.
 *
 * @param p Point to add.
 * @return bool Whether or not the voxel grid was changed.
 */
bool VoxelGrid::add_point(const Point &p){
    Cell c = get_cell_of_point(p);
    if(grid.find(c) == grid.end()){
        // No point in this cell yet, add this point.
        grid.emplace(c, p);
        return true;
    }
    const Point &curr_p = grid.at(c);
    if(p.ts >= curr_p.ts && (p.d_dist(curr_p) > d_thresh || p.c_dist(curr_p) > c_thresh)){
        grid.at(c) = p;
        return true;
    }
    return false;
}


/**
 * @brief Gets the cell corresponding to a point given the grid's resolution.
 *
 * @param p Query point.
 * @return Cell Cell the point corresponds to.
 */
Cell VoxelGrid::get_cell_of_point(const Point &p) const{
    return {
        (int)(std::floor(p.x / res)),
        (int)(std::floor(p.y / res)),
        (int)(std::floor(p.z / res))
    };
}

void VoxelGrid::print_points() {
    for (auto& it: grid) {
        Point pt = it.second;
        std::cout << pt.r << "," << pt.g << "," << pt.b << std::endl;
    }
}


