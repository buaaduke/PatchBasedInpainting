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

#include "PixelCollection.h"

int main(int argc, char*argv[])
{
  PixelCollection pixels;
  for(unsigned int i = 0; i < 4; ++i)
    {
    itk::Index<2> index = {{i, i}};
    pixels.AddPixel(index);
    }

  for(PixelCollection::ConstIterator pixelIterator = pixels.begin(); pixelIterator != pixels.end(); ++pixelIterator)
    {
    std::cout << *pixelIterator << std::endl;
    }

  return EXIT_SUCCESS;
}