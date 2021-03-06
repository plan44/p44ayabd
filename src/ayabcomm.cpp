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

#include "ayabcomm.hpp"

#include "consolekey.hpp"
#include "application.hpp"

using namespace p44;

#define AYAB_COMMAPARMS "115200,8,N,1"


// AYAB serial protocol
#define AYAB_EXPECTED_FIRMWARE 4 // current version per November 2017
#define AYAB_SENDS_EXTRA_CRLF 1 // above version sends extra CRLF in confirm commands

#define AYABCMD_DEBUG 0x23 // debug message from hardware

#define AYABCMD_FROM_HOST 0x00 // command comes from host
#define AYABCMD_FROM_AYAB 0x80 // command comes from AYAB
#define AYABCMD_REQUEST 0x00 // command is a request
#define AYABCMD_CONFIRM 0x40 // command is a confirmation
#define AYABCMD_MSGID_MASK 0x0F // message identifier mask


#define AYABMSGID_START 1 // start
#define AYABMSGID_LINE 2 // line
#define AYABMSGID_INFO 3 // info
#define AYABMSGID_STATE 4 // state (v4 only, from AYAB only)
#define AYABMSGID_TEST 4 // test (v4 only, from HOST only)

#pragma mark - CRC8

// taken from EnOcean ESP, assuming AYAB will use this once it actually checks CRC...

static u_int8_t CRC8Table[256] = {
  0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
  0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
  0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
  0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
  0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
  0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
  0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
  0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
  0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
  0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
  0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
  0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
  0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
  0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
  0x76, 0x71, 0x78, 0x7f, 0x6A, 0x6d, 0x64, 0x63,
  0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
  0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
  0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
  0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8D, 0x84, 0x83,
  0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
  0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};


static uint8_t crc8(uint8_t *aDataP, size_t aNumBytes, uint8_t aCRCValue)
{
  int i;
  for (i = 0; i<aNumBytes; i++) {
    aCRCValue = CRC8Table[aCRCValue ^ aDataP[i]];
  }
  return aCRCValue;
}


#pragma mark - AyabRow

AyabRow::AyabRow() :
  rowSize(0),
  rowData(NULL)
{
}


AyabRow::~AyabRow()
{
  clear();
}


void AyabRow::clear()
{
  if (rowData!=NULL) {
    delete [] rowData;
    rowData = NULL;
  }
  rowSize = 0;
}


void AyabRow::setRowSize(size_t aRowSize)
{
  clear();
  if (aRowSize>0) {
    rowSize = aRowSize;
    rowData = new bool[rowSize];
  }
}


void AyabRow::setRowPixel(size_t aPixelNo, bool aValue)
{
  if (aPixelNo<rowSize) {
    rowData[aPixelNo] = aValue;
  }
}




#pragma mark - AyabComm

AyabComm::AyabComm(MainLoop &aMainLoop) :
	inherited(aMainLoop),
  simulated(false),
  fullspeedsim(false),
  firstNeedle(0),
  width(0),
  rowCallBack(NULL),
  rowCount(0),
  nextRequestRow(0),
  status(ayabstatus_offline)
{
}


AyabComm::~AyabComm()
{
}


void AyabComm::setConnectionSpecification(const char *aConnectionSpec, uint16_t aDefaultPort)
{
  LOG(LOG_DEBUG, "AyabComm::setConnectionSpecification: %s", aConnectionSpec);
  if (strcmp(aConnectionSpec,"simulation")==0) {
    // simulation mode
    simulated = true;
    // install console key to trigger rows
    ConsoleKeyManager::sharedKeyManager()->setKeyPressHandler(boost::bind(&AyabComm::simulationControlKeyHandler, this, _1));
    LOG(LOG_NOTICE, "SIMULATION MODE: press N to request next row, F to toggle full speed run");
    status = ayabstatus_connected;
  }
  else {
    serialComm->setConnectionSpecification(aConnectionSpec, aDefaultPort, AYAB_COMMAPARMS);
    // open connection so we can receive
    if (serialComm->requestConnection()) {
      status = ayabstatus_connected;
      serialComm->setDTR(false); // arduino reset
    }
    // set accept buffer for re-assembling messages before processing
    setAcceptBuffer(20); // largest message is 10 bytes (8 + possibly CRLF)
  }
}


