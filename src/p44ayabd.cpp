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

#include "application.hpp"

#include "ayabcomm.hpp"

#include <png.h>

using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE
#define MAINLOOP_CYCLE_TIME_uS 33333 // 33mS



class P44ayabd : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // %%% tmp, move into PNG handler class later
  png_image image; /* The control structure used by libpng */
  png_bytep buffer;
  AyabCommPtr ayabComm;

public:

  P44ayabd()
  {
  }


  virtual int main(int argc, char **argv)
  {
    const char *usageText =
    "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 'l', "loglevel",        true,  "level;set max level of log message detail to show on stderr" },
      { 'W', "jsonapiport",     true,  "port;server port number for JSON API" },
      { 0  , "jsonapinonlocal", false, "allow connection to JSON API from non-local clients" },
      { 'h', "help",            false, "show this text" },
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    if (numOptions()<1) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // log level?
    int loglevel = DEFAULT_LOGLEVEL;
    getIntOption("loglevel", loglevel);
    SETLOGLEVEL(loglevel);
    SETERRLEVEL(loglevel, false); // all diagnostics go to stderr

    // app now ready to run
    return run();
  }

  virtual void initialize()
  {
//    // TODO: set up API
//    // - open a PNG file
//    FILE *fp = fopen("/Users/luz/Documents/plan44/Projekte/Guerilla-Knitting-Thalwil/p44ayabd/imgs/test.png", "rb");
//    if (!fp) {
//      terminateApp(SysError::errNo());
//      return;
//    }
//    // - read the header
//    const size_t headerSize = 8;
//    uint8_t header[headerSize];
//    ssize_t hb = fread(header, 1, headerSize, fp);
//    // - check the header
//    if (png_sig_cmp(header, 0, hb)) {
//      terminateApp(WebError::webError(415, "file is not a PNG image"));
//      return;
//    }
//    // - create info structures
//    png_structp png_ptr = png_create_read_struct(
//      PNG_LIBPNG_VER_STRING,
//      NULL, //%%% (png_voidp)user_error_ptr,
//      NULL, //%%% user_error_fn,
//      NULL //%%% user_warning_fn
//    );
//    if (!png_ptr) {
//      terminateApp(WebError::webError(500, "png_create_read_struct"));
//      return;
//    }
//    png_infop info_ptr = png_create_info_struct(png_ptr);
//    if (!info_ptr) {
//      png_destroy_read_struct(
//        &png_ptr,
//        (png_infopp)NULL,
//        (png_infopp)NULL
//      );
//      terminateApp(WebError::webError(500, "png_create_info_struct"));
//      return;
//    }
//    png_infop end_info = png_create_info_struct(png_ptr);
//    if (!end_info) {
//      png_destroy_read_struct(
//        &png_ptr,
//        (png_infopp)NULL,
//        (png_infopp)NULL
//      );
//      terminateApp(WebError::webError(500, "png_create_info_struct"));
//      return;
//    }
//    // - init IO
//    png_init_io(png_ptr, fp);
//    png_set_sig_bytes(png_ptr, (int)hb);


    /* Initialize the 'png_image' structure. */
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    /* The first argument is the file to read: */
    if (png_image_begin_read_from_file(&image, "/Users/luz/Documents/plan44/Projekte/Guerilla-Knitting-Thalwil/p44ayabd/imgs/test.png") != 0)
    {

      /* Set the format in which to read the PNG file; this code chooses a
       * simple sRGB format with a non-associated alpha channel, adequate to
       * store most images.
       */
      image.format = PNG_FORMAT_GRAY;

      /* Now allocate enough memory to hold the image in this format; the
       * PNG_IMAGE_SIZE macro uses the information about the image (width,
       * height and format) stored in 'image'.
       */
      buffer = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
      LOG(LOG_INFO, "Image size in bytes = %d\n", PNG_IMAGE_SIZE(image));
      LOG(LOG_INFO, "Image width = %d\n", image.width);
      LOG(LOG_INFO, "Image height = %d\n", image.height);
      LOG(LOG_INFO, "Image width*height = %d\n", image.height*image.width);


      /* If enough memory was available read the image in the desired format
       * then write the result out to the new file.  'background' is not
       * necessary when reading the image because the alpha channel is
       * preserved; if it were to be removed, for example if we requested
       * PNG_FORMAT_RGB, then either a solid background color would have to
       * be supplied or the output buffer would have to be initialized to the
       * actual background of the image.
       *
       * The fourth argument to png_image_finish_read is the 'row_stride' -
       * this is the number of components allocated for the image in each
       * row.  It has to be at least as big as the value returned by
       * PNG_IMAGE_ROW_STRIDE, but if you just allocate space for the
       * default, minimum, size using PNG_IMAGE_SIZE as above you can pass
       * zero.
       *
       * The final argument is a pointer to a buffer for the colormap;
       * colormaps have exactly the same format as a row of image pixels (so
       * you choose what format to make the colormap by setting
       * image.format).  A colormap is only returned if
       * PNG_FORMAT_FLAG_COLORMAP is also set in image.format, so in this
       * case NULL is passed as the final argument.  If you do want to force
       * all images into an index/color-mapped format then you can use:
       *
       *    PNG_IMAGE_COLORMAP_SIZE(image)
       *
       * to find the maximum size of the colormap in bytes.
       */
      if (
        buffer != NULL &&
        png_image_finish_read(
          &image,
          NULL /*background*/,
          buffer,
          0/*row_stride*/,
          NULL/*colormap*/
        ) != 0
      ) {
        // image read ok
        for (int x=0; x<image.width; x++) {
          for (int y=image.height-1; y>=0; --y) {
            fputc(buffer[y*image.width+x]<128 ? 'X' : '.', stdout);
          }
          fprintf(stdout, "\n");
        }
        // start knitting it
        // - height of image is width of knit
        ayabComm = AyabCommPtr(new AyabComm(MainLoop::currentMainLoop()));
//        ayabComm->setConnectionSpecification("/dev/cu.usbmodem14241", 2109);
        ayabComm->setConnectionSpecification("/dev/cu.usbmodem1421", 2109);
        sleep(3);
        initiateKnitting();
        return;
      }
      else {
        free(buffer);
      }
      /* Something went wrong reading or writing the image.  libpng stores a
       * textual message in the 'png_image' structure:
       */
      terminateApp(TextError::err("pngtopng: error: %s", image.message));
    }
    else {
      terminateApp(TextError::err("could not open PNG file"));
    }
  };


  void initiateKnitting()
  {
    if (!ayabComm->startKnittingJob(100-image.height/2, image.height, boost::bind(&P44ayabd::rowCallBack, this, _1, _2))) {
      //terminateApp(TextError::err("startKnittingJob failed"));
      MainLoop::currentMainLoop().executeOnce(boost::bind(&P44ayabd::initiateKnitting, this), 3*Second);
    }
  }


  AyabRowPtr rowCallBack(int aRowNum, ErrorPtr aError)
  {
    AyabRowPtr row;
    if (!Error::isOK(aError)) {
      LOG(LOG_ERR, "Knitting job aborted with error: %s\n", aError->description().c_str());
      // %%% restart
      MainLoop::currentMainLoop().executeOnce(boost::bind(&P44ayabd::initiateKnitting, this), 3*Second);
    }
    else {
      if (aRowNum<image.width) {
        // there is a row, return it
        row = AyabRowPtr(new AyabRow);
        row->setRowSize(image.height);
        for (int y=image.height-1; y>=0; --y) {
          row->setRowPixel(y, buffer[y*image.width+aRowNum]<128);
        }
      }
    }
    return row;
  };


};


int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create the mainloop
  MainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static P44ayabd application;
  // pass control
  return application.main(argc, argv);
}


