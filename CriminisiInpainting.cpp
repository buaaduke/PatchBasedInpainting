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

#include "CriminisiInpainting.h"

// Custom
#include "Helpers.h"
#include "RotateVectors.h"

#include "SelfPatchCompare.h"
#include "SelfPatchCompareColor.h"
#include "SelfPatchCompareAll.h"

// STL
#include <iostream>

// VXL
#include <vnl/vnl_double_2.h>

// ITK
#include "itkBinaryContourImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkFlatStructuringElement.h"
#include "itkGradientImageFilter.h"
#include "itkInvertIntensityImageFilter.h"
#include "itkMaskImageFilter.h"
#include "itkRGBToLuminanceImageFilter.h"


CriminisiInpainting::CriminisiInpainting()
{
  this->PatchRadius.Fill(3);

  this->BoundaryImage = UnsignedCharScalarImageType::New();
  this->BoundaryNormals = FloatVector2ImageType::New();
  this->IsophoteImage = FloatVector2ImageType::New();
  this->PriorityImage = FloatScalarImageType::New();
  this->OriginalMask = Mask::New();
  this->CurrentMask = Mask::New();
  this->OriginalImage = FloatVectorImageType::New();
  this->CurrentOutputImage = FloatVectorImageType::New();
  this->CIELabImage = FloatVectorImageType::New();
  this->BlurredImage = FloatVectorImageType::New();
  
  // Set the image to use for pixel to pixel comparisons.
  //this->CompareImage = this->CIELabImage;
  this->CompareImage = this->BlurredImage;
  
  this->ConfidenceImage = FloatScalarImageType::New();
  this->ConfidenceMapImage = FloatScalarImageType::New();
  this->DataImage = FloatScalarImageType::New();
  
  this->DebugImages = false;
  this->DebugMessages = false;
  this->NumberOfCompletedIterations = 0;
  
  this->HistogramBinsPerDimension = 10;
  this->MaxForwardLookPatches = 10;
  //this->MaxPotentialPatches = 2;
  
  this->MaxPixelDifference = 0.0f;
}

void CriminisiInpainting::ComputeMaxPixelDifference()
{
  // We assume all values of all channels are positive, so the max difference can be computed as max(p_i - 0) because (0,0,0,...) is the minimum pixel.
  
  itk::ImageRegionConstIterator<FloatVectorImageType> imageIterator(this->OriginalImage, this->OriginalImage->GetLargestPossibleRegion());

  FloatVectorImageType::PixelType zeroPixel;
  zeroPixel.SetSize(this->OriginalImage->GetNumberOfComponentsPerPixel());
  zeroPixel.Fill(0);
  
  FloatVectorImageType::PixelType maxPixel = zeroPixel;
  while(!imageIterator.IsAtEnd())
    {
    FloatVectorImageType::PixelType pixel = this->OriginalImage->GetPixel(imageIterator.GetIndex());
    if(pixel.GetNorm() > maxPixel.GetNorm())
      {
      maxPixel = pixel;
      }
    ++imageIterator;
    }

  /*
  this->MaxPixelDifference = 0.0f;
  for(unsigned int i = 0; i < this->OriginalImage->GetNumberOfComponentsPerPixel(); ++i)
    {
    this->MaxPixelDifference += maxPixel[i];
    }
  */
  this->MaxPixelDifference = SelfPatchCompareAll::StaticPixelDifference(maxPixel, zeroPixel);
  
  std::cout << "MaxPixelDifference computed to be: " << this->MaxPixelDifference << std::endl;
}

void CriminisiInpainting::ComputeSourcePatches(const itk::ImageRegion<2>& region)
{
  // Find all full patches that are entirely Valid
  DebugMessage("ComputeSourcePatches()");
  try
  {
    this->SourcePatches.clear();
    itk::ImageRegionConstIterator<FloatVectorImageType> imageIterator(this->CurrentOutputImage, region);

    while(!imageIterator.IsAtEnd())
      {
      itk::Index<2> currentPixel = imageIterator.GetIndex();
      itk::ImageRegion<2> region = Helpers::GetRegionInRadiusAroundPixel(currentPixel, this->PatchRadius[0]);
    
      if(this->CurrentMask->GetLargestPossibleRegion().IsInside(region))
	{
	if(this->CurrentMask->IsValid(region))
	  {
	  //this->SourcePatches.push_back(Patch(this->OriginalImage, region));
	  this->SourcePatches.push_back(Patch(region));
	  //DebugMessage("Added a source patch.");
	  }
	}
    
      ++imageIterator;
      }
    DebugMessage<unsigned int>("Number of source patches: ", this->SourcePatches.size());
    
    if(this->SourcePatches.size() == 0)
      {
      std::cerr << "There must be at least 1 source patch!" << std::endl;
      exit(-1);
      }
  }// end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeSourcePatches!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }

}

void CriminisiInpainting::InitializeConfidenceMap()
{
  DebugMessage("InitializeConfidenceMap()");
  // Clone the mask - we need to invert the mask to actually perform the masking, but we don't want to disturb the original mask
  Mask::Pointer maskClone = Mask::New();
  //Helpers::DeepCopy<Mask>(this->CurrentMask, maskClone);
  maskClone->DeepCopyFrom(this->CurrentMask);
  
  // Invert the mask
  typedef itk::InvertIntensityImageFilter <Mask> InvertIntensityImageFilterType;

  InvertIntensityImageFilterType::Pointer invertIntensityFilter = InvertIntensityImageFilterType::New();
  invertIntensityFilter->SetInput(maskClone);
  //invertIntensityFilter->InPlaceOn();
  invertIntensityFilter->Update();

  // Convert the inverted mask to floats and scale them to between 0 and 1
  // to serve as the initial confidence image
  typedef itk::RescaleIntensityImageFilter< Mask, FloatScalarImageType > RescaleFilterType;
  RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
  rescaleFilter->SetInput(invertIntensityFilter->GetOutput());
  rescaleFilter->SetOutputMinimum(0);
  rescaleFilter->SetOutputMaximum(1);
  rescaleFilter->Update();

  Helpers::DeepCopy<FloatScalarImageType>(rescaleFilter->GetOutput(), this->ConfidenceMapImage);

}