bool AyabComm::simulationControlKeyHandler(char aKey)
{
  if (toupper(aKey)=='N') {
    // requst next row
    fullspeedsim = false; // terminate full speed
    sendNextRow();
  }
  else if (toupper(aKey)=='F') {
    fullspeedsim = !fullspeedsim;
    if (fullspeedsim)
      sendNextRow();
  }
  else if (toupper(aKey)=='Q') {
    Application::sharedApplication()->terminateAppWith(WebError::webErr(500,"Simulation mode, user hit Q key"));
  }
  return true; // fully handled
}





void AyabComm::sendCommand(size_t aCmdLength, uint8_t *aCmdBytesP, StatusCB aStatusCB)
{
  if (simulated) {
    LOG(LOG_DEBUG,"Simulated sending of Command to AYAB, %lu bytes", aCmdLength+1);
    aStatusCB(ErrorPtr());
    return;
  }
  SerialOperationSendPtr sendOp = SerialOperationSendPtr(new SerialOperationSend);
  sendOp->setDataSize(aCmdLength);
  sendOp->appendData(aCmdLength, aCmdBytesP);
  // prepare response reading operation
  SerialOperationReceivePtr recOp = SerialOperationReceivePtr(new SerialOperationReceive);
  #if AYAB_SENDS_EXTRA_CRLF
  recOp->setExpectedBytes(4); // expected 4 response bytes
  #else
  recOp->setExpectedBytes(2); // expected 2 response bytes
  #endif
  recOp->inSequence = true;
  recOp->setTimeout(20*Second); // large timeout, because it can really take time until all expected answers are received
  recOp->setCompletionCallback(boost::bind(&AyabComm::ayabCmdResponseHandler, this, aStatusCB, recOp, _1));
  // chain response op
  sendOp->setChainedOperation(recOp);
  queueSerialOperation(sendOp);
  // process operations
  processOperations();
}


void AyabComm::sendResponse(size_t aRespLength, uint8_t *aRespBytesP)
{
  if (simulated) {
    LOG(LOG_DEBUG,"Simulated sending of response to AYAB, %lu bytes", aRespLength+1);
    return;
  }
  SerialOperationSendPtr sendOp = SerialOperationSendPtr(new SerialOperationSend);
  sendOp->setDataSize(aRespLength);
  sendOp->appendData(aRespLength, aRespBytesP);
  queueSerialOperation(sendOp);
  // process operations
  processOperations();
}




void AyabComm::ayabCmdResponseHandler(StatusCB aStatusCB, SerialOperationReceivePtr aRecOp, ErrorPtr aError)
{
  // get received data
  if (!aError && aRecOp->getDataSize()>=2) {
    uint8_t resp = aRecOp->getDataP()[0];
    if (resp==(AYABCMD_FROM_AYAB|AYABCMD_CONFIRM|AYABMSGID_INFO)) {
      // params: 0xaa 0xbb 0xcc - aa = API Version Identifier, bb = Firmware Major Version, cc = Firmware Minor Version
      uint8_t ver = aRecOp->getDataP()[1];
      uint8_t maj = aRecOp->getDataP()[2];
      uint8_t min = aRecOp->getDataP()[3];
      LOG(LOG_INFO, "AYAB API version: %d, Firmware Version %d.%d", ver, maj, min);
      if (ver!=AYAB_EXPECTED_FIRMWARE) {
        aError = TextError::err("AYAB reports firmware version %d, but we expect version %d", ver, AYAB_EXPECTED_FIRMWARE);
      }
    }
    else if (resp==(AYABCMD_FROM_AYAB|AYABCMD_CONFIRM|AYABMSGID_START)) {
      uint8_t sta = aRecOp->getDataP()[1];
      LOG(LOG_INFO, "AYAB start status: %d", sta);
      if (sta!=1) {
        aError = TextError::err("AYAB start command failed, AYAB status code = %d", sta);
      }
    }
    else {
      aError = TextError::err("AYAB invalid response for command: 0x%02X", resp);
    }
  }
  if (aStatusCB) {
    aStatusCB(aError);
  }
}




