#ifndef VECTOR3_HPP
#define VECTOR3_HPP

#include <cmath>
#include <iostream>

class Vector3 {
public:
    float x, y, z;

    // Constructors
    Vector3();
    Vector3(float x, float y, float z);

    // Magnitude and Normalization
    float magnitude() const;
    Vector3 normalized() const;

    // Dot and Cross Product
    float dot(const Vector3& other) const;
    Vector3 cross(const Vector3& other) const;

    // Operators
    Vector3 operator+(const Vector3& other) const;
    Vector3 operator-(const Vector3& other) const;
    Vector3 operator*(float scalar) const;
    Vector3 operator/(float scalar) const;

    // Compound Assignment
    Vector3& operator+=(const Vector3& other);
    Vector3& operator-=(const Vector3& other);
    Vector3& operator*=(float scalar);
    Vector3& operator/=(float scalar);

    // Comparison
    bool operator==(const Vector3& other) const;
    bool operator!=(const Vector3& other) const;

    // Print
    void print() const;
};

#endif // VECTOR3_HPP
