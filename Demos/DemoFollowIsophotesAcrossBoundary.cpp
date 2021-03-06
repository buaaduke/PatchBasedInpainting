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

// Submodules
#include <Helpers/Helpers.h>
#include <ITKHelpers/ITKHelpers.h>
#include <Mask/Mask.h>
#include <Mask/MaskOperations.h>

// ITK
#include "itkImageFileReader.h"
#include "itkRGBToLuminanceImageFilter.h"

int main(int argc, char *argv[])
{
  if(argc != 3)
  {
    std::cerr << "Required arguments: image mask" << std::endl;
    return EXIT_FAILURE;
  }
  std::string imageFilename = argv[1];
  std::string maskFilename = argv[2];
  std::cout << "Reading image: " << imageFilename << std::endl;
  std::cout << "Reading mask: " << maskFilename << std::endl;

  typedef itk::VectorImage<float, 2> FloatVectorImageType;
  typedef itk::ImageFileReader<FloatVectorImageType> ImageReaderType;
  ImageReaderType::Pointer imageReader = ImageReaderType::New();
  imageReader->SetFileName(imageFilename.c_str());
  imageReader->Update();

  std::cout << "Read image " << imageReader->GetOutput()->GetLargestPossibleRegion() << std::endl;

  typedef itk::ImageFileReader<Mask> MaskReaderType;
  MaskReaderType::Pointer maskReader = MaskReaderType::New();
  maskReader->SetFileName(maskFilename.c_str());
  maskReader->Update();

  std::cout << "Read mask " << maskReader->GetOutput()->GetLargestPossibleRegion() << std::endl;

  // Prepare image
  typedef itk::Image<itk::RGBPixel<unsigned char>, 2> RGBImageType;
  RGBImageType::Pointer rgbImage = RGBImageType::New();
  // TODO: update this to the new API
  //Helpers::VectorImageToRGBImage(imageReader->GetOutput(), rgbImage);

  ITKHelpers::WriteImage(rgbImage.GetPointer(), "Test/TestIsophotes.rgb.mha");

  typedef itk::Image<float, 2> FloatScalarImageType;
  typedef itk::RGBToLuminanceImageFilter< RGBImageType, FloatScalarImageType > LuminanceFilterType;
  LuminanceFilterType::Pointer luminanceFilter = LuminanceFilterType::New();
  luminanceFilter->SetInput(rgbImage);
  luminanceFilter->Update();

  FloatScalarImageType::Pointer blurredLuminance = FloatScalarImageType::New();
  // Blur with a Gaussian kernel
  unsigned int kernelRadius = 5;
  MaskOperations::MaskedBlur<FloatScalarImageType>(luminanceFilter->GetOutput(), maskReader->GetOutput(), kernelRadius,
                                            blurredLuminance);

  ITKHelpers::WriteImage(blurredLuminance.GetPointer(), "Test/TestIsophotes.blurred.mha");

  //inpainting.ComputeMaskedIsophotes(blurredLuminance, maskReader->GetOutput());

  //Helpers::WriteImage<FloatVector2ImageType>(inpainting.GetIsophoteImage(), );
  //HelpersOutput::Write2DVectorImage(inpainting.GetIsophoteImage(), "Test/TestIsophotes.isophotes.mha");

  itk::Size<2> size;
  size.Fill(21);

  // Target
  itk::Index<2> targetIndex;
  targetIndex[0] = 187;
  targetIndex[1] = 118;
  itk::ImageRegion<2> targetRegion(targetIndex, size);

  // Source
  itk::Index<2> sourceIndex;
  sourceIndex[0] = 176;
  sourceIndex[1] = 118;
  itk::ImageRegion<2> sourceRegion(sourceIndex, size);

  //PatchPair patchPair(Patch(sourceRegion), Patch(targetRegion));
  //PatchPair patchPair;
//   Patch sourcePatch(sourceRegion);
//   Patch targetPatch(targetRegion);
//   PatchPair patchPair(sourcePatch, targetPatch);

  //inpainting.FindBoundary();

//   std::vector<itk::Index<2> > borderPixels =
//     ITKHelpers::GetNonZeroPixels(inpainting.GetBoundaryImage(), targetRegion);

  itk::RGBPixel<unsigned char> black;
  black.SetRed(0);
  black.SetGreen(0);
  black.SetBlue(0);

  itk::RGBPixel<unsigned char> red;
  red.SetRed(255);
  red.SetGreen(0);
  red.SetBlue(0);

  itk::RGBPixel<unsigned char> darkRed;
  darkRed.SetRed(100);
  darkRed.SetGreen(0);
  darkRed.SetBlue(0);

  itk::RGBPixel<unsigned char> yellow;
  yellow.SetRed(255);
  yellow.SetGreen(255);
  yellow.SetBlue(0);

  itk::RGBPixel<unsigned char> green;
  green.SetRed(0);
  green.SetGreen(255);
  green.SetBlue(0);

  itk::RGBPixel<unsigned char> darkGreen;
  darkGreen.SetRed(0);
  darkGreen.SetGreen(100);
  darkGreen.SetBlue(0);

  itk::RGBPixel<unsigned char> blue;
  blue.SetRed(0);
  blue.SetGreen(0);
  blue.SetBlue(255);

  RGBImageType::Pointer output = RGBImageType::New();
  output->SetRegions(imageReader->GetOutput()->GetLargestPossibleRegion());
  output->Allocate();
  output->FillBuffer(black);

  ITKHelpers::BlankAndOutlineRegion(output.GetPointer(), targetRegion, black, red);
  ITKHelpers::BlankAndOutlineRegion(output.GetPointer(), sourceRegion, black, green);

  RGBImageType::Pointer target = RGBImageType::New();
  target->SetRegions(imageReader->GetOutput()->GetLargestPossibleRegion());
  target->Allocate();
  ITKHelpers::BlankAndOutlineRegion(target.GetPointer(), targetRegion, black, red);

  RGBImageType::Pointer source = RGBImageType::New();
  source->SetRegions(imageReader->GetOutput()->GetLargestPossibleRegion());
  source->Allocate();
  ITKHelpers::BlankAndOutlineRegion(source.GetPointer(), sourceRegion, black, green);

  // itk::Offset<2> offset = targetIndex - sourceIndex;
  /*
  for(unsigned int pixelId = 0; pixelId < borderPixels.size(); ++pixelId)
    {
    itk::Index<2> targetPatchSourceSideBoundaryPixel = borderPixels[pixelId];
    itk::Index<2> sourcePatchTargetSideBoundaryPixel;
    //bool valid = GetAdjacentBoundaryPixel(currentPixel, candidatePairs[sourcePatchId], adjacentBoundaryPixel);
    bool valid = inpainting.GetAdjacentBoundaryPixel(targetPatchSourceSideBoundaryPixel, patchPair, sourcePatchTargetSideBoundaryPixel);

    target->SetPixel(targetPatchSourceSideBoundaryPixel, darkRed);
    source->SetPixel(sourcePatchTargetSideBoundaryPixel, darkGreen);

    if(!valid)
      {
      continue;
      }

    // Bring the adjacent pixel back to the target region.
    itk::Index<2> targetPatchTargetSideBoundaryPixel = sourcePatchTargetSideBoundaryPixel + offset;

    output->SetPixel(targetPatchSourceSideBoundaryPixel, darkRed);

    output->SetPixel(targetPatchTargetSideBoundaryPixel, blue);
    output->SetPixel(sourcePatchTargetSideBoundaryPixel, darkGreen);
    }
  */

//   OutputHelpers::WriteImage(output.GetPointer(), "Test/FollowIsophotes.Output.mha");
//   OutputHelpers::WriteImage(target.GetPointer(), "Test/FollowIsophotes.Target.mha");
//   OutputHelpers::WriteImage(source.GetPointer(), "Test/FollowIsophotes.Source.mha");

  return EXIT_SUCCESS;
}