void CriminisiInpainting::InitializeTargetImage()
{
  DebugMessage("InitializeTargetImage()");
  // Initialize to the input
  Helpers::DeepCopyVectorImage<FloatVectorImageType>(this->OriginalImage, this->CurrentOutputImage);
  
  // We set hole pixels to green so we can visually ensure these pixels are not being copied during the inpainting
  FloatVectorImageType::PixelType green;
  green.SetSize(this->OriginalImage->GetNumberOfComponentsPerPixel());
  green.Fill(0);
  green[1] = 255;
  
  itk::ImageRegionConstIterator<Mask> maskIterator(this->CurrentMask, this->CurrentMask->GetLargestPossibleRegion());

  while(!maskIterator.IsAtEnd())
    {
    if(this->CurrentMask->IsHole(maskIterator.GetIndex()))
      {
      this->CurrentOutputImage->SetPixel(maskIterator.GetIndex(), green);
      }

    ++maskIterator;
    }
}

void CriminisiInpainting::Initialize()
{
  DebugMessage("Initialize()");
  try
  {
    this->NumberOfCompletedIterations = 0;
  
    // We find the boundary of the mask at every iteration (in Iterate()), but we do this here so that everything is initialized and valid if we are observing this class for display.
    FindBoundary();
    
    InitializeTargetImage();
    Helpers::DebugWriteImageConditional<FloatVectorImageType>(this->CurrentOutputImage, "Debug/Initialize.CurrentOutputImage.mha", this->DebugImages);
  
    ComputeIsophotes();
    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(this->IsophoteImage, "Debug/Initialize.IsophoteImage.mha", this->DebugImages);

    // Blur the image incase we want to use a blurred image for pixel to pixel comparisons.
    unsigned int kernelRadius = 5;
    Helpers::VectorMaskedBlur(this->OriginalImage, this->CurrentMask, kernelRadius, this->BlurredImage);
    Helpers::DebugWriteImageConditional<FloatVectorImageType>(this->BlurredImage, "Debug/Initialize.BlurredImage.mha", this->DebugImages);
    
    InitializeImage<FloatScalarImageType>(this->DataImage);
    
    InitializeImage<FloatScalarImageType>(this->PriorityImage);
    
    InitializeImage<FloatScalarImageType>(this->ConfidenceImage);
        
    InitializeConfidenceMap();
    Helpers::DebugWriteImageConditional<FloatScalarImageType>(this->ConfidenceMapImage, "Debug/Initialize.ConfidenceMapImage.mha", this->DebugImages);

    DebugMessage("Computing source patches...");
    ComputeSourcePatches(this->FullImageRegion);
    // Debugging outputs
    //WriteImage<TImage>(this->Image, "InitialImage.mhd");
    //WriteImage<UnsignedCharImageType>(this->Mask, "InitialMask.mhd");
    //WriteImage<FloatImageType>(this->ConfidenceImage, "InitialConfidence.mhd");
    //WriteImage<VectorImageType>(this->IsophoteImage, "InitialIsophotes.mhd");
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in Initialize()!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::Iterate()
{
  DebugMessage("Iterate()");
  
  FindBoundary();
  Helpers::DebugWriteImageConditional<UnsignedCharScalarImageType>(this->BoundaryImage, "Debug/BoundaryImage.mha", this->DebugImages);

  DebugMessage("Found boundary.");

  ComputeBoundaryNormals();
  Helpers::DebugWriteImageConditional<FloatVector2ImageType>(this->BoundaryNormals, "Debug/BoundaryNormals.mha", this->DebugImages);

  DebugMessage("Computed boundary normals.");

  ComputeAllDataTerms();
  ComputeAllConfidenceTerms();
  ComputeAllPriorities();
  DebugMessage("Computed priorities.");

  PatchPair patchPair;
    
  //FindBestPatchForHighestPriority(sourcePatch, targetPatch);
  
  FindBestPatchLookAhead(patchPair);

  std::stringstream ssSource;
  ssSource << "Debug/source_" << Helpers::ZeroPad(this->NumberOfCompletedIterations, 3) << ".mha";
  Helpers::WritePatch<FloatVectorImageType>(this->CurrentOutputImage, patchPair.SourcePatch, ssSource.str());

  std::stringstream ssTarget;
  ssTarget << "Debug/target_" << Helpers::ZeroPad(this->NumberOfCompletedIterations, 3) << ".mha";
  Helpers::WritePatch<FloatVectorImageType>(this->CurrentOutputImage, patchPair.TargetPatch, ssTarget.str());
  
  this->UsedPatchPairs.push_back(patchPair);
  
  // Copy the patch. This is the actual inpainting step.
  Helpers::CopySelfPatchIntoValidRegion<FloatVectorImageType>(this->CurrentOutputImage, this->CurrentMask, patchPair.SourcePatch.Region, patchPair.TargetPatch.Region);
  
  // We also have to copy patches in the blurred image and CIELab image incase we are using those
  Helpers::CopySelfPatchIntoValidRegion<FloatVectorImageType>(this->BlurredImage, this->CurrentMask, patchPair.SourcePatch.Region, patchPair.TargetPatch.Region);
  Helpers::CopySelfPatchIntoValidRegion<FloatVectorImageType>(this->CIELabImage, this->CurrentMask, patchPair.SourcePatch.Region, patchPair.TargetPatch.Region);
  
  float confidence = this->ConfidenceImage->GetPixel(Helpers::GetRegionCenter(patchPair.TargetPatch.Region));
  // Copy the new confidences into the confidence image
  UpdateConfidences(patchPair.TargetPatch.Region, confidence);

  // The isophotes can be copied because they would (should!) only change slightly if recomputed. !!! TODO: Maybe they should actually be recomputed?
  Helpers::CopySelfPatchIntoValidRegion<FloatVector2ImageType>(this->IsophoteImage, this->CurrentMask, patchPair.SourcePatch.Region, patchPair.TargetPatch.Region);

  // Update the mask
  this->UpdateMask(patchPair.TargetPatch.Region);
  DebugMessage("Updated mask.");

  // Sanity check everything
  DebugWriteAllImages();

  // Add new source patches
  ComputeSourcePatches(patchPair.TargetPatch.Region);
  
  // At the end of an iteration, things would be slightly out of sync. The boundary is computed at the beginning of the iteration (before the patch is filled),
  // so at the end of the iteration, the boundary image will not correspond to the boundary of the remaining hole in the image - it will be off by 1 iteration.
  // We fix this by computing the boundary and boundary normals again at the end of the iteration.
  FindBoundary();
  ComputeBoundaryNormals();
  
  this->NumberOfCompletedIterations++;

  DebugMessage<unsigned int>("Completed iteration: ", this->NumberOfCompletedIterations);

  
}

void CriminisiInpainting::FindBestPatchForHighestPriority(PatchPair& bestPatchPair)
{
  // This function implements Criminisi's idea of "find the highest priority pixel and proceed to fill it".
  // We have replaced this idea with FindBestPatchLookAhead().
  
  // This function returns the best PatchPair by reference.
  
  float highestPriority = 0;
  itk::Index<2> pixelToFill = FindHighestValueOnBoundary(this->PriorityImage, highestPriority);
  DebugMessage<itk::Index<2> >("Highest priority found to be ", pixelToFill);

  itk::ImageRegion<2> targetRegion = Helpers::GetRegionInRadiusAroundPixel(pixelToFill, this->PatchRadius[0]);
  Patch targetPatch;
  targetPatch.Region = targetRegion;
  
  DebugMessage("Finding best patch...");

  SelfPatchCompare* patchCompare;
  patchCompare = new SelfPatchCompareColor(this->CompareImage->GetNumberOfComponentsPerPixel());
  patchCompare->SetImage(this->CompareImage);
  patchCompare->SetMask(this->CurrentMask);
  patchCompare->SetSourcePatches(this->SourcePatches);
  patchCompare->SetTargetPatch(targetPatch);
  
  float distance = 0;
  unsigned int bestMatchSourcePatchId = patchCompare->FindBestPatch(distance);
  //DebugMessage<unsigned int>("Found best patch to be ", bestMatchSourcePatchId);
  //std::cout << "Found best patch to be " << bestMatchSourcePatchId << std::endl;

  //this->DebugWritePatch(this->SourcePatches[bestMatchSourcePatchId], "SourcePatch.png");
  //this->DebugWritePatch(targetRegion, "TargetPatch.png");
  Patch sourcePatch;
  sourcePatch = this->SourcePatches[bestMatchSourcePatchId];
  
  bestPatchPair.TargetPatch = targetPatch;
  bestPatchPair.SourcePatch = sourcePatch;
}


void CriminisiInpainting::FindBestPatchLookAhead(PatchPair& bestPatchPair)
{
  // This function returns the best PatchPair by reference
  
  FloatScalarImageType::Pointer CurrentPriorityImage = FloatScalarImageType::New();
  Helpers::DeepCopy<FloatScalarImageType>(this->PriorityImage, CurrentPriorityImage);

  std::vector<CandidatePatches> allCandidatePatches;
  
  for(unsigned int i = 0; i < this->MaxForwardLookPatches; ++i)
    {
    float highestPriority = 0;
    itk::Index<2> pixelToFill = FindHighestValueOnBoundary(CurrentPriorityImage, highestPriority);

    if(!Helpers::HasHoleNeighbor(pixelToFill, this->CurrentMask))
      {
      std::cerr << "pixelToFill " << pixelToFill << " does not have a hole neighbor - something is wrong!" << std::endl;
      std::cerr << "Mask value " << static_cast<unsigned int>(this->CurrentMask->GetPixel(pixelToFill)) << std::endl;
      std::cerr << "Boundary value " << static_cast<unsigned int>(this->BoundaryImage->GetPixel(pixelToFill)) << std::endl;
      exit(-1);
      }

    // We want to do at least one comparison, but if we are near the end of the completion, there may not be more than 1 priority max
    if(highestPriority < .0001 && i > 0)
      {
      std::cout << "Highest priority was only " << highestPriority << std::endl;
      break;
      }
    //DebugMessage<itk::Index<2> >("Highest priority found to be ", pixelToFill);

    itk::ImageRegion<2> targetRegion = Helpers::GetRegionInRadiusAroundPixel(pixelToFill, this->PatchRadius[0]);
    Patch targetPatch(targetRegion);

    CandidatePatches candidatePatches;
    candidatePatches.TargetPatch = targetPatch;
    
    /*
    SelfPatchCompare* patchCompare;
    patchCompare = new SelfPatchCompareColor(this->CompareImage->GetNumberOfComponentsPerPixel());
    patchCompare->SetImage(this->CompareImage);
    patchCompare->SetMask(this->CurrentMask);
    patchCompare->SetSourcePatches(this->SourcePatches);
    patchCompare->SetTargetPatch(targetPatch);
    
    float averageSSD = 0;
    unsigned int bestMatchSourcePatchId = patchCompare->FindBestPatch(averageSSD);
    patchPair.AverageSSD = averageSSD;
    
    //DebugMessage<unsigned int>("Found best patch to be ", bestMatchSourcePatchId);
    //std::cout << "Found best patch to be " << bestMatchSourcePatchId << std::endl;

    //this->DebugWritePatch(this->SourcePatches[bestMatchSourcePatchId], "SourcePatch.png");
    //this->DebugWritePatch(targetRegion, "TargetPatch.png");
    Patch sourcePatch = this->SourcePatches[bestMatchSourcePatchId];
    */
    
    std::vector<Patch> patchesSortedByContinuation = SortPatchesByContinuationDifference(targetPatch);

    // Keep only the number of top patches specified.
    patchesSortedByContinuation.erase(patchesSortedByContinuation.begin() + this->NumberOfTopPatchesToSave, patchesSortedByContinuation.end());
    
    // Blank a region around the current potential patch to fill. This will ensure the next potential patch to fill is reasonably far away.
    Helpers::SetRegionToConstant<FloatScalarImageType>(CurrentPriorityImage, targetRegion, 0.0f);
    
    //std::cout << "Potential pair: Source: " << sourcePatch.Region << " Target: " << targetPatch.Region << std::endl;

    /*
    // Compute histograms and histogram difference
    //HistogramType::Pointer sourceHistogram = Helpers::ComputeNDHistogramOfRegionManual(this->CurrentOutputImage, sourcePatch.Region, this->HistogramBinsPerDimension);
    HistogramType::Pointer sourceHistogram = Helpers::ComputeNDHistogramOfMaskedRegionManual(this->CurrentOutputImage, this->CurrentMask, sourcePatch.Region, this->HistogramBinsPerDimension);
    HistogramType::Pointer targetHistogram = Helpers::ComputeNDHistogramOfMaskedRegionManual(this->CurrentOutputImage, this->CurrentMask, targetPatch.Region, this->HistogramBinsPerDimension);
    
    float histogramDifference = Helpers::NDHistogramDifference(sourceHistogram, targetHistogram);
    patchPair.HistogramDifference = histogramDifference;
    */
    
    // Compute continuation difference

    Patch bestSourcePatch = patchesSortedByContinuation[0];
    PatchPair patchPair;
    patchPair.SourcePatch = bestSourcePatch;
    patchPair.TargetPatch = targetPatch;
    float continuationDifference = ContinuationDifference(patchPair);
    patchPair.ContinuationDifference = continuationDifference;

    candidatePatches.CandidateSourcePatches = patchesSortedByContinuation;
    allCandidatePatches.push_back(candidatePatches);

    } // end forward look loop

    this->PotentialCandidatePatches.push_back(allCandidatePatches);
//   std::cout << "Scores: " << std::endl;
//   for(unsigned int i = 0; i < ssdScores.size(); ++i)
//     {
//     std::cout << i << ": " << ssdScores[i] << std::endl;
//     }
  
  /*
  std::sort(patchPairs.begin(), patchPairs.end(), SortByAverageSSD);
  
  float histogramDifferenceThreshold = .4;
  
  unsigned int bestAttempt = 0;
  
  for(unsigned int i = 0; i < patchPairs.size(); ++i)
    {
    if(patchPairs[i].HistogramDifference < histogramDifferenceThreshold)
      {
      bestAttempt = i;
      break;
      }
    }
    
  std::cout << "Best attempt was " << bestAttempt << std::endl;
  */

  // Return the result by reference. TODO: This should be a "choose the best" rather than simply returning the best source patch of the first look ahead target patch.
  bestPatchPair.TargetPatch = allCandidatePatches[0].TargetPatch;
  bestPatchPair.SourcePatch = allCandidatePatches[0].CandidateSourcePatches[0];

  
}

void CriminisiInpainting::Inpaint()
{
  // This function is intended to be used by the command line version. It will do the complete inpainting without updating any UI or the ability to stop before it is complete.
  //std::cout << "CriminisiInpainting::Inpaint()" << std::endl;
  try
  {
    // Start the procedure
    DebugMessage("Initializing...");
    Initialize();

    this->NumberOfCompletedIterations = 0;
    while(HasMoreToInpaint())
      {
      Iterate();
      }

  }// end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in Inpaint!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::ComputeIsophotes()
{
  
  try
  {
    Helpers::DebugWriteImageConditional<FloatVectorImageType>(this->CurrentOutputImage, "Debug/ComputeIsophotes.input.mha", this->DebugImages);
    
    /*
    // This only works when the image is RGB
    typedef itk::VectorMagnitudeImageFilter<FloatVectorImageType, UnsignedCharScalarImageType>  VectorMagnitudeFilterType;
    VectorMagnitudeFilterType::Pointer magnitudeFilter = VectorMagnitudeFilterType::New();
    magnitudeFilter->SetInput(this->OriginalImage); // We use the original image here because the image that has been painted green inside the hole has a strong gradient around the hole.
    magnitudeFilter->Update();
    */
    RGBImageType::Pointer rgbImage = RGBImageType::New();
    Helpers::VectorImageToRGBImage(this->OriginalImage, rgbImage);
    
    Helpers::DebugWriteImageConditional<RGBImageType>(rgbImage, "Debug/ComputeIsophotes.rgb.mha", this->DebugImages);

    typedef itk::RGBToLuminanceImageFilter< RGBImageType, FloatScalarImageType > LuminanceFilterType;
    LuminanceFilterType::Pointer luminanceFilter = LuminanceFilterType::New();
    luminanceFilter->SetInput(rgbImage);
    luminanceFilter->Update();
  
    Helpers::DebugWriteImageConditional<FloatScalarImageType>(luminanceFilter->GetOutput(), "Debug/ComputeIsophotes.luminance.mha", this->DebugImages);

    FloatScalarImageType::Pointer blurredLuminance = FloatScalarImageType::New();
    // Blur with a Gaussian kernel
    unsigned int kernelRadius = 5;
    Helpers::MaskedBlur<FloatScalarImageType>(luminanceFilter->GetOutput(), this->CurrentMask, kernelRadius, blurredLuminance);
    
    Helpers::DebugWriteImageConditional<FloatScalarImageType>(blurredLuminance, "Debug/ComputeIsophotes.blurred.mha", true);
    
    // Compute the gradient
    FloatScalarImageType::Pointer xDerivative = FloatScalarImageType::New();
    Helpers::MaskedDerivative<FloatScalarImageType>(blurredLuminance, this->CurrentMask, 0, xDerivative);
    
    FloatScalarImageType::Pointer yDerivative = FloatScalarImageType::New();
    Helpers::MaskedDerivative<FloatScalarImageType>(blurredLuminance, this->CurrentMask, 1, yDerivative);
    
    FloatVector2ImageType::Pointer gradient = FloatVector2ImageType::New();
    Helpers::GradientFromDerivatives<float>(xDerivative, yDerivative, gradient);
    
    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(gradient, "Debug/ComputeIsophotes.gradient.mha", this->DebugImages);
 
    // Rotate the gradient 90 degrees to obtain isophotes from gradient
    typedef itk::UnaryFunctorImageFilter<FloatVector2ImageType, FloatVector2ImageType,
    RotateVectors<FloatVector2ImageType::PixelType,
                  FloatVector2ImageType::PixelType> > FilterType;

    FilterType::Pointer rotateFilter = FilterType::New();
    rotateFilter->SetInput(gradient);
    rotateFilter->Update();

    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(rotateFilter->GetOutput(), "Debug/ComputeIsophotes.rotatedGradient.mha", this->DebugImages);
      
    Helpers::DeepCopy<FloatVector2ImageType>(rotateFilter->GetOutput(), this->IsophoteImage);
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeIsophotes!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

bool CriminisiInpainting::HasMoreToInpaint()
{
  try
  {
    if(this->DebugImages)
      {
      Helpers::WriteImage<Mask>(this->CurrentMask, "Debug/HasMoreToInpaint.input.png");
      }
      
    itk::ImageRegionIterator<Mask> maskIterator(this->CurrentMask, this->CurrentMask->GetLargestPossibleRegion());

    while(!maskIterator.IsAtEnd())
      {
      if(this->CurrentMask->IsHole(maskIterator.GetIndex()))
	{
	return true;
	}

      ++maskIterator;
      }

    // If no pixels were holes, then we don't have any more to inpaint.
    return false;
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in HasMoreToInpaint!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::FindBoundary()
{
  try
  {
    // Compute the "outer" boundary of the region to fill. That is, we want the boundary pixels to be in the source region.

    Helpers::DebugWriteImageConditional<Mask>(this->CurrentMask, "Debug/FindBoundary.CurrentMask.mha", this->DebugImages);
    Helpers::DebugWriteImageConditional<Mask>(this->CurrentMask, "Debug/FindBoundary.CurrentMask.png", this->DebugImages);

    // Create a binary image (throw away the "dont use" pixels)
    Mask::Pointer holeOnly = Mask::New();
    holeOnly->DeepCopyFrom(this->CurrentMask);
    
    itk::ImageRegionIterator<Mask> maskIterator(holeOnly, holeOnly->GetLargestPossibleRegion());
    // This should result in a white hole on a black background
    while(!maskIterator.IsAtEnd())
      {
      itk::Index<2> currentPixel = maskIterator.GetIndex();
      if(!holeOnly->IsHole(currentPixel))
	{
	holeOnly->SetPixel(currentPixel, holeOnly->GetValidValue());
	}
      ++maskIterator;
      }

    Helpers::DebugWriteImageConditional<Mask>(holeOnly, "Debug/FindBoundary.HoleOnly.mha", this->DebugImages);
    Helpers::DebugWriteImageConditional<Mask>(holeOnly, "Debug/FindBoundary.HoleOnly.png", this->DebugImages);
      
    // Since the hole is white, we want the foreground value of the contour filter to be black. This means that the boundary will
    // be detected in the black pixel region, which is on the outside edge of the hole like we want. However,
    // The BinaryContourImageFilter will change all non-boundary pixels to the background color, so the resulting output will be inverted -
    // the boundary pixels will be black and the non-boundary pixels will be white.
    
    // Find the boundary
    typedef itk::BinaryContourImageFilter <Mask, Mask> binaryContourImageFilterType;
    binaryContourImageFilterType::Pointer binaryContourFilter = binaryContourImageFilterType::New();
    binaryContourFilter->SetInput(holeOnly);
    binaryContourFilter->SetFullyConnected(true);
    binaryContourFilter->SetForegroundValue(holeOnly->GetValidValue());
    binaryContourFilter->SetBackgroundValue(holeOnly->GetHoleValue());
    binaryContourFilter->Update();

    Helpers::DebugWriteImageConditional<Mask>(binaryContourFilter->GetOutput(), "Debug/FindBoundary.Boundary.mha", this->DebugImages);
    Helpers::DebugWriteImageConditional<Mask>(binaryContourFilter->GetOutput(), "Debug/FindBoundary.Boundary.png", this->DebugImages);

    // Since we want to interpret non-zero pixels as boundary pixels, we must invert the image.
    typedef itk::InvertIntensityImageFilter <Mask> InvertIntensityImageFilterType;
    InvertIntensityImageFilterType::Pointer invertIntensityFilter = InvertIntensityImageFilterType::New();
    invertIntensityFilter->SetInput(binaryContourFilter->GetOutput());
    invertIntensityFilter->SetMaximum(255);
    invertIntensityFilter->Update();
    
    //this->BoundaryImage = binaryContourFilter->GetOutput();
    //this->BoundaryImage->Graft(binaryContourFilter->GetOutput());
    Helpers::DeepCopy<UnsignedCharScalarImageType>(invertIntensityFilter->GetOutput(), this->BoundaryImage);

    Helpers::DebugWriteImageConditional<UnsignedCharScalarImageType>(this->BoundaryImage, "Debug/FindBoundary.BoundaryImage.mha", this->DebugImages);
    
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in FindBoundary!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::UpdateMask(const itk::ImageRegion<2>& region)
{
  DebugMessage("UpdateMask()");
  try
  {
    itk::ImageRegionIterator<Mask> maskIterator(this->CurrentMask, region);
  
    while(!maskIterator.IsAtEnd())
      {
      if(this->CurrentMask->IsHole(maskIterator.GetIndex()))
	{
	maskIterator.Set(this->CurrentMask->GetValidValue());
	}
  
      ++maskIterator;
      }
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in UpdateMask!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::ComputeBoundaryNormals()
{
  DebugMessage("ComputeBoundaryNormals()");
  try
  {
    // Blur the mask, compute the gradient, then keep the normals only at the original mask boundary

    Helpers::DebugWriteImageConditional<UnsignedCharScalarImageType>(this->BoundaryImage, "Debug/ComputeBoundaryNormals.BoundaryImage.mha", this->DebugImages);
    Helpers::DebugWriteImageConditional<Mask>(this->CurrentMask, "Debug/ComputeBoundaryNormals.CurrentMask.mha", this->DebugImages);
      
    // Blur the mask
    typedef itk::DiscreteGaussianImageFilter< Mask, FloatScalarImageType >  BlurFilterType;
    BlurFilterType::Pointer gaussianFilter = BlurFilterType::New();
    gaussianFilter->SetInput(this->CurrentMask);
    gaussianFilter->SetVariance(2);
    gaussianFilter->Update();

    Helpers::DebugWriteImageConditional<FloatScalarImageType>(gaussianFilter->GetOutput(), "Debug/ComputeBoundaryNormals.BlurredMask.mha", this->DebugImages);

    // Compute the gradient of the blurred mask
    typedef itk::GradientImageFilter< FloatScalarImageType, float, float>  GradientFilterType;
    GradientFilterType::Pointer gradientFilter = GradientFilterType::New();
    gradientFilter->SetInput(gaussianFilter->GetOutput());
    gradientFilter->Update();

    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(gradientFilter->GetOutput(), "Debug/ComputeBoundaryNormals.BlurredMaskGradient.mha", this->DebugImages);

    // Only keep the normals at the boundary
    typedef itk::MaskImageFilter< FloatVector2ImageType, UnsignedCharScalarImageType, FloatVector2ImageType > MaskFilterType;
    MaskFilterType::Pointer maskFilter = MaskFilterType::New();
    //maskFilter->SetInput1(gradientFilter->GetOutput());
    //maskFilter->SetInput2(this->BoundaryImage);
    maskFilter->SetInput(gradientFilter->GetOutput());
    maskFilter->SetMaskImage(this->BoundaryImage);
    maskFilter->Update();

    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(maskFilter->GetOutput(), "Debug/ComputeBoundaryNormals.BoundaryNormalsUnnormalized.mha", this->DebugImages);
      
    //this->BoundaryNormals = maskFilter->GetOutput();
    //this->BoundaryNormals->Graft(maskFilter->GetOutput());
    Helpers::DeepCopy<FloatVector2ImageType>(maskFilter->GetOutput(), this->BoundaryNormals);

    // Normalize the vectors because we just care about their direction (the Data term computation calls for the normalized boundary normal)
    itk::ImageRegionIterator<FloatVector2ImageType> boundaryNormalsIterator(this->BoundaryNormals, this->BoundaryNormals->GetLargestPossibleRegion());
    itk::ImageRegionConstIterator<UnsignedCharScalarImageType> boundaryIterator(this->BoundaryImage, this->BoundaryImage->GetLargestPossibleRegion());

    while(!boundaryNormalsIterator.IsAtEnd())
      {
      if(boundaryIterator.Get()) // The pixel is on the boundary
        {
        FloatVector2ImageType::PixelType p = boundaryNormalsIterator.Get();
        p.Normalize();
        boundaryNormalsIterator.Set(p);
        }
      ++boundaryNormalsIterator;
      ++boundaryIterator;
      }

    Helpers::DebugWriteImageConditional<FloatVector2ImageType>(this->BoundaryNormals, "Debug/ComputeBoundaryNormals.BoundaryNormals.mha", this->DebugImages);
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeBoundaryNormals!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

itk::Index<2> CriminisiInpainting::FindHighestValueOnBoundary(const FloatScalarImageType::Pointer image, float& maxValue)
{
  // Return the location of the highest pixel on the current boundary. Return the value of that pixel by reference.
  try
  {
    // Explicity find the maximum on the boundary
    maxValue = 0; // priorities are non-negative, so anything better than 0 will win
    itk::Index<2> locationOfMaxValue;
    itk::ImageRegionConstIteratorWithIndex<UnsignedCharScalarImageType> boundaryIterator(this->BoundaryImage, this->BoundaryImage->GetLargestPossibleRegion());

    while(!boundaryIterator.IsAtEnd())
      {
      if(boundaryIterator.Get())
	{
	if(image->GetPixel(boundaryIterator.GetIndex()) > maxValue)
	  {
	  maxValue = image->GetPixel(boundaryIterator.GetIndex());
	  locationOfMaxValue = boundaryIterator.GetIndex();
	  }
	}
      ++boundaryIterator;
      }
    DebugMessage<float>("Highest value: ", maxValue);
    return locationOfMaxValue;
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in FindHighestValueOnBoundary!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::ComputeAllPriorities()
{
  try
  {
    // Only compute priorities for pixels on the boundary
    itk::ImageRegionConstIterator<UnsignedCharScalarImageType> boundaryIterator(this->BoundaryImage, this->BoundaryImage->GetLargestPossibleRegion());

    // Blank the priority image.
    this->PriorityImage->FillBuffer(0);

    // The main loop is over the boundary image. We only want to compute priorities at boundary pixels.
    unsigned int boundaryPixelCounter = 0;
    while(!boundaryIterator.IsAtEnd())
      {
      if(boundaryIterator.Get() != 0) // Pixel is on the boundary
	{
	float priority = ComputePriority(boundaryIterator.GetIndex());
	//DebugMessage<float>("Priority: ", priority);
	this->PriorityImage->SetPixel(boundaryIterator.GetIndex(), priority);
	boundaryPixelCounter++;
	}    
      ++boundaryIterator;
      }
    DebugMessage<unsigned int>("Number of boundary pixels: ", boundaryPixelCounter);
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeAllPriorities!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

float CriminisiInpainting::ComputePriority(const itk::Index<2>& queryPixel)
{
  //double confidence = ComputeConfidenceTerm(queryPixel);
  //double data = ComputeDataTerm(queryPixel);

  float confidence = this->ConfidenceImage->GetPixel(queryPixel);
  float data = this->DataImage->GetPixel(queryPixel);

  float priority = confidence * data;

  return priority;
}

float CriminisiInpainting::ComputeConfidenceTerm(const itk::Index<2>& queryPixel)
{
  //DebugMessage<itk::Index<2>>("Computing confidence for ", queryPixel);
  try
  {
    // Allow for regions on/near the image border

    //itk::ImageRegion<2> region = this->CurrentMask->GetLargestPossibleRegion();
    //region.Crop(Helpers::GetRegionInRadiusAroundPixel(queryPixel, this->PatchRadius[0]));
    itk::ImageRegion<2> targetRegion = Helpers::GetRegionInRadiusAroundPixel(queryPixel, this->PatchRadius[0]);
    itk::ImageRegion<2> region = CropToValidRegion(targetRegion);
    
    itk::ImageRegionConstIterator<Mask> maskIterator(this->CurrentMask, region);

    // The confidence is computed as the sum of the confidences of patch pixels in the source region / area of the patch

    float sum = 0;

    while(!maskIterator.IsAtEnd())
      {
      if(this->CurrentMask->IsValid(maskIterator.GetIndex()))
        {
        sum += this->ConfidenceMapImage->GetPixel(maskIterator.GetIndex());
        }
      ++maskIterator;
      }

    unsigned int numberOfPixels = GetNumberOfPixelsInPatch();
    float areaOfPatch = static_cast<float>(numberOfPixels);
    
    float confidence = sum/areaOfPatch;
    DebugMessage<float>("Confidence: ", confidence);

    return confidence;
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeConfidenceTerm!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

float CriminisiInpainting::ComputeDataTerm(const itk::Index<2>& queryPixel)
{
  try
  {
    FloatVector2Type isophote = this->IsophoteImage->GetPixel(queryPixel);
    FloatVector2Type boundaryNormal = this->BoundaryNormals->GetPixel(queryPixel);

    DebugMessage<FloatVector2Type>("Isophote: ", isophote);
    DebugMessage<FloatVector2Type>("Boundary normal: ", boundaryNormal);
    // D(p) = |dot(isophote at p, normalized normal of the front at p)|/alpha

    vnl_double_2 vnlIsophote(isophote[0], isophote[1]);

    vnl_double_2 vnlNormal(boundaryNormal[0], boundaryNormal[1]);

    float dot = std::abs(dot_product(vnlIsophote,vnlNormal));

    float alpha = 255; // This doesn't actually contribue anything, since the argmax of the priority is all that is used, and alpha ends up just being a scaling factor since the proiority is purely multiplicative.
    float dataTerm = dot/alpha;

    return dataTerm;
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeDataTerm!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

void CriminisiInpainting::UpdateConfidences(const itk::ImageRegion<2>& targetRegion, const float value)
{
  try
  {
    // Force the region to update to be entirely inside the image
    itk::ImageRegion<2> region = CropToValidRegion(targetRegion);
    
    // Use an iterator to find masked pixels. Compute their new value, and save it in a vector of pixels and their new values.
    // Do not update the pixels until after all new values have been computed, because we want to use the old values in all of
    // the computations.
    itk::ImageRegionConstIterator<Mask> maskIterator(this->CurrentMask, region);

    while(!maskIterator.IsAtEnd())
      {
      if(this->CurrentMask->IsHole(maskIterator.GetIndex()))
	{
	itk::Index<2> currentPixel = maskIterator.GetIndex();
	this->ConfidenceMapImage->SetPixel(currentPixel, value);
	}

      ++maskIterator;
      } // end while looop with iterator

  } // end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in UpdateConfidences!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}


void CriminisiInpainting::ComputeAllDataTerms()
{
  try
  {
    itk::ImageRegionConstIteratorWithIndex<UnsignedCharScalarImageType> boundaryIterator(this->BoundaryImage, this->BoundaryImage->GetLargestPossibleRegion());

    // Blank the data term image
    this->DataImage->FillBuffer(0);

    while(!boundaryIterator.IsAtEnd())
      {
      if(boundaryIterator.Get() != 0) // This is a pixel on the current boundary
	{
	itk::Index<2> currentPixel = boundaryIterator.GetIndex();
	float dataTerm = ComputeDataTerm(currentPixel);
	this->DataImage->SetPixel(currentPixel, dataTerm);
	//DebugMessage<float>("Set DataTerm to ", dataTerm);
	}

      ++boundaryIterator;
      }
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeAllDataTerms!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}


void CriminisiInpainting::ComputeAllConfidenceTerms()
{
  try
  {
    itk::ImageRegionConstIteratorWithIndex<UnsignedCharScalarImageType> boundaryIterator(this->BoundaryImage, this->BoundaryImage->GetLargestPossibleRegion());

    // Blank the data term image
    this->ConfidenceImage->FillBuffer(0);

    while(!boundaryIterator.IsAtEnd())
      {
      if(boundaryIterator.Get() != 0) // This is a pixel on the current boundary
	{
	itk::Index<2> currentPixel = boundaryIterator.GetIndex();
	float confidenceTerm = ComputeConfidenceTerm(currentPixel);
	this->ConfidenceImage->SetPixel(currentPixel, confidenceTerm);
	}

      ++boundaryIterator;
      }
  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeAllConfidenceTerms!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

bool CriminisiInpainting::IsValidPatch(const itk::Index<2>& queryPixel, const unsigned int radius)
{
  // This function checks if a patch is completely inside the image and not intersecting the mask

  itk::ImageRegion<2> region = Helpers::GetRegionInRadiusAroundPixel(queryPixel, radius);
  return IsValidRegion(region);
}

bool CriminisiInpainting::IsValidRegion(const itk::ImageRegion<2>& region)
{
  return this->CurrentMask->IsValid(region);
}

unsigned int CriminisiInpainting::GetNumberOfPixelsInPatch()
{
  return this->GetPatchSize()[0]*this->GetPatchSize()[1];
}

itk::Size<2> CriminisiInpainting::GetPatchSize()
{
  itk::Size<2> patchSize;

  patchSize[0] = Helpers::SideLengthFromRadius(this->PatchRadius[0]);
  patchSize[1] = Helpers::SideLengthFromRadius(this->PatchRadius[1]);

  return patchSize;
}

itk::ImageRegion<2> CriminisiInpainting::CropToValidRegion(const itk::ImageRegion<2>& inputRegion)
{
  itk::ImageRegion<2> region = this->FullImageRegion;
  region.Crop(inputRegion);
  
  return region;
}

bool CriminisiInpainting::GetPotentialCandidatePatches(const unsigned int iteration, const unsigned int forwardLookId, CandidatePatches& candidatePatches)
{
  if(iteration < this->PotentialCandidatePatches.size())
    {
    candidatePatches = this->PotentialCandidatePatches[iteration][forwardLookId];
    return true;
    }
  return false;
}

bool CriminisiInpainting::GetUsedPatchPair(const unsigned int id, PatchPair& patchPair)
{
  if(id < this->UsedPatchPairs.size())
    {
    patchPair = this->UsedPatchPairs[id]; 
    return true;
    }
  return false;
}
  
unsigned int CriminisiInpainting::GetNumberOfCompletedIterations()
{
  return this->NumberOfCompletedIterations;
}

float CriminisiInpainting::ContinuationDifference(const itk::Index<2>& boundaryPixel, const PatchPair& patchPair)
{
  // 'boundaryPixel' should be a pixel from the target patch on the source/valid side of the boundary. We want to compare this pixel value and isophote to the pixel from the source patch that would be on the target/hole side of the boundary if that patch were to be copied.
  
  // The boundary between the source and target regions actually exists at two places in the image - once in the source patch and once in the target patch.
  // We need to compare the source region of the target patch to the target region of the source patch, because these are the parts of the image that will
  // be pasted together.
  
  if(this->CurrentMask->IsHole(boundaryPixel) || !patchPair.TargetPatch.Region.IsInside(boundaryPixel))
    {
    std::cerr << "Error: The input boundary pixel must be on the valid side of the boundary (not in the hole)!" << std::endl;
    exit(-1);
    }
  
  FloatVector2Type sourceSideIsophote = this->IsophoteImage->GetPixel(boundaryPixel);
    
  itk::Index<2> pixelAcrossBoundary = Helpers::GetNextPixelAlongVector(boundaryPixel, sourceSideIsophote);
  
  // Some pixels might not have a valid pixel on the other side of the boundary.
  bool valid = false;
  
  // If the next pixel along the isophote is in bounds and in the hole region of the patch, procede.
  if(patchPair.TargetPatch.Region.IsInside(pixelAcrossBoundary) && this->CurrentMask->IsHole(pixelAcrossBoundary))
    {
    valid = true;
    }
  else
    {
    // There is no requirement for the isophote to be pointing a particular orientation, so try to step along the negative isophote.
    sourceSideIsophote *= -1.0;
    pixelAcrossBoundary = Helpers::GetNextPixelAlongVector(boundaryPixel, sourceSideIsophote);
    if(patchPair.TargetPatch.Region.IsInside(pixelAcrossBoundary) && this->CurrentMask->IsHole(pixelAcrossBoundary))
      {
      valid = true;
      }
    }
  
  // If the pixel stepping along the isophote in either direction is valid, we can sensibly compute the differences.
  if(valid)
    {
    // Determine the position of the pixel relative to the patch corner.
    itk::Offset<2> intraPatchOffset = pixelAcrossBoundary - patchPair.TargetPatch.Region.GetIndex();
  
    // Determine the position of the corresponding pixel in the source patch.
    itk::Index<2> sourcePatchTargetPixel = patchPair.SourcePatch.Region.GetIndex() + intraPatchOffset;
    //std::cout << "sourcePatchTargetPixel: " << sourcePatchTargetPixel << std::endl;

    // Get the isophote on the hole side of the boundary
    FloatVector2Type targetSideIsophote = this->IsophoteImage->GetPixel(sourcePatchTargetPixel);
  
    // Compute the pixel difference.
    float pixelDifference = SelfPatchCompareAll::StaticPixelDifference(this->CompareImage->GetPixel(boundaryPixel), this->CompareImage->GetPixel(sourcePatchTargetPixel));
    //std::cout << "pixelDifference: " << pixelDifference << std::endl;
    float pixelDifferenceNormalized = pixelDifference / MaxPixelDifference; // This produces a score between 0 and 1.
    DebugMessage<float>("pixelDifferenceNormalized: ", pixelDifferenceNormalized);
    
    // Compute the isophote difference.
    // We want to compare the isophote on the target side of the boundary to the isophote on the source side of the boundary.
    float isophoteDifference = Helpers::AngleBetween(sourceSideIsophote, targetSideIsophote);
    //std::cout << "sourceSideIsophote: " << sourceSideIsophote << std::endl;
    //std::cout << "targetSideIsophote: " << targetSideIsophote << std::endl;
    //std::cout << "isophoteDifference: " << isophoteDifference << std::endl;
    
    float isophoteDifferenceNormalized = isophoteDifference/3.14159; // The maximum angle between vectors is pi, so this produces a score between 0 and 1.
    DebugMessage<float>("isophoteDifferenceNormalized: ", isophoteDifferenceNormalized);
    
    // This is a bad idea- I thought we didn't want to penalize isophotes for being different if they weren't strong, but we do - if one side of the boundary is not strong, the other shouldn't be either! 
    //float weightedIsophoteDifference = this->IsophoteImage->GetPixel(currentSourcePixel).GetNorm() * isophoteDifferenceNormalized; // We want to 
    
    float continuationDifference = (pixelDifferenceNormalized + isophoteDifferenceNormalized)/2.0; // This is a score between 0 and 1.
    DebugMessage<float>("continuationDifference: ", continuationDifference);
    
    return continuationDifference;
    }
  else
    {
    //std::cout << "Warning: continuationDifference skipped." << std::endl;
    //DebugMessage(Warning: continuationDifference skipped.);
    return 0.0f; // Skip this comparison
    }
}

float CriminisiInpainting::ContinuationDifference(const PatchPair& patchPair)
{
  float totalContinuationDifference = 0.0f;
  
  // Identify border pixels on the source side of the boundary. Of course the boundary is defined in the target patch. The isophote extension is later check in the source patch.
  std::vector<itk::Index<2> > borderPixels = Helpers::GetNonZeroPixels<UnsignedCharScalarImageType>(this->BoundaryImage, patchPair.TargetPatch.Region);
  
  for(unsigned int i = 0; i < borderPixels.size(); ++i)
    {
    float difference = ContinuationDifference(borderPixels[i], patchPair);
    totalContinuationDifference += difference;
    }
  
  // We don't watch patches to be penalized for having longer boundaries
  float averageContinuationDifference = totalContinuationDifference / borderPixels.size();
  
  //return totalContinuationDifference;
  return averageContinuationDifference;
}

/*
unsigned int CriminisiInpainting::FindPatchWithBestContinuationDifference(const Patch& targetPatch)
{
  // This is a naive method that recomputes the boundary for every pair! This is way too slow to actually use.
  
  std::vector<PatchPair> patchPairs;
  std::vector<float> differences;
  
  for(unsigned int i = 0; i < this->SourcePatches.size(); ++i)
    {
    PatchPair patchPair;
    patchPair.SourcePatch = this->SourcePatches[i];
    patchPair.TargetPatch = targetPatch;
    
    float continuationDifference = ContinuationDifference(patchPair);
    differences.push_back(continuationDifference);
    }

  unsigned int bestPatchId = Helpers::argmin<float>(differences);
  return bestPatchId;
}
*/

std::vector<Patch> CriminisiInpainting::SortPatchesByContinuationDifference(const Patch& targetPatch)
{
  // Identify border pixels on the source side of the boundary.
  std::vector<itk::Index<2> > borderPixels = Helpers::GetNonZeroPixels<UnsignedCharScalarImageType>(this->BoundaryImage, targetPatch.Region);
  
  std::vector<Patch> patches;
  
  for(unsigned int sourcePatchId = 0; sourcePatchId < this->SourcePatches.size(); ++sourcePatchId)
    {
    PatchPair patchPair;
    patchPair.SourcePatch = this->SourcePatches[sourcePatchId];
    patchPair.TargetPatch = targetPatch;
    
    float totalContinuationDifference = 0.0f;
  
    for(unsigned int i = 0; i < borderPixels.size(); ++i)
      {
      float difference = ContinuationDifference(borderPixels[i], patchPair);
      totalContinuationDifference += difference;
      }

    Patch patch = this->SourcePatches[sourcePatchId];
    patch.SortValue = totalContinuationDifference;
    patch.Id = sourcePatchId;
    patches.push_back(patch);
    }

  //unsigned int bestPatchId = Helpers::argmin<float>(differences);
  
  std::sort(patches.begin(), patches.end(), SortBySortValue);
  
  return patches;
}

bool CriminisiInpainting::GetAllPotentialCandidatePatches(const unsigned int iteration, std::vector<CandidatePatches>& allCandidatePatches)
{
  if(iteration < this->PotentialCandidatePatches.size())
    {
    allCandidatePatches = this->PotentialCandidatePatches[iteration];
    return true;
    }
  else
    {
    return false;
    }
}
