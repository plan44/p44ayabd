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

#include "patternqueue.hpp"

using namespace p44;

#define QUEUE_STATE_FILE_NAME "p44ayabd_queuestate.json"

PatternQueue::PatternQueue() :
  stateDirty(false)
{
  clear();
}


void PatternQueue::clear()
{
  // clear queue
  queue.clear();
  cursorEntry = 0; // first
  cursorOffset = 0;
  patternWidth = 0; // none
}


ErrorPtr PatternQueue::setWidth(int aWidth)
{
  patternWidth = aWidth;
  stateDirty = true;
  return ErrorPtr();
}



ErrorPtr PatternQueue::addFile(string aFilePath, string aWebURL)
{
  PatternContainerPtr pattern = PatternContainerPtr(new PatternContainer);
  ErrorPtr err = pattern->readPNGfromFile(aFilePath.c_str());
  if (Error::isOK(err)) {
    // file could be read
    // - create queue entry
    PatternQueueEntryPtr qe = PatternQueueEntryPtr(new PatternQueueEntry);
    qe->filepath = aFilePath;
    qe->weburl = aWebURL;
    qe->pattern = pattern;
    qe->patternLength = pattern->length();
    // if queue was empty before, also set width
    if (queue.size()==0 && patternWidth==0) {
      patternWidth = pattern->width();
    }
    // - push into queue
    queue.push_back(qe);
    stateDirty = true; // new entry, state is dirty now
    // make sure image under cursor is loaded (and others are not)
    loadPatternAtCursor();
  }
  return err;
}


ErrorPtr PatternQueue::removeFile(int aIndex, bool aDeleteFile)
{
  if (aIndex>=queue.size()) {
    return WebError::webErr(500, "Invalid index");
  }
  // cannot remove file under cursor
  if (cursorEntry==aIndex && cursorOffset>0) {
    return WebError::webErr(500, "Cannot remove file under cursor");
  }
  // delete file if requested
  stateDirty = true;
  if (aDeleteFile) {
    unlink(queue[aIndex]->filepath.c_str());
  }
  // remove from queue
  queue.erase(queue.begin()+aIndex);
  if (aIndex<cursorEntry) {
    // removed something before current cursor -> adjust cursor to remain at same position within pattern
    cursorEntry--;
  }
  return ErrorPtr();
}




bool PatternQueue::endOfPattern()
{
  if (cursorEntry>=queue.size()) return true;
  PatternQueueEntryPtr qe = queue[cursorEntry];
  if (cursorOffset>=qe->patternLength) return true;
  // within pattern = not end of pattern
  return false;
}


int PatternQueue::cursorPosition()
{
  // calculated: absolute position of cursor within entire queue
  return (imageStartPos(cursorEntry)+cursorOffset);
}


int PatternQueue::imageStartPos(int aImageIndex)
{
  // calculated: absolute position of cursor within entire queue
  if (aImageIndex<0) aImageIndex = (int)queue.size(); // size of entire queue
  int pos = 0;
  for (int i=0; i<aImageIndex; i++) {
    pos += queue[i]->patternLength;
  }
  return pos;
}



void PatternQueue::moveCursor(int aNewPos, bool aRelative, bool aBeginningOfEntry)
{
  int oldCursor = cursorPosition();
  int newCursor = aRelative ? oldCursor+aNewPos : aNewPos;
  // not negative
  if (newCursor<0) newCursor=0;
  // calculate entry and offset
  if (newCursor!=oldCursor) {
    // needs recalculation
    stateDirty = true;
    cursorOffset = 0;
    cursorEntry = 0;
    // search image for cursor
    int pos = 0;
    while (cursorEntry<queue.size()) {
      PatternQueueEntryPtr qe = queue[cursorEntry];
      int newpos = pos+qe->patternLength;
      if (newCursor<newpos) {
        // this is the new queue element
        if (!aBeginningOfEntry)
          cursorOffset = newCursor-pos;
        break;
      }
      // cursor is higher
      pos = newpos;
      cursorEntry++;
    }
    // make sure image under cursor is loaded (and others are not)
    loadPatternAtCursor();
  }
  LOG(LOG_INFO,
    "Cursor moved from %d to %d, now entry = %d/%lu, offset = %d, endOfPattern = %s",
    oldCursor, newCursor,
    cursorEntry, queue.size(),
    cursorOffset,
    endOfPattern() ? "YES" : "no"
  );
}


