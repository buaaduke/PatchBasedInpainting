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

#ifndef PriorityFactory_H
#define PriorityFactory_H

#include <string>
#include <vector>

#include "Priority.h"

/**
\class PriorityFactory
\brief This class constructs a Priority object based on a specified string/enum.
*/
template <typename TImage>
class PriorityFactory
{
public:
  enum PriorityTypes {DEPTH, MANUAL, ONIONPEEL, CRIMINISI, RANDOM, INVALID};
  static Priority<TImage>* Create(const PriorityTypes priorityType, const TImage* const image, const Mask* const maskImage, const unsigned int patchRadius);

  static std::string NameOfPriority(const PriorityTypes);
  static PriorityTypes PriorityTypeFromName(const std::string&);

  static std::vector<std::string> GetImageNames(const PriorityTypes& priorityType);

private:
  typedef std::map <PriorityTypes, std::string> PriorityNameMapType;
  static PriorityNameMapType DifferenceNameMap;
  static PriorityNameMapType CreateNameMap();
};

#include "PriorityFactory.hxx"

#endif