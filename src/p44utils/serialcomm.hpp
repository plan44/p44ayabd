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

#ifndef __p44utils__serialcomm__
#define __p44utils__serialcomm__

#include "p44_common.hpp"

#include "fdcomm.hpp"

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>


using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    SerialCommErrorOK,
    SerialCommErrorInvalidHost,
    SerialCommErrorUnknownBaudrate,
  } SerialCommErrors;

  class SerialCommError : public Error
  {
  public:
    static const char *domain() { return "SerialComm"; }
    virtual const char *getErrorDomain() const { return SerialCommError::domain(); };
    SerialCommError(SerialCommErrors aError) : Error(ErrorCode(aError)) {};
    SerialCommError(SerialCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };



  class SerialComm;
  typedef boost::intrusive_ptr<SerialComm> SerialCommPtr;

  /// A class providing serialized access to a serial device attached directly or via a TCP proxy
  class SerialComm : public FdComm
  {
    typedef FdComm inherited;

    // serial connection
    string connectionPath;
    uint16_t connectionPort;
    int baudRate;
    bool connectionOpen;
    int connectionFd;
    struct termios oldTermIO;
    bool serialConnection;
    bool reconnecting;

  public:

    SerialComm(MainLoop &aMainLoop);
    virtual ~SerialComm();

    /// Specify the serial connection parameters as single string
    /// @param aConnectionSpec "/dev[:baudrate]" or "hostname[:port]"
    /// @param aDefaultPort default port number for TCP connection (irrelevant for direct serial device connection)
    /// @param aDefaultBaudRate default baud rate for serial connection (irrelevant for TCP connection)
    void setConnectionSpecification(const char* aConnectionSpec, uint16_t aDefaultPort, int aDefaultBaudRate);

    /// Set the serial connection parameters
    /// @param aConnectionPath serial device path (/dev/...) or host name/address (1.2.3.4 or xxx.yy)
    /// @param aPortNo port number for TCP connection (irrelevant for direct serial device connection)
    /// @param aBaudRate baud rate for serial connection (irrelevant for TCP connection)
    void setConnectionParameters(const char* aConnectionPath, uint16_t aPortNo, int aBaudRate);

    /// establish the serial connection
    /// @note can be called multiple times, opens connection only if not already open
    /// @return error in case connection cannot be opened
    ErrorPtr establishConnection();

    /// tries to establish the connection, and will retry if opening fails right now
    /// @return true if connection is open now
    bool requestConnection();

    /// close the current connection, if any
    void closeConnection();

    /// check if connection is currently open
    bool connectionIsOpen();

  protected:

    /// This is called when
    /// an exception (HUP or error) occurs on the file descriptor
    virtual void dataExceptionHandler(int aFd, int aPollFlags);


  private:

    void reconnectHandler();

  };

} // namespace p44

#endif /* defined(__p44utils__serialcomm__) */