uint8_t PatternQueue::grayAtCursor(int aAtWidth)
{
  if(cursorEntry>queue.size())
    return 0; // outside
  PatternQueueEntryPtr qe = queue[cursorEntry];
  if (!qe->pattern)
    loadPatternAtCursor();
  if (!qe->pattern)
    return 0; // safety
  return qe->pattern->grayAt(cursorOffset, aAtWidth);
}




void PatternQueue::loadPatternAtCursor()
{
  for (int i=0; i<queue.size(); ++i) {
    PatternQueueEntryPtr qe = queue[i];
    if (i==cursorEntry) {
      // current pattern, must be loaded
      if (!qe->pattern) {
        // load it
        qe->pattern = PatternContainerPtr(new PatternContainer);
        ErrorPtr err = qe->pattern->readPNGfromFile(qe->filepath.c_str());
      }
    }
    else {
      qe->pattern.reset();
    }
  }
}




#pragma mark - state load and save


void PatternQueue::loadState(const char *aStateDir)
{
  clear();
  stateDirty = true; // assume no correct state saved so far
  string statefile = aStateDir;
  statefile += "/" QUEUE_STATE_FILE_NAME;
  JsonObjectPtr s = JsonObject::objFromFile(statefile.c_str());
  clear();
  if (s) {
    JsonObjectPtr o;
    // the pattern width
    o = s->get("patternWidth");
    if (o) patternWidth = o->int32Value();
    // the cursor
    JsonObjectPtr cu = s->get("cursor");
    if (cu) {
      o = cu->get("entry");
      if (o) cursorEntry = o->int32Value();
      o = cu->get("offset");
      if (o) cursorOffset = o->int32Value();
    }
    // the entries of the queue
    JsonObjectPtr qes = s->get("queue");
    if (qes) {
      stateDirty = false; // having found queue signals state loaded
      for (int i=0; i<qes->arrayLength(); i++) {
        JsonObjectPtr qe = qes->arrayGet(i);
        if (qe) {
          PatternQueueEntryPtr entry = PatternQueueEntryPtr(new PatternQueueEntry);
          o = qe->get("filePath");
          if (o) entry->filepath = o->stringValue();
          o = qe->get("weburl");
          if (o) entry->weburl = o->stringValue();
          o = qe->get("patternLength");
          if (o) entry->patternLength = o->int32Value();
          // put to queue
          queue.push_back(entry);
        }
      }
    }
  }
}


void PatternQueue::saveState(const char *aStateDir, bool aAnyWay)
{
  if (stateDirty || aAnyWay) {
    string statefile = aStateDir;
    statefile += "/" QUEUE_STATE_FILE_NAME;
    // now save
    JsonObjectPtr s = queueStateJSON();
    s->saveToFile(statefile.c_str());
    stateDirty = false;
  }
}


#pragma mark - JSON

JsonObjectPtr PatternQueue::cursorStateJSON()
{
  JsonObjectPtr s = JsonObject::newObj();
  s->add("entry", JsonObject::newInt32(cursorEntry));
  s->add("offset", JsonObject::newInt32(cursorOffset));
  s->add("position", JsonObject::newInt32(cursorPosition()));
  s->add("endOfPattern", JsonObject::newBool(endOfPattern()));
  return s;
}


JsonObjectPtr PatternQueue::queueEntriesJSON()
{
  JsonObjectPtr qes = JsonObject::newArray();
  for (PatternQueueVector::iterator pos=queue.begin(); pos!=queue.end(); ++pos) {
    JsonObjectPtr qe = JsonObject::newObj();
    qe->add("filePath", JsonObject::newString((*pos)->filepath));
    qe->add("weburl", JsonObject::newString((*pos)->weburl));
    qe->add("patternLength", JsonObject::newInt32((*pos)->patternLength));
    qes->arrayAppend(qe);
  }
  return qes;
}


JsonObjectPtr PatternQueue::queueStateJSON()
{
  JsonObjectPtr s = JsonObject::newObj();
  s->add("queue",queueEntriesJSON());
  s->add("cursor",cursorStateJSON());
  s->add("patternWidth",JsonObject::newInt32(patternWidth));
  return s;
}