ssize_t AyabComm::acceptExtraBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // got bytes without having sent command before: must be Line request or state indication
  if (aBytes[0]=='#') {
    // Debug comment text line: text terminated by \r\n
    int idx=1;
    while (idx<aNumBytes-1) {
      if (aBytes[idx]=='\r') {
        string msg((char *)(aBytes+1), idx-1);
        LOG(LOG_INFO, "Debug message from AYAB: %s", msg.c_str());
        return idx+2; // including # and CRLF
      }
      idx++;
    }
    return NOT_ENOUGH_BYTES;
  }
  if (aBytes[0]==(AYABCMD_FROM_AYAB|AYABCMD_REQUEST|AYABMSGID_LINE)) {
    // must be 2 bytes at least
    if (aNumBytes<2) {
      return NOT_ENOUGH_BYTES;
    }
    // AYAB requests next line
    // params: 0xaa - aa = line number (Range: 0..255)
    LOG(LOG_INFO, "AYAB requests data for row #%d (overall count %d)", nextRequestRow, rowCount);
    uint8_t rowNo = aBytes[1];
    size_t consumed = 2;
    // swallow possible extra CRLF
    while (consumed<aNumBytes) {
      if (aBytes[consumed]!=0x0A && aBytes[consumed]!=0x0D) break; // not CR or LF, probably real data
      consumed++;
    }
    // process row request
    if (rowNo!=nextRequestRow) {
      LOG(LOG_ERR, "AYAB requests line #%d, we would have expected #%d", rowNo, nextRequestRow);
      nextRequestRow = rowNo;
    }
    // obtain and send next row
    sendNextRow();
    // the 2 request bytes are consumed
    return consumed;
  }
  else if (aBytes[0]==(AYABCMD_FROM_AYAB|AYABCMD_REQUEST|AYABMSGID_STATE)) {
    if (aNumBytes<8) {
      return NOT_ENOUGH_BYTES;
    }
    // 0x0a 0xBB 0xbb 0xCC 0xcc 0xdd 0xee
    // - a = ready (0 = false, 1 = true)
    // - BBbb = int left hall sensor value
    // - CCcc = int right hall sensor value
    // - dd = the carriage
    //   0 = no carriage detected
    //   1 = knit carriage “Strickschlitten”
    //   2 = hole carriage “Lochmusterschlitten”
    // - ee = the needle number currently in progress
    LOG(LOG_NOTICE,
      "AYAB indicates state:\n"
      "- ready: %d\n"
      "- left hall sensor: %d\n"
      "- right hall sensor: %d\n"
      "- carriage: %s\n"
      "- needle number in progress: %d",
      aBytes[1],
      (aBytes[2]<<8)+aBytes[3],
      (aBytes[4]<<8)+aBytes[5],
      aBytes[6]==0 ? "<none>" : (aBytes[6]==1 ? "Knit" : "Hole"),
      aBytes[7]
    );
    return 8; // consumed all
  }
  // consume all other data to re-sync
  LOG(LOG_DEBUG, "%zu extra bytes from AYAB discarded", aNumBytes);
  return (ssize_t)aNumBytes;
}


