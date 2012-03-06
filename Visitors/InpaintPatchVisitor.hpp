#ifndef InpaintPatchVisitor_HPP
#define InpaintPatchVisitor_HPP

// Helpers
#include "Helpers/ITKHelpers.h"
#include "Helpers/OutputHelpers.h"
#include "Helpers/BoostHelpers.h"

template <typename TImage>
struct InpaintPatchVisitor
{
  TImage* Image;
  Mask* MaskImage;

  const unsigned int PatchHalfWidth;

  InpaintPatchVisitor(TImage* const image, Mask* const mask,
                    const unsigned int patchHalfWidth) :
  Image(image), MaskImage(mask), PatchHalfWidth(patchHalfWidth)
  {
  }

  template <typename TNode>
  void PaintPatch(TNode target, TNode source) const
  {
    TNode target_patch_corner;
    target_patch_corner[0] = target[0] - PatchHalfWidth;
    target_patch_corner[1] = target[1] - PatchHalfWidth;

    TNode source_patch_corner;
    source_patch_corner[0] = source[0] - PatchHalfWidth;
    source_patch_corner[1] = source[1] - PatchHalfWidth;

    TNode target_node;
    TNode source_node;
    for(std::size_t i = 0; i < PatchHalfWidth * 2 + 1; ++i)
    {
      for(std::size_t j = 0; j < PatchHalfWidth * 2 + 1; ++j)
      {
        target_node[0] = target_patch_corner[0] + i;
        target_node[1] = target_patch_corner[1] + j;

        source_node[0] = source_patch_corner[0] + i;
        source_node[1] = source_patch_corner[1] + j;

        // Only paint the pixel if it is currently a hole
        if( MaskImage->IsHole(ITKHelpers::CreateIndex(target_node)) )
        {
          //std::cout << "Copying pixel " << source_node << " to pixel " << target_node << std::endl;
          PaintVertex(target_node, source_node); //paint the vertex.
        }

      }
    }
  };
  
  template <typename TNode>
  void PaintVertex(TNode target, TNode source) const
  {
    itk::Index<2> target_index = ITKHelpers::CreateIndex(target);

    itk::Index<2> source_index = ITKHelpers::CreateIndex(source);

    assert(Image->GetLargestPossibleRegion().IsInside(source_index));
    assert(Image->GetLargestPossibleRegion().IsInside(target_index));

    Image->SetPixel(target_index, Image->GetPixel(source_index));
  };

  void InpaintingComplete() const
  {
    //OutputHelpers::WriteImage(Image, "InpaintPatchVisitor::output.mha");
  }

}; // InpaintingVisitor

#endif
