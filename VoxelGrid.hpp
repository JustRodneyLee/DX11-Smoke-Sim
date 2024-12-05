#include <vector>
#include "Vector3.hpp"

template <typename T>
class VoxelGrid {
public:
    VoxelGrid(int w = 16, int h = 16, int d = 16);
    T& At(const Vector3& pos);
    const T& At(const Vector3& pos) const;

    T& At(int x, int y, int z);
    const T& At(int x, int y, int z) const;

    T* Data();
    const T* Data() const;

private:
    std::vector<T> grid; // Single flat vector
    int width, height, depth;

    int Index(int x, int y, int z) const; // Helper to compute 1D index
};

template <typename T>
VoxelGrid<T>::VoxelGrid(int w, int h, int d)
    : width(w), height(h), depth(d), grid(w* h* d, T{}) {}

template <typename T>
T& VoxelGrid<T>::At(const Vector3& pos) {
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    int z = static_cast<int>(pos.z);
    return grid[Index(x, y, z)];
}

template <typename T>
const T& VoxelGrid<T>::At(const Vector3& pos) const {
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    int z = static_cast<int>(pos.z);
    return grid[Index(x, y, z)];
}

template <typename T>
T& VoxelGrid<T>::At(int x, int y, int z) {
    return grid[Index(x, y, z)];
}

template <typename T>
const T& VoxelGrid<T>::At(int x, int y, int z) const {
    return grid[Index(x, y, z)];
}

template <typename T>
T* VoxelGrid<T>::Data() {
    return grid.data();
}

template <typename T>
const T* VoxelGrid<T>::Data() const {
    return grid.data();
}

template <typename T>
int VoxelGrid<T>::Index(int x, int y, int z) const {
    return x + width * (y + height * z);
}
