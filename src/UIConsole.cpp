/*
 * UIConsole.cpp
 *
 *      Author: Andreas Volz
 */

// project
#include "UIConsole.h"
#include "Logger.h"
#include "Hurricane.h"

// system
#include <png.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace std;

static Logger logger = Logger("startool.UIConsole");

namespace
{

bool readPngSize(const std::string &file, unsigned &width, unsigned &height)
{
  png_image image;
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, file.c_str()))
  {
    return false;
  }

  width = image.width;
  height = image.height;
  png_image_free(&image);
  return true;
}

bool cropPng(const std::string &inFile, const std::string &outFile,
             unsigned x, unsigned y, unsigned w, unsigned h)
{
  png_image image;
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, inFile.c_str()))
  {
    return false;
  }

  const unsigned srcW = image.width;
  const unsigned srcH = image.height;
  image.format = PNG_FORMAT_RGBA;

  if (x >= srcW || y >= srcH)
  {
    png_image_free(&image);
    return false;
  }
  if (x + w > srcW) w = srcW - x;
  if (y + h > srcH) h = srcH - y;

  png_bytep src = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
  if (!src)
  {
    png_image_free(&image);
    return false;
  }

  if (!png_image_finish_read(&image, NULL, src, 0, NULL))
  {
    free(src);
    return false;
  }

  const unsigned srcStride = PNG_IMAGE_ROW_STRIDE(image);
  png_bytep dst = (png_bytep)malloc((size_t)w * h * 4);
  if (!dst)
  {
    free(src);
    return false;
  }

  for (unsigned row = 0; row < h; row++)
  {
    memcpy(dst + (size_t)row * w * 4,
           src + (size_t)(y + row) * srcStride + (size_t)x * 4,
           (size_t)w * 4);
  }
  free(src);

  png_image outImage;
  memset(&outImage, 0, sizeof(outImage));
  outImage.version = PNG_IMAGE_VERSION;
  outImage.width = w;
  outImage.height = h;
  outImage.format = PNG_FORMAT_RGBA;

  bool ok = png_image_write_to_file(&outImage, outFile.c_str(), 0, dst, 0, NULL) != 0;
  free(dst);
  return ok;
}

} // anonymous namespace

UIConsole::UIConsole(std::shared_ptr<Hurricane> hurricane) :
  Converter(hurricane)
{

}

UIConsole::~UIConsole()
{

}

bool UIConsole::convert(Storage pngfile, int left, int right)
{
  bool result = true;

  string complete_file = pngfile.getFullPath() + ".png";
  string left_file = pngfile.getFullPath() + "_left.png";
  string right_file = pngfile.getFullPath() + "_right.png";
  string middle_file = pngfile.getFullPath() + "_middle.png";

  unsigned width = 0;
  unsigned height = 0;
  if (!readPngSize(complete_file, width, height))
  {
    return false;
  }

  const int middle = right - left;
  if (left < 0 || right <= left || right > (int)width)
  {
    return false;
  }

  // left slice  [0, left)
  if (result)
  {
    result = cropPng(complete_file, left_file, 0, 0, left, height);
  }
  // middle slice [left, right)
  if (result)
  {
    result = cropPng(complete_file, middle_file, left, 0, middle, height);
  }
  // right slice [right, width)
  if (result)
  {
    result = cropPng(complete_file, right_file, right, 0, width - right, height);
  }

  return result;
}
