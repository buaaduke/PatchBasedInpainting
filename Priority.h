/*=========================================================================
 *
 *  Copyright David Doria 2011 daviddoria@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#ifndef PRIORITY_H
#define PRIORITY_H

#include "DebugOutputs.h"
#include "Types.h"

class Priority : public DebugOutputs
{
public:
  // Compute the priority of a specific pixel.
  virtual float ComputePriority(const itk::Index<2>& queryPixel) = 0;
  
  // Compute the priorities at all boundary pixels.
  void ComputeAllPriorities(const UnsignedCharScalarImageType::Pointer boundaryImage);
  
  // Get the current priority image
  FloatScalarImageType::Pointer GetPriorityImage();
  
protected:
  // Keep track of the priority of each pixel.
  FloatScalarImageType::Pointer PriorityImage;
};

#endif