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

#include "CandidatePairs.h"
#include "Mask.h"
#include "SourcePatchCollection.h"
#include "Testing.h"
#include "PixelWiseDifferencePatchPairVisitor.h"

int main(int argc, char*argv[])
{
  // Setup target patch
  itk::Index<2> targetCorner;
  targetCorner.Fill(0);

  itk::Size<2> patchSize;
  patchSize.Fill(10);

  itk::ImageRegion<2> targetRegion(targetCorner, patchSize);
  Patch targetPatch(targetRegion);

  Mask::Pointer mask = Mask::New();
  Testing::GetFullyValidMask(mask);
  const unsigned int patchRadius = 5;
  SourcePatchCollection sourcePatchCollection(mask, patchRadius);

  SourcePatchCollection::PatchContainer patches = sourcePatchCollection.FindSourcePatchesInRegion(mask->GetLargestPossibleRegion());

  sourcePatchCollection.AddPatches(patches);

  CandidatePairs candidatePairs(targetPatch);

  candidatePairs.AddSourcePatches(sourcePatchCollection);

  FloatScalarImageType::Pointer image = FloatScalarImageType::New();
  Testing::GetBlankImage(image.GetPointer());

  PixelWiseDifferencePatchPairVisitor<FloatScalarImageType::PixelType> visitor(image.GetPointer(), mask.GetPointer());
  candidatePairs.VisitAllPatchPairs(image.GetPointer(), mask.GetPointer(), visitor);
  return EXIT_SUCCESS;
}
