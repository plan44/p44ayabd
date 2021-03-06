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

#include "patterncontainer.hpp"


using namespace p44;


PatternContainer::PatternContainer() :
  pngBuffer(NULL),
  patternWidth(0),
  patternLength(0),
  imgOffsetW(0),
  imgOffsetL(0)
{
  clear();
}


void PatternContainer::clear()
{
  // init empty size
  patternLength = 0;
  patternWidth = 0;
  imgOffsetW = 0;
  imgOffsetL= 0;
  // init libpng image structure
  memset(&pngImage, 0, (sizeof pngImage));
  pngImage.version = PNG_IMAGE_VERSION;
  // free the buffer if allocated
  if (pngBuffer) {
    free(pngBuffer);
    pngBuffer = NULL;
  }
}


void PatternContainer::dumpPatternToConsole()
{
  if (pngBuffer) {
    for (int x=0; x<patternLength; x++) {
      for (int y=patternWidth-1; y>=0; --y) {
        fputc(grayAt(x, y)<128 ? 'X' : '.', stdout);
      }
      fprintf(stdout, "\n");
    }
  }
}


ErrorPtr PatternContainer::readPNGfromFile(const char *aPNGFileName)
{
  // clear any previous pattern
  clear();
  // read image
  if (png_image_begin_read_from_file(&pngImage, aPNGFileName) == 0) {
    // error
    return TextError::err("could not open PNG file %s", aPNGFileName);
  }
  else {
    // We only need the luminance
    pngImage.format = PNG_FORMAT_GRAY;
    // Now allocate enough memory to hold the image in this format; the
    // PNG_IMAGE_SIZE macro uses the information about the image (width,
    // height and format) stored in 'image'.
    pngBuffer = (png_bytep)malloc(PNG_IMAGE_SIZE(pngImage));
    LOG(LOG_INFO, "Image size in bytes = %d", PNG_IMAGE_SIZE(pngImage));
    LOG(LOG_INFO, "Image width = %d", pngImage.width);
    LOG(LOG_INFO, "Image height = %d", pngImage.height);
    LOG(LOG_INFO, "Image width*height = %d", pngImage.height*pngImage.width);
    if (pngBuffer==NULL) {
      return TextError::err("Could not allocate buffer for reading PNG file %s", aPNGFileName);
    }
    // now actually red the image
    if (png_image_finish_read(
      &pngImage,
      NULL, // background
      pngBuffer,
      0, // row_stride
      NULL //colormap
    ) == 0) {
      // error
      ErrorPtr err = TextError::err("Error reading PNG file %s: error: %s", aPNGFileName, pngImage.message);
      clear(); // clear only after pngImage.message has been used
      return err;
    }
  }
  // image read ok
  // - set size (Note: width and length reversed, as we need the pattern sidewards)
  patternLength = pngImage.width;
  patternWidth = pngImage.height;
  return ErrorPtr();
}


void PatternContainer::setSize(int aWidth, int aLength)
{
  patternWidth = aWidth;
  patternLength = aLength;
}


void PatternContainer::setContentOffset(int aOffsetW, int aOffsetL)
{
  imgOffsetW = aOffsetW;
  imgOffsetL = aOffsetL;
}


uint8_t PatternContainer::grayAt(int aAtLenght, int aAtWidth)
{
  if (
    pngBuffer == NULL ||
    aAtLenght<0 || aAtLenght>=length() ||
    aAtWidth<0 || aAtWidth>=width()
  ) {
    return 0; // outside pattern -> no color
  }
  // within pattern, apply offsets
  aAtLenght -= imgOffsetL;
  aAtWidth -= imgOffsetW;
  if (
    aAtLenght<0 || aAtLenght>=pngImage.width ||
    aAtWidth<0 || aAtWidth>=pngImage.height
  ) {
    return 0; // outside image -> no color
  }
  // inside image, return level of gray/black
  return 255-pngBuffer[aAtWidth*length()+aAtLenght]; // pixel information is amount of white, we want amount of black
}




