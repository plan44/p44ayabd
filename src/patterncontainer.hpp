//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44ayabd.
//
//  p44ayabd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44ayabd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44ayabd. If not, see <http://www.gnu.org/licenses/>.
//


#ifndef __p44ayabd__patterncontainer__
#define __p44ayabd__patterncontainer__

#include "p44utils_common.hpp"

#include <png.h>

using namespace std;

namespace p44 {

class PatternContainer;

typedef boost::intrusive_ptr<PatternContainer> PatternContainerPtr;

class PatternContainer : public P44Obj
{
  typedef P44Obj inherited;

  png_image pngImage; /// The control structure used by libpng
  png_bytep pngBuffer; /// byte buffer


public:

  PatternContainer();

  /// clear container
  void clear();

  /// read pattern from file
  ErrorPtr readPNGfromFile(const char *aPNGFileName);

  /// dump pattern to console in ASCII-Art style
  void dumpPatternToConsole();

  /// get pattern width
  int width();

  /// get pattern length
  int length();

  /// get gray value at given point
  uint8_t grayAt(int aAtLenght, int aAtWidth);

private:

};





} // namespace p44

#endif /* defined(__p44ayabd__patterncontainer__) */
