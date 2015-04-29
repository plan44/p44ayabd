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
#include "patterncontainer.hpp"


using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE
#define MAINLOOP_CYCLE_TIME_uS 33333 // 33mS



class P44ayabd : public CmdLineApp
{
  typedef CmdLineApp inherited;

  AyabCommPtr ayabComm;

  PatternContainerPtr pattern;

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
      { 0  , "png",             true,  "png_file;PNG file to knit" },
      { 0  , "ayabconnection",  true,  "serial_if;serial interface where AYAB is connected (/device or IP:port - or 'simulation' for test w/o actual AYAB)" },
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
    ErrorPtr err;

    string pngfile;
    if (!getStringOption("png", pngfile)) {
      terminateApp(TextError::err("Missing PNG file to knit"));
      return;
    }
    // read image
    pattern = PatternContainerPtr(new PatternContainer);
    err = pattern->readPNGfromFile(pngfile.c_str());
    if (Error::isOK(err)) {
      // start knitting it
      // - set interface
      string ayabconnection;
      if (getStringOption("ayabconnection", ayabconnection)) {
        ayabComm = AyabCommPtr(new AyabComm(MainLoop::currentMainLoop()));
        ayabComm->setConnectionSpecification(ayabconnection.c_str(), 2109);
        sleep(3);
        initiateKnitting();
      }
      else {
        terminateApp(TextError::err("no connection specified for AYAB"));
      }
    }
    else {
      terminateApp(err);
    }
  };


  void initiateKnitting()
  {
    // height of image is width of knit
    int w = pattern->width();
    if (!ayabComm->startKnittingJob(100-w/2, w, boost::bind(&P44ayabd::rowCallBack, this, _1, _2))) {
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
      if (aRowNum<pattern->length()) {
        // there is a row, return it
        row = AyabRowPtr(new AyabRow);
        row->setRowSize(pattern->width());
        int w = pattern->width();
        for (int y=w-1; y>=0; --y) {
          row->setRowPixel(y, pattern->grayAt(aRowNum, y)<128);
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


