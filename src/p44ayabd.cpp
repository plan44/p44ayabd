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
#include "patternqueue.hpp"
#include "jsoncomm.hpp"

using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE
#define DEFAULT_STATE_DIR "/tmp"

#define MAINLOOP_CYCLE_TIME_uS 33333 // 33mS



class P44ayabd : public CmdLineApp
{
  typedef CmdLineApp inherited;

  AyabCommPtr ayabComm;

  PatternContainerPtr pattern;

  bool apiMode; ///< set if in API mode (means working as daemon, not quitting when job is done)
  // API Server
  SocketCommPtr apiServer;

  PatternQueuePtr patternQueue;
  string statedir;

public:

  P44ayabd() :
    apiMode(false)
  {
  };


  virtual int main(int argc, char **argv)
  {
    const char *usageText =
    "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 'l', "loglevel",        true,  "level;set max level of log message detail to show on stderr" },
      { 'W', "jsonapiport",     true,  "port;server port number for JSON API" },
      { 0  , "jsonapinonlocal", false, "allow connection to JSON API from non-local clients" },
      { 0  , "knitpng",         true,  "png_file;simple mode: just knit specified PNG file and then exit" },
      { 0  , "ayabconnection",  true,  "serial_if;serial interface where AYAB is connected (/device or IP:port - or 'simulation' for test w/o actual AYAB)" },
      { 0  , "statedir",        true,  "path;writable directory where to store state information. Defaults to " DEFAULT_STATE_DIR },
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

    // state dir
    statedir = DEFAULT_STATE_DIR;
    getStringOption("statedir", statedir);

    // app now ready to run
    return run();
  }


  virtual void initialize()
  {
    ErrorPtr err;

    // get AYAB connection
    // - set interface
    string ayabconnection;
    if (getStringOption("ayabconnection", ayabconnection)) {
      ayabComm = AyabCommPtr(new AyabComm(MainLoop::currentMainLoop()));
      ayabComm->setConnectionSpecification(ayabconnection.c_str(), 2109);
    }
    else {
      terminateApp(TextError::err("no connection specified for AYAB"));
    }
    // check mode
    string p;
    if (getStringOption("knitpng", p)) {
      simpleMode(p);
    }
    else if (getStringOption("jsonapiport", p)) {
      // API mode
      apiMode = true;
      // - create queue
      patternQueue = PatternQueuePtr(new PatternQueue);
      patternQueue->loadState(statedir.c_str());

      // - start API server and wait for things to happen
      apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
      apiServer->setConnectionParams(NULL, p.c_str(), SOCK_STREAM, AF_INET);
      apiServer->setAllowNonlocalConnections(getOption("jsonapinonlocal"));
      apiServer->startServer(boost::bind(&P44ayabd::apiConnectionHandler, this, _1), 3);
    }
    else {
      // unknown mode
      terminateApp(TextError::err("Must use either --knitpng or --jsonapiport"));
    }
  };


  void cleanup(int aExitCode)
  {
    // clean up
    if (patternQueue) {
      patternQueue->saveState(statedir.c_str(), true); // save anyway
    }
  }


  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&P44ayabd::apiRequestHandler, this, conn, _1, _2));
    return conn;
  }


  void apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
  {
    ErrorPtr err;
    JsonObjectPtr answer = JsonObject::newObj();
    // Decode mg44-style request (HTTP wrapped in JSON)
    if (Error::isOK(aError)) {
      LOG(LOG_INFO,"API request: %s\n", aRequest->c_strValue());
      JsonObjectPtr o;
      o = aRequest->get("method");
      if (o) {
        string method = o->stringValue();
        string uri;
        o = aRequest->get("uri");
        if (o) uri = o->stringValue();
        JsonObjectPtr data;
        bool action = (method!="GET");
        if (action) {
          data = aRequest->get("data");
        }
        else {
          data = aRequest->get("uri_params");
          if (data) action = true; // GET, but with query_params: treat like PUT/POST with data
        }
        // request elements now: uri and data
        JsonObjectPtr r = processRequest(uri, data, action);
        if (r) answer->add("result", r);
      }
    }
    else {
      LOG(LOG_ERR,"Invalid JSON request");
      answer->add("Error", JsonObject::newString(aError->description()));
    }
    LOG(LOG_INFO,"API answer: %s\n", answer->c_strValue());
    err = aConnection->sendMessage(answer);
    aConnection->closeAfterSend();
  }


  JsonObjectPtr processRequest(string aUri, JsonObjectPtr aData, bool aIsAction)
  {
    if (aUri=="/queue") {
      if (aIsAction) {
        // check action to execute on queue
        JsonObjectPtr o = aData->get("addFile");
        if (o) {
          JsonObjectPtr p = aData->get("webURL");
          patternQueue->addFile(o->stringValue(), p->stringValue());
          patternQueue->saveState(statedir.c_str(), false);
        }
      }
      return patternQueue->queueEntriesJSON();
    }
    else if (aUri=="/cursor") {
      return patternQueue->cursorStateJSON();
    }
    // unknown
    return JsonObjectPtr();
  }


  void simpleMode(string aPNGfilename)
  {
    ErrorPtr err;
    // simple mode: just knit a PNG file passed via command line
    LOG(LOG_NOTICE, "Simple mode - knitting single PNG file: %s\n", aPNGfilename.c_str());
    apiMode = false;
    pattern = PatternContainerPtr(new PatternContainer);
    err = pattern->readPNGfromFile(aPNGfilename.c_str());
    if (Error::isOK(err)) {
      // start knitting it
      sleep(3);
      initiateKnitting();
    }
    else {
      terminateApp(err);
    }
  }


  void doneSimpleMode()
  {
    LOG(LOG_NOTICE, "Done knitting single PNG\n");
    terminateApp(EXIT_SUCCESS);
  }



  void initiateKnitting()
  {
    // height of image is width of knit
    int w = pattern->width();
    if (!ayabComm->startKnittingJob(100-w/2, w, boost::bind(&P44ayabd::rowCallBack, this, _1, _2))) {
      // repeat in case of immediate failure
      // (Note: usually rowCallBack will be called with Error as long as machine is not ready)
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
      // check for end of knit
      if (!row && !apiMode) {
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44ayabd::doneSimpleMode, this), 2*Second);
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