void AyabComm::sendNextRow()
{
  // call back to get row data
  AyabRowPtr row;
  if (rowCallBack) {
    row = rowCallBack(rowCount, ErrorPtr());
    rowCount++;
  }
  // send data or stop
  status = ayabstatus_knitting;
  // 0xaa 0xbb[24, 23, 22, ... 1, 0] 0xcc 0xdd
  // - aa = line number (Range: 0..255)
  // - bb[24 to 0] = binary pixel data
  // - cc = flags (bit 0: lastLine)
  // - dd = CRC8 Checksum
  const int rowresponselen = 29;
  uint8_t rowresponse[rowresponselen];
  memset(rowresponse,0,rowresponselen); // init to default
  rowresponse[0] = AYABCMD_CONFIRM|AYABCMD_FROM_HOST|AYABMSGID_LINE;
  rowresponse[1] = nextRequestRow; // answer for requested row
  if (row) {
    // we got a row to knit, fill in bits
    if (LOGENABLED(LOG_NOTICE)) {
      string rs;
      for (int i=0; i<row->rowSize; i++) {
        rs += row->rowData[i] ? 'X' : '.';
      }
      LOG(LOG_NOTICE,"Row No. %4d : %s", rowCount, rs.c_str());
    }
    // MSByte contains the first needle in bit0, the eigth needle in bit7, the ninth needle is bit0 in second byte, etc.
    for (int i=0; i<row->rowSize; i++) {
      // inverse direction
      if (row->rowData[row->rowSize-1-i]) {
        int j = i+firstNeedle;
        rowresponse[2+(j>>3)] |= (0x01 << (j & 0x07));
      }
    }
    // next
    nextRequestRow++;
  }
  else {
    LOG(LOG_NOTICE, "--- End of knitting job - total row count %d", rowCount);
    fullspeedsim = false; // end full speed simulation at end of job
    // no more rows, send empty one with lastline flag set
    rowresponse[rowresponselen-2] = 1; // lastline
    // back to ready
    status = ayabstatus_ready;
  }
  // calculate CRC8
  // TODO: once AYAB actually checks CRC, we might need to adjust range of checked bytes and start value here
  rowresponse[rowresponselen-1] = crc8(rowresponse, rowresponselen-1, 0); // for now: %%% CRC over entire message and start value 0
  // send it now
  sendResponse(rowresponselen, rowresponse);
  // in simulated full speed, just call again
  if (simulated && fullspeedsim) {
    MainLoop::currentMainLoop().executeOnce(boost::bind(&AyabComm::sendNextRow, this), 10*MilliSecond);
  }
}


void AyabComm::restart(SimpleCB aDoneCB)
{
  serialComm->setDTR(true); // arduino reset
  LOG(LOG_NOTICE, "restarting AYAB - DTR set active and waiting 3 seconds");
  status = ayabstatus_offline;
  MainLoop::currentMainLoop().executeOnce(boost::bind(&AyabComm::endReset, this, aDoneCB), 3*Second);
}


void AyabComm::endReset(SimpleCB aDoneCB)
{
  serialComm->setDTR(false); // arduino reset
  LOG(LOG_NOTICE, "restarting AYAB - DTR set inactive again and waiting 3 seconds");
  status = ayabstatus_connected;
  MainLoop::currentMainLoop().executeOnce(boost::bind(&AyabComm::restarted, this, aDoneCB), 3*Second);
}


void AyabComm::restarted(SimpleCB aDoneCB)
{
  LOG(LOG_NOTICE, "restarting AYAB done - should be ready in 3 seconds");
  if (aDoneCB) aDoneCB();
}




// Note:Machine is initialized when left hall sensor is passed in Right direction

bool AyabComm::startKnittingJob(unsigned aFirstNeedle, unsigned aWidth, AyabRowCB aRowCB)
{
  status = ayabstatus_ready;
  // store params
  if (!aRowCB || aWidth<2 || aFirstNeedle+aWidth>200) {
    return false; // invalid parameters
  }
  rowCallBack = aRowCB;
  firstNeedle = aFirstNeedle;
  width = aWidth;
  // check version first
  LOG(LOG_NOTICE, "+++ Start of knitting job - firstNeedle=%d, width=%d", firstNeedle, width);
  uint8_t cmd;
  cmd = AYABCMD_FROM_HOST|AYABCMD_REQUEST|AYABMSGID_INFO;
  sendCommand(1, &cmd, boost::bind(&AyabComm::ayabVersionResponseHandler, this, _1));
  return true; // launched job
}


void AyabComm::ayabVersionResponseHandler(ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    rowCallBack(0,aError);
    return;
  }
  // version is ok, now configure
  status = ayabstatus_ready;
  uint8_t cmd[3];
  cmd[0] = AYABCMD_FROM_HOST|AYABCMD_REQUEST|AYABMSGID_START;
  cmd[1] = firstNeedle; // first needle
  cmd[2] = firstNeedle+width-1; // last needle
  sendCommand(3, cmd, boost::bind(&AyabComm::ayabStartedResponseHandler, this, _1));
}


void AyabComm::ayabStartedResponseHandler(ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    rowCallBack(0,aError);
    return;
  }
  rowCount = 0;
  nextRequestRow = 0; // start at 0, will wrap around after 255 rows
  // now, rowCallBack will be called whenever the machine wants a new row
  status = ayabstatus_knitting;
}
