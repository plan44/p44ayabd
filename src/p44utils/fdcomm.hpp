//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__fdcomm__
#define __p44utils__fdcomm__

#include "p44_common.hpp"

// unix I/O and network
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>


using namespace std;

namespace p44 {


  class FdComm;


  typedef boost::intrusive_ptr<FdComm> FdCommPtr;

  /// callback for signalling ready for receive or transmit, or error
  typedef boost::function<void (ErrorPtr aError)> FdCommCB;


  /// wrapper for non-blocking I/O on a file descriptor
  class FdComm : public P44Obj
  {
    FdCommCB receiveHandler;
    FdCommCB transmitHandler;

  protected:

    int dataFd;
    MainLoop &mainLoop;

  public:

    FdComm(MainLoop &aMainLoop);
    virtual ~FdComm();

    /// place to attach a related object
    P44ObjPtr relatedObject;

    /// Set file descriptor
    /// @param aFd the file descriptor to monitor, -1 to cancel monitoring
    void setFd(int aFd);

    /// Stop monitoring (unregister MainLoop callbacks) and close the file descriptor
    void stopMonitoringAndClose();

    /// Get file descriptor
    int getFd() { return dataFd; };

    /// write data (non-blocking)
    /// @param aNumBytes number of bytes to transfer
    /// @param aBytes pointer to buffer to be sent
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually written, can be 0 (e.g. if connection is still in process of opening)
    virtual size_t transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError);


    /// transmit string
    /// @param aString string to transmit
    /// @return true if string could be sent in one single attempt, false if not (truncated, not ready, error)
    /// @note intended for datagrams. Use transmitBytes to be able to handle partial transmission
    bool transmitString(string &aString);


    /// @return number of bytes ready for read
    size_t numBytesReady();

    /// read data (non-blocking)
    /// @param aNumBytes max number of bytes to receive
    /// @param aBytes pointer to buffer to store received bytes
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually read
    size_t receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError);

    /// read data into string
    ErrorPtr receiveString(string &aString, ssize_t aMaxBytes = -1);

    /// read data and append to string
    ErrorPtr receiveAndAppendToString(string &aString, ssize_t aMaxBytes = -1);

    /// install callback for data becoming ready to read
    /// @param aCallBack will be called when data is ready for reading (receiveBytes()) or an asynchronous error occurs on the file descriptor
    void setReceiveHandler(FdCommCB aReceiveHandler);

    /// install callback for file descriptor ready for accepting new data to send
    /// @param aCallBack will be called when file descriptor is ready to transmit more data (using transmitBytes())
    void setTransmitHandler(FdCommCB aTransmitHandler);

    /// make non-blocking
    /// @param aFd optional; fd to switch to non-blocking, defaults to this FdConn's fd set with setFd()
    void makeNonBlocking(int aFd = -1);


  protected:
    /// this is intended to be overridden in subclases, and is called when
    /// an exception (HUP or error) occurs on the file descriptor
    virtual void dataExceptionHandler(int aFd, int aPollFlags);

  private:

    bool dataMonitorHandler(MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags);
  };


  class FdStringCollector : public FdComm
  {
    bool ended; ///< set when FD returns error or HUP
    FdCommCB endedCallback; ///< called when collecting ends (after setup by collectToEnd())

  public:

    string collectedData; ///< all data received from the fd is collected into this string

    FdStringCollector(MainLoop &aMainLoop);

    /// collect until file descriptor does not provide any more data
    void collectToEnd(FdCommCB aEndedCallback);

  protected:

    virtual void dataExceptionHandler(int aFd, int aPollFlags);

  private:

    void gotData(ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<FdStringCollector> FdStringCollectorPtr;


} // namespace p44


#endif /* defined(__p44utils__fdcomm__) */
