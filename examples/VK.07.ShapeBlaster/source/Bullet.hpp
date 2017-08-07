
/*
================================================================================

  MIT License

  Copyright (c) 2016 Dynamic_Static

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

================================================================================
*/

// Based on "Make a Neon Vector Shooter in XNA"
// https://gamedevelopment.tutsplus.com/series/cross-platform-vector-shooter-xna--gamedev-10559

#pragma once

#include "Entity.hpp"

#include "Dynamic_Static/Core/Math.hpp"

namespace ShapeBlaster {

    class Bullet final
        : public Entity
    {
    public:
        Bullet(const dst::Vector2& position, const dst::Vector2& velocity)
        {
            mPosition = position;
            mVelocity = velocity;
            mOrientation = to_angle(mVelocity);
            mRadius = 8;
        }

    public:
        void update() override final
        {
            if (mVelocity.x || mVelocity.y) {
                mOrientation = to_angle(mVelocity);
            }

            mPosition += mVelocity;

            // TODO : If out of bounds...
            if (false) {
                mExpired = true;
            }
        }
    };

} // namespace ShapeBlaster
