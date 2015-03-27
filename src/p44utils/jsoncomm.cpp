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

#include "jsoncomm.hpp"

using namespace p44;


JsonComm::JsonComm(MainLoop &aMainLoop) :
  inherited(aMainLoop),
  tokener(NULL),
  ignoreUntilNextEOM(false),
  closeWhenSent(false)
{
  setReceiveHandler(boost::bind(&JsonComm::gotData, this, _1));
}


JsonComm::~JsonComm()
{
  if (tokener) {
    json_tokener_free(tokener);
    tokener = NULL;
  }
}


void JsonComm::setMessageHandler(JSonMessageCB aJsonMessageHandler)
{
  jsonMessageHandler = aJsonMessageHandler;
}


void JsonComm::gotData(ErrorPtr aError)
{
  JsonCommPtr keepMeAlive(this); // make sure this object lives until routine terminates
  if (Error::isOK(aError)) {
    // no error, read data we've got so far
    size_t dataSz = numBytesReady();
    if (dataSz>0) {
      // temporary buffer
      uint8_t *buf = new uint8_t[dataSz];
      size_t receivedBytes = receiveBytes(dataSz, buf, aError);
      if (Error::isOK(aError)) {
        // check for end-of-message (LF), make spaces from any other ctrl char
        size_t bom = 0;
        while (bom<receivedBytes) {
          // data to process, scan for EOM
          size_t eom = bom;
          bool messageComplete = false;
          while (eom<receivedBytes) {
            if (buf[eom]<0x20) {
              if (buf[eom]=='\n') {
                // end of message
                buf[eom] = 0; // terminate message here
                messageComplete = true;
                break;
              }
              else {
                // other control char, convert to space
                buf[eom] = ' ';
              }
            }
            eom++;
          }
          // create tokener to parse message, if none found already
          if (!tokener) {
            tokener = json_tokener_new();
          }
          if (eom>0 && !ignoreUntilNextEOM) {
            // feed data to tokener
            struct json_object *o = json_tokener_parse_ex(tokener, (const char *)buf+bom, (int)(eom-bom));
            if (o==NULL) {
              // error (or incomplete JSON, which is fine)
              JsonErrors err = json_tokener_get_error(tokener);
              if (err!=json_tokener_continue) {
                // real error
                if (jsonMessageHandler) {
                  jsonMessageHandler(ErrorPtr(new JsonError(err)), JsonObjectPtr());
                }
                // reset the parser
                ignoreUntilNextEOM = true;
                json_tokener_reset(tokener);
              }
            }
            else {
              // got JSON object
              JsonObjectPtr message = JsonObject::newObj(o);
              if (jsonMessageHandler) {
                // pass json_object into handler, will consume it
                jsonMessageHandler(ErrorPtr(), message);
              }
              ignoreUntilNextEOM = true;
              json_tokener_reset(tokener);
            }
          }
          // now check for having reached the end of the message in this data chunk
          if (messageComplete) {
            // new message starts, don't ignore any more
            ignoreUntilNextEOM = false;
            // skip any control chars
            while (eom<receivedBytes && buf[eom]<0x20) eom++;
          }
          // now eom becomes the new bom
          bom = eom;
        } // while data to process
      } // no read error
      delete[] buf; buf = NULL;
    } // some data seems to be ready
  } // no connection error
  if (!Error::isOK(aError)) {
    // error occurred, report
    if (jsonMessageHandler) {
      jsonMessageHandler(aError, JsonObjectPtr());
    }
    ignoreUntilNextEOM = false;
    if (tokener) json_tokener_reset(tokener);
  }
}


ErrorPtr JsonComm::sendMessage(JsonObjectPtr aJsonObject)
{
  string json_string = aJsonObject->json_c_str();
  json_string.append("\n");
  return sendRaw(json_string);
}


ErrorPtr JsonComm::sendRaw(string &aRawBytes)
{
  ErrorPtr err;
  if (transmitBuffer.size()>0) {
    // other messages are already waiting, append entire message
    transmitBuffer.append(aRawBytes);
  }
  else {
    size_t rawSize = aRawBytes.size();
    // nothing in buffer yet, start new send
    size_t sentBytes = transmitBytes(rawSize, (uint8_t *)aRawBytes.c_str(), err);
    if (Error::isOK(err)) {
      // check if all could be sent
      if (sentBytes<rawSize) {
        // Not everything (or maybe nothing, transmitBytes() can return 0) was sent
        // - enable callback for ready-for-send
        setTransmitHandler(boost::bind(&JsonComm::canSendData, this, _1));
        // buffer the rest, canSendData handler will take care of writing it out
        transmitBuffer.assign(aRawBytes.c_str()+sentBytes, rawSize-sentBytes);
      }
			else {
				// all sent
				// - disable transmit handler
        setTransmitHandler(NULL);
			}
    }
  }
  return err;
}


void JsonComm::closeAfterSend()
{
  if (transmitBuffer.size()==0) {
    // nothing buffered for later, close now
    closeConnection();
  }
  else {
    closeWhenSent = true;
  }
}




void JsonComm::canSendData(ErrorPtr aError)
{
  size_t bytesToSend = transmitBuffer.size();
  if (bytesToSend>0 && Error::isOK(aError)) {
    // send data from transmit buffer
    size_t sentBytes = transmitBytes(bytesToSend, (const uint8_t *)transmitBuffer.c_str(), aError);
    if (Error::isOK(aError)) {
      if (sentBytes==bytesToSend) {
        // all sent
        transmitBuffer.erase();
				// - disable transmit handler
        setTransmitHandler(NULL);
      }
      else {
        // partially sent, remove sent bytes
        transmitBuffer.erase(0, sentBytes);
      }
      // check for closing connection when no data pending to be sent any more
      if (closeWhenSent && transmitBuffer.size()==0) {
        closeWhenSent = false; // done
        closeConnection();
      }
    }
  }
}

