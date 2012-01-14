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

#include "PriorityManual.h"
#include "PriorityOnionPeel.h"
#include "Mask.h"
#include "NamedVTKImage.h"
#include "Testing.h"

int main()
{
  FloatVectorImageType::Pointer image = FloatVectorImageType::New();
  Testing::GetBlankImage(image.GetPointer(), 4);

  Mask::Pointer mask = Mask::New();
  Testing::GetFullyValidMask(mask.GetPointer());

  UnsignedCharScalarImageType::Pointer manualPriorityImage = UnsignedCharScalarImageType::New();
  Testing::GetBlankImage(manualPriorityImage.GetPointer());

  unsigned int patchRadius = 5;
  PriorityOnionPeel priorityOnionPeel(mask, patchRadius);
  
  PriorityManual<UnsignedCharScalarImageType, PriorityOnionPeel> priority(manualPriorityImage, &priorityOnionPeel);

  itk::ImageRegion<2> filledRegion;
  priority.Update(filledRegion);
  priority.SetManualPriorityImage(manualPriorityImage);

  itk::Index<2> queryPixel;
  queryPixel.Fill(0);
  priority.ComputePriority(queryPixel);

  return EXIT_SUCCESS;
}
