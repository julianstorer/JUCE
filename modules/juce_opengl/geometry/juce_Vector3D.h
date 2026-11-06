/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
/**
    A three-coordinate vector.

    @tags{OpenGL}
*/
template <typename Type>
class Vector3D
{
public:
    Vector3D() noexcept                                        : x(), y(), z() {}
    Vector3D (Type xValue, Type yValue, Type zValue) noexcept  : x (xValue), y (yValue), z (zValue) {}
    Vector3D (const Vector3D& other) noexcept                  : x (other.x), y (other.y), z (other.z) {}
    Vector3D& operator= (Vector3D other) noexcept              { x = other.x;  y = other.y;  z = other.z; return *this; }

    /** Returns a vector that lies along the X axis. */
    static Vector3D xAxis() noexcept                        { return { (Type) 1, 0, 0 }; }
    /** Returns a vector that lies along the Y axis. */
    static Vector3D yAxis() noexcept                        { return { 0, (Type) 1, 0 }; }
    /** Returns a vector that lies along the Z axis. */
    static Vector3D zAxis() noexcept                        { return { 0, 0, (Type) 1 }; }

    Vector3D& operator+= (Type value) noexcept              { x += value;  y += value;  z += value;  return *this; }
    Vector3D& operator-= (Type value) noexcept              { x -= value;  y -= value;  z -= value;  return *this; }
    Vector3D& operator*= (Type scaleFactor) noexcept        { x *= scaleFactor;  y *= scaleFactor;  z *= scaleFactor;  return *this; }
    Vector3D& operator/= (Type scaleFactor) noexcept        { x /= scaleFactor;  y /= scaleFactor;  z /= scaleFactor;  return *this; }
    Vector3D& operator+= (Vector3D other) noexcept          { x += other.x;  y += other.y;  z += other.z;  return *this; }
    Vector3D& operator-= (Vector3D other) noexcept          { x -= other.x;  y -= other.y;  z -= other.z;  return *this; }
    Vector3D& operator/= (Vector3D other) noexcept          { x /= other.x;  y /= other.y;  z /= other.z;  return *this; }

    Vector3D operator+ (Type value) const noexcept          { return { x + value, y + value, z + value }; }
    Vector3D operator- (Type value) const noexcept          { return { x - value, y - value, z - value }; }
    Vector3D operator* (Type scaleFactor) const noexcept    { return { x * scaleFactor, y * scaleFactor, z * scaleFactor }; }
    Vector3D operator/ (Type scaleFactor) const noexcept    { return { x / scaleFactor, y / scaleFactor, z / scaleFactor }; }
    Vector3D operator+ (Vector3D other) const noexcept      { return { x + other.x, y + other.y, z + other.z }; }
    Vector3D operator- (Vector3D other) const noexcept      { return { x - other.x, y - other.y, z - other.z }; }
	Vector3D operator/ (Vector3D other) const noexcept      { return { x / other.x, y / other.y, z / other.z }; }
    Vector3D operator-() const noexcept                     { return { -x, -y, -z }; }


    Vector3D multiplied (Vector3D other) const noexcept     { return { x * other.x, y * other.y, z * other.z }; }
    void multiply(Vector3D other) const noexcept            { x *= other.x; y *= other.y; z *= other.z; }

    /** Returns the dot-product of these two vectors. */
    Type operator* (Vector3D other) const noexcept          { return x * other.x + y * other.y + z * other.z; }

    /** Returns the cross-product of these two vectors. */
    Vector3D operator^ (Vector3D other) const noexcept      { return { y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x }; }

    Type length() const noexcept                            { return std::sqrt (lengthSquared()); }
    Type lengthSquared() const noexcept                     { return x * x + y * y + z * z; }

    Vector3D normalised() const noexcept                    { return *this / length(); }

    /** Returns true if the vector is practically equal to the origin. */
    bool lengthIsBelowEpsilon() const noexcept
    {
        auto epsilon = std::numeric_limits<Type>::epsilon();
        return ! (x < -epsilon || x > epsilon || y < -epsilon || y > epsilon || z < -epsilon || z > epsilon);
    }

    Type x, y, z;
};

} // namespace juce
