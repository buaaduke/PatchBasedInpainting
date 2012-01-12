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

#ifndef PointCloudDescriptorCreator_H
#define PointCloudDescriptorCreator_H

#include "DescriptorCreator.h"

/**
\class PointCloudDescriptorCreator
\brief This class reads a file to create a descriptor for each pixel.
*/
class PointCloudDescriptorCreator : public DescriptorCreator
{
public:

  PointCloudDescriptorCreator(const std::string& fileName);

  Item* CreateItem(const itk::Index<2>& index) const;

private:
  std::map<itk::Index<2>, std::vector<float> > Descriptors;

};

#include "PointCloudDescriptorCreator.hxx"

#endif
