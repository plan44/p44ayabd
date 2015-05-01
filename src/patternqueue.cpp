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
  cursorPosition = 0;
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
    // - push into queue
    queue.push_back(qe);
    stateDirty = true; // new entry, state is dirty now
    // - unload image itself if this is not the current image
    if (cursorEntry!=queue.size()-1) {
      // cursor is not in the newly added entry
      qe->pattern.reset(); // we have the length, we don't need the pattern until the cursor is in it
    }
  }
  return err;
}




void PatternQueue::loadState(const char *aStateDir)
{
  stateDirty = true; // assume no correct state saved so far
  string statefile = aStateDir;
  statefile += "/" QUEUE_STATE_FILE_NAME;
  JsonObjectPtr s = JsonObject::objFromFile(statefile.c_str());
  clear();
  if (s) {
    // the cursor
    JsonObjectPtr cu = s->get("cursor");
    JsonObjectPtr o;
    if (cu) {
      o = cu->get("entry");
      if (o) cursorEntry = o->int32Value();
      o = cu->get("offset");
      if (o) cursorPosition = o->int32Value();
    }
    // the entries of the queue
    JsonObjectPtr qes = s->get("queue");
    if (qes) {
      stateDirty = false; // having found queue signals state loaded
      for (int i=0; i<qes->arrayLength(); i++) {
        JsonObjectPtr qe = qes->arrayGet(i);
        if (qe) {
          PatternQueueEntryPtr entry = PatternQueueEntryPtr(new PatternQueueEntry);
          o = qe->get("filename");
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


JsonObjectPtr PatternQueue::cursorStateJSON()
{
  JsonObjectPtr s = JsonObject::newObj();
  s->add("entry", JsonObject::newInt32(cursorEntry));
  s->add("offset", JsonObject::newInt32(cursorPosition));
  // calculated: absolute position of cursor within entire queue
  int pos = 0;
  for (int i=0; i<cursorEntry; i++) {
    pos += queue[i]->patternLength;
  }
  s->add("position", JsonObject::newInt32(pos+cursorPosition));
  return s;
}


JsonObjectPtr PatternQueue::queueEntriesJSON()
{
  JsonObjectPtr qes = JsonObject::newArray();
  for (PatternQueueVector::iterator pos=queue.begin(); pos!=queue.end(); ++pos) {
    JsonObjectPtr qe = JsonObject::newObj();
    qe->add("filepath", JsonObject::newString((*pos)->filepath));
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
  return s;
}




