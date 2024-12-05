#include "Vector3.hpp"

// Default constructor
Vector3::Vector3() : x(0), y(0), z(0) {}

// Parameterized constructor
Vector3::Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

// Magnitude of the vector
float Vector3::magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

// Normalized vector
Vector3 Vector3::normalized() const {
    float mag = magnitude();
    return (mag > 0) ? (*this / mag) : Vector3(0, 0, 0);
}

// Dot product
float Vector3::dot(const Vector3& other) const {
    return x * other.x + y * other.y + z * other.z;
}

// Cross product
Vector3 Vector3::cross(const Vector3& other) const {
    return Vector3(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

// Addition
Vector3 Vector3::operator+(const Vector3& other) const {
    return Vector3(x + other.x, y + other.y, z + other.z);
}

// Subtraction
Vector3 Vector3::operator-(const Vector3& other) const {
    return Vector3(x - other.x, y - other.y, z - other.z);
}

// Scalar multiplication
Vector3 Vector3::operator*(float scalar) const {
    return Vector3(x * scalar, y * scalar, z * scalar);
}

// Scalar division
Vector3 Vector3::operator/(float scalar) const {
    return (scalar != 0) ? Vector3(x / scalar, y / scalar, z / scalar) : Vector3(0, 0, 0);
}

// Compound addition
Vector3& Vector3::operator+=(const Vector3& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

// Compound subtraction
Vector3& Vector3::operator-=(const Vector3& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

// Compound scalar multiplication
Vector3& Vector3::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

// Compound scalar division
Vector3& Vector3::operator/=(float scalar) {
    if (scalar != 0) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
    }
    return *this;
}

// Equality check
bool Vector3::operator==(const Vector3& other) const {
    return x == other.x && y == other.y && z == other.z;
}

// Inequality check
bool Vector3::operator!=(const Vector3& other) const {
    return !(*this == other);
}

// Print the vector
void Vector3::print() const {
    std::cout << "(" << x << ", " << y << ", " << z << ")\n";
}
