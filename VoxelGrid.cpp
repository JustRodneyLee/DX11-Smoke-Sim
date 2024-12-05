#include "VoxelGrid.hpp"

template <typename T>
VoxelGrid<T>::VoxelGrid(int w, int h, int d) {
	grid = vector<T>(w, vector<T>(h, vector<T>(d, 0)));
}