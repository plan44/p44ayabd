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

#ifndef __p44ayabd__ayabcomm__
#define __p44ayabd__ayabcomm__

#include "p44_common.hpp"

#include "serialqueue.hpp"

using namespace std;

namespace p44 {


  class AyabComm;
  class AyabRow;


  typedef boost::intrusive_ptr<AyabRow> AyabRowPtr;
  class AyabRow : public P44Obj
  {
    typedef P44Obj inherited;

  public:
    AyabRow();
    virtual ~AyabRow();

    void clear();
    void setRowSize(size_t aRowSize);
    void setRowPixel(size_t aPixelNo, bool aValue);

    size_t rowSize;
    bool *rowData;
  };






  // Knitting line by line callback
  typedef boost::function<AyabRowPtr (int aRowNum, ErrorPtr aError)> AyabRowCB;


  typedef boost::intrusive_ptr<AyabComm> AyabCommPtr;
  // Enocean communication
  class AyabComm : public SerialOperationQueue
  {
    typedef SerialOperationQueue inherited;

    bool simulated; ///< if set, AYAB is simulated on console
    bool fullspeedsim; ///< if set, entire knit job is run through automatically quickly

    uint16_t firstNeedle;
    uint16_t width;
    AyabRowCB rowCallBack;
    uint8_t nextRequestRow; ///< next row number we expect a request for
    int rowCount; ///< overall row counter

  public:

    AyabComm(MainLoop &aMainLoop);
    virtual ~AyabComm();

    /// set the connection parameters to connect to the Arduino with the AYAB shield
    /// @param aConnectionSpec serial device path (/dev/...) or host name/address[:port] (1.2.3.4 or xxx.yy)
    /// @param aDefaultPort default port number for TCP connection (irrelevant for direct serial device connection)
    void setConnectionSpecification(const char *aConnectionSpec, uint16_t aDefaultPort);

    /// start knitting job
    /// @param aFirstNeedle number of the first needle from the left to use (0..199)
    /// @param aWidth width in number of needles
    /// @param aRowCB is called once for every row, must return a AyabRow or nothing to end knitting job
    /// @return true if params ok, false otherwise
    bool startKnittingJob(unsigned aFirstNeedle, unsigned aWidth, AyabRowCB aRowCB);

  protected:

    /// called to process extra bytes after all pending operations have processed their bytes
    virtual ssize_t acceptExtraBytes(size_t aNumBytes, uint8_t *aBytes);

  private:

    void sendCommand(size_t aCmdLength, uint8_t *aCmdBytesP, StatusCB aStatusCB);
    void sendResponse(size_t aRespLength, uint8_t *aRespBytesP);
    void ayabCmdResponseHandler(StatusCB aStatusCB, SerialOperationPtr aOperation, ErrorPtr aError);

    void ayabVersionResponseHandler(ErrorPtr aError);
    void ayabStartedResponseHandler(ErrorPtr aError);

    void sendNextRow();

    bool simulationControlKeyHandler(char aKey);

  };



} // namespace p44

#endif /* defined(__p44ayabd__ayabcomm__) */
