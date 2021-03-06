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

#include "GMHDifference.hpp"
#include "GMHDifferenceFast.hpp"

//#include "Testing.h"

// Submodules
#include "ITKHelpers/ITKHelpers.h"

// ITK
#include "itkImage.h"
#include "itkRandomImageSource.h"

static void Scalar();
static void Vector();

int main(int, char*[])
{
  Scalar();
  Vector();

  return EXIT_SUCCESS;
}

void Scalar()
{
  typedef itk::Image< unsigned char, 2 >  ImageType;

  itk::Size<2> imageSize = {{500,500}};

  itk::RandomImageSource<ImageType>::Pointer randomImageSource =
    itk::RandomImageSource<ImageType>::New();
  randomImageSource->SetNumberOfThreads(1); // to produce non-random results
  randomImageSource->SetSize(imageSize);
  randomImageSource->Update();

  itk::Size<2> patchSize = {{21,21}};

  // There is nothing magic about these particular patches
  itk::Index<2> targetCorner = {{319, 302}};
  itk::ImageRegion<2> targetRegion(targetCorner, patchSize);

  itk::Index<2> sourceCorner = {{341, 300}};
  itk::ImageRegion<2> sourceRegion(sourceCorner, patchSize);

  Mask::Pointer mask = Mask::New();
  mask->SetRegions(randomImageSource->GetOutput()->GetLargestPossibleRegion());
  mask->Allocate();
  ITKHelpers::SetImageToConstant(mask.GetPointer(), mask->GetValidValue());

  const unsigned int numberOfBinsPerChannel = 30;
  GMHDifference<ImageType> gmhDifference(randomImageSource->GetOutput(), mask, numberOfBinsPerChannel);
//  GMHDifferenceFast<ImageType> gmhDifference(imageReader->GetOutput(), mask, numberOfBinsPerChannel); // This does not produce the same value as GMHDifference!

  float difference = gmhDifference.Difference(targetRegion, sourceRegion);

  std::cout << "GMHDifference: " << difference << std::endl;
}

void Vector()
{
  typedef itk::Image<unsigned char, 2 >  ChannelType;
  const unsigned int NumberOfChannels = 3;
  typedef itk::Image<itk::CovariantVector<unsigned char, NumberOfChannels>, 2 >  ImageType;

  ImageType::Pointer image = ImageType::New();
  itk::Index<2> corner = {{0,0}};
  itk::Size<2> imageSize = {{500,500}};
  itk::ImageRegion<2> fullRegion(corner, imageSize);
  image->SetRegions(fullRegion);
  image->Allocate();

  for(unsigned int i = 0; i < NumberOfChannels; ++i)
  {
    itk::RandomImageSource<ChannelType>::Pointer randomImageSource =
      itk::RandomImageSource<ChannelType>::New();
    randomImageSource->SetNumberOfThreads(1); // to produce non-random results
    randomImageSource->SetSize(imageSize);
    randomImageSource->Update();

    ITKHelpers::SetChannel(image.GetPointer(), i, randomImageSource->GetOutput());
  }

  itk::Size<2> patchSize = {{21,21}};

  // There is nothing magic about these particular patches
  itk::Index<2> targetCorner = {{319, 302}};
  itk::ImageRegion<2> targetRegion(targetCorner, patchSize);

  itk::Index<2> sourceCorner = {{341, 300}};
  itk::ImageRegion<2> sourceRegion(sourceCorner, patchSize);

  Mask::Pointer mask = Mask::New();
  mask->SetRegions(fullRegion);
  mask->Allocate();
  ITKHelpers::SetImageToConstant(mask.GetPointer(), mask->GetValidValue());

  const unsigned int numberOfBinsPerChannel = 30;
  GMHDifference<ImageType> gmhDifference(image.GetPointer(), mask, numberOfBinsPerChannel);
//  GMHDifferenceFast<ImageType> gmhDifference(imageReader->GetOutput(), mask, numberOfBinsPerChannel); // This produces 5.87 but it should produce 0.932

  float difference = gmhDifference.Difference(targetRegion, sourceRegion);

  std::cout << "GMHDifference: " << difference << std::endl;

}
