/*=========================================================================
 *
 *  Copyright David Doria 2012 daviddoria@gmail.com
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

#ifndef PatchVerificationDialog_HPP
#define PatchVerificationDialog_HPP

#include "PatchVerificationDialog.h" // Appease syntax parser

// ITK
#include "itkCastImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMaskImageFilter.h"
#include "itkRegionOfInterestImageFilter.h"

// Qt
#include <QGraphicsPixmapItem>

// Submodules
#include "Interactive/Delegates/PixmapDelegate.h"
#include "Interactive/ManualPatchSelectionDialog.h"
#include "Utilities/PatchHelpers.h"
#include "InteractorStyleImageWithDrag.h"

// Custom
#include <Helpers/Helpers.h>
#include <ITKVTKHelpers/ITKVTKHelpers.h>
#include <QtHelpers/QtHelpers.h>
#include <Mask/Mask.h>

template <typename TImage>
PatchVerificationDialog<TImage>::
PatchVerificationDialog(const TImage* const image, const Mask* const mask,
                        const unsigned int patchHalfWidth, QWidget* parent) :
  PatchVerificationDialogParent(parent), Image(image), MaskImage(mask),
  PatchHalfWidth(patchHalfWidth)
{
  this->setupUi(this);

//   if(image->GetNumberOfComponentsPerPixel() == 3)
//   {
//     // assume the image is RGB, and use it directly
//     ITKHelpers::DeepCopy(image, this->Image);
//   }
  this->MaskedQueryPatchItem = new QGraphicsPixmapItem;
  this->QueryPatchScene = new QGraphicsScene();
  this->gfxQueryPatch->setScene(this->QueryPatchScene);

  this->ProposedPatchItem = new QGraphicsPixmapItem;
  this->ProposedPatchScene = new QGraphicsScene();
  this->gfxProposedPatch->setScene(this->ProposedPatchScene);

  this->SourcePatchItem = new QGraphicsPixmapItem;
  this->SourcePatchScene = new QGraphicsScene();
  this->gfxSourcePatch->setScene(this->SourcePatchScene);
}

template <typename TImage>
void PatchVerificationDialog<TImage>::SetSourceNode(const Node& sourceNode)
{
  this->SourceNode = sourceNode;
}

template <typename TImage>
template <typename TNode>
void PatchVerificationDialog<TImage>::SetSourceNode(const TNode& sourceNode)
{
  this->SourceNode = Helpers::ConvertFrom<Node, TNode>(sourceNode);
}

template <typename TImage>
void PatchVerificationDialog<TImage>::SetQueryNode(const Node& queryNode)
{
  this->QueryNode = queryNode;

  itk::Index<2> queryIndex = ITKHelpers::CreateIndex(queryNode);
  itk::ImageRegion<2> queryRegion =
      ITKHelpers::GetRegionInRadiusAroundPixel(queryIndex, this->PatchHalfWidth);

  // Make sure the query is inside the image
  queryRegion.Crop(this->Image->GetLargestPossibleRegion());

  typename TImage::Pointer regionImage = TImage::New();
  ITKHelpers::ExtractRegion(this->Image, queryRegion,
                            regionImage.GetPointer());

  Mask::Pointer regionMask = Mask::New();
  ITKHelpers::ExtractRegion(this->MaskImage, queryRegion,
                            regionMask.GetPointer());
  regionMask->CopyInformationFrom(this->MaskImage);
//  std::cout << "There are " << regionMask->CountHolePixels() << " hole pixels." << std::endl;

  typename TImage::PixelType zeroPixel(3);
  zeroPixel = itk::NumericTraits<typename TImage::PixelType>::ZeroValue(zeroPixel);
//  regionMask->ApplyToImage(regionImage.GetPointer(), zeroPixel);

  // We must now refer to the regionImage as a new, standalone image
  // (use it's LargestPossibleRegion instead of queryRegion)
  QImage maskedQueryPatch =
//      ITKQtHelpers::GetQImageColor(regionImage.GetPointer(), queryRegion);
      ITKQtHelpers::GetQImageColor(regionImage.GetPointer(), regionImage->GetLargestPossibleRegion());
  this->MaskedQueryPatchItem =
      this->QueryPatchScene->addPixmap(QPixmap::fromImage(maskedQueryPatch));

  // We do this here because we could potentially call SetQueryNode after
  // the widget is constructed as well.
  gfxQueryPatch->fitInView(MaskedQueryPatchItem);
}

template <typename TImage>
Node PatchVerificationDialog<TImage>::GetSelectedNode() const
{
  return this->SelectedNode;
}

template <typename TImage>
void PatchVerificationDialog<TImage>::showEvent(QShowEvent*)
{
//  std::cout << "TopPatchesDialog::showEvent()" << std::endl;

  // We do this here because we will usually call SetQueryNode before the widget is constructed (i.e. before exec() is called).
  gfxQueryPatch->fitInView(this->MaskedQueryPatchItem);

  // Setup the proposed patch
  gfxProposedPatch->fitInView(this->ProposedPatchItem);

  DisplayPatchSelection();
}

template <typename TImage>
void PatchVerificationDialog<TImage>::closeEvent(QCloseEvent*)
{
  std::cout << "There is no recourse if you do not select a patch!" << std::endl;
  QApplication::quit();
}

template <typename TImage>
void PatchVerificationDialog<TImage>::on_btnAccept_clicked()
{
  this->SelectedNode = this->SourceNode;

  this->NumberOfVerifications++;

  accept();
}

template <typename TImage>
void PatchVerificationDialog<TImage>::on_btnSelectManually_clicked()
{
  itk::Index<2> queryIndex = ITKHelpers::CreateIndex(this->QueryNode);

  itk::ImageRegion<2> queryRegion =
      ITKHelpers::GetRegionInRadiusAroundPixel(queryIndex, this->PatchHalfWidth);

  ManualPatchSelectionDialog<TImage> manualPatchSelectionDialog(this->Image, this->MaskImage, queryRegion);
  manualPatchSelectionDialog.exec();

  if(manualPatchSelectionDialog.result() == QDialog::Rejected)
  {
    std::cout << "Did not choose patch manually." << std::endl;
  }
  else if(manualPatchSelectionDialog.result() == QDialog::Accepted)
  {
//    std::cout << "Chose patch manually." << std::endl;
    this->NumberOfManualSelections++;

    this->SelectedNode = manualPatchSelectionDialog.GetSelectedNode();

//    std::cout << "SelectedNode : " << this->SelectedNode[0] << " "
//              << this->SelectedNode[1] << std::endl;

    // Close the dialog
    accept();
  }
}

template <typename TImage>
void PatchVerificationDialog<TImage>::Report()
{
  std::cout << "Selection report:" << std::endl
            << "Verifications: " << this->NumberOfVerifications << std::endl
            << "Manual Selections: " << this->NumberOfManualSelections << std::endl;
}

template <typename TImage>
void PatchVerificationDialog<TImage>::PositionNextToParent()
{
  if(this->parentWidget())
  {
    this->move(this->parentWidget()->pos().x() + this->parentWidget()->width(),
               this->parentWidget()->pos().y());
  }
}

template <typename TImage>
void PatchVerificationDialog<TImage>::slot_UpdateResult(const itk::ImageRegion<2>& sourceRegionIn,
                                                        const itk::ImageRegion<2>& targetRegionIn)
{
//  std::cout << "TopPatchesDialog::slot_UpdateResult" << std::endl;

  // Crop the source region to look like the potentially cropped query region. We must do this before we crop the target region.
  itk::ImageRegion<2> sourceRegion = ITKHelpers::CropRegionAtPosition(sourceRegionIn, this->MaskImage->GetLargestPossibleRegion(), targetRegionIn);
  itk::ImageRegion<2> targetRegion = targetRegionIn;
  targetRegion.Crop(this->MaskImage->GetLargestPossibleRegion());

  assert(sourceRegion.GetSize() == targetRegion.GetSize());

  QImage qimage(sourceRegion.GetSize()[0], sourceRegion.GetSize()[1], QImage::Format_RGB888);

  itk::ImageRegionConstIterator<TImage> sourceIterator(this->Image, sourceRegion);
  itk::ImageRegionConstIterator<TImage> targetIterator(this->Image, targetRegion);
  itk::ImageRegionConstIterator<Mask> maskIterator(this->MaskImage, targetRegion);

  typename TImage::Pointer resultPatchImage = TImage::New();
  resultPatchImage->SetNumberOfComponentsPerPixel(this->Image->GetNumberOfComponentsPerPixel());

  itk::Index<2> corner = {{0,0}};
  itk::ImageRegion<2> resultPatchRegion(corner, sourceRegion.GetSize());

  resultPatchImage->SetRegions(resultPatchRegion);
  resultPatchImage->Allocate();

  while(!maskIterator.IsAtEnd())
  {
    typename TImage::PixelType pixel;

    if(this->MaskImage->IsHole(maskIterator.GetIndex()))
    {
      pixel = sourceIterator.Get();
    }
    else
    {
      pixel = targetIterator.Get();
    }

    itk::Offset<2> offset = sourceIterator.GetIndex() - sourceRegion.GetIndex();
    itk::Index<2> offsetIndex;
    offsetIndex[0] = offset[0];
    offsetIndex[1] = offset[1];
    resultPatchImage->SetPixel(offsetIndex, pixel);

    ++sourceIterator;
    ++targetIterator;
    ++maskIterator;
  }

  qimage = ITKQtHelpers::GetQImageColor(resultPatchImage.GetPointer(), resultPatchImage->GetLargestPossibleRegion());

  this->ProposedPatchScene->clear();
  QGraphicsPixmapItem* item = this->ProposedPatchScene->addPixmap(QPixmap::fromImage(qimage));
  gfxProposedPatch->fitInView(item);

//  std::cout << "Leave TopPatchesDialog::slot_UpdateResult" << std::endl;
}

template <typename TImage>
void PatchVerificationDialog<TImage>::DisplayPatchSelection()
{
//  std::cout << "Leave TopPatchesDialog::DisplayPatchSelection" << std::endl;
  itk::Index<2> queryIndex = ITKHelpers::CreateIndex(this->QueryNode);

  itk::ImageRegion<2> queryRegion =
      ITKHelpers::GetRegionInRadiusAroundPixel(queryIndex, this->PatchHalfWidth);

  itk::Index<2> sourceIndex = ITKHelpers::CreateIndex(this->SourceNode);

  itk::ImageRegion<2> sourceRegion =
      ITKHelpers::GetRegionInRadiusAroundPixel(sourceIndex, this->PatchHalfWidth);

  // Crop the source region to look like the potentially cropped query region. We must do this before we crop the target region.
  sourceRegion = ITKHelpers::CropRegionAtPosition(sourceRegion, this->MaskImage->GetLargestPossibleRegion(), queryRegion);

  queryRegion.Crop(this->MaskImage->GetLargestPossibleRegion());

  assert(sourceRegion.GetSize() == queryRegion.GetSize());

  emit signal_SelectedRegion(sourceRegion);

  // Double check that there are no hole pixels in the source patch
//  {
//  Mask::Pointer regionMask = Mask::New();
//  ITKHelpers::ExtractRegion(this->MaskImage, sourceRegion,
//                            regionMask.GetPointer());
//  regionMask->CopyInformationFrom(this->MaskImage);
//  std::cout << "There are " << regionMask->CountHolePixels() << " hole pixels in the source region." << std::endl;
//  }

  QImage sourcePatch = ITKQtHelpers::GetQImageColor(this->Image, sourceRegion);
//      PatchHelpers::GetQImageCombinedPatch(this->Image, sourceRegion, queryRegion, this->MaskImage);

  this->SourcePatchItem =
      this->SourcePatchScene->addPixmap(QPixmap::fromImage(sourcePatch));

  gfxSourcePatch->fitInView(this->SourcePatchItem);

  slot_UpdateResult(sourceRegion, queryRegion);

//  std::cout << "Leave TopPatchesDialog::DisplayPatchSelection" << std::endl;
}

#endif
