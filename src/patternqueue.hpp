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


#ifndef __p44ayabd__patternqueue__
#define __p44ayabd__patternqueue__

#include "p44utils_common.hpp"

#include "jsonobject.hpp"

#include "patterncontainer.hpp"


using namespace std;

namespace p44 {

  class PatternQueue;
  class PatternQueueEntry;

  typedef boost::intrusive_ptr<PatternQueue> PatternQueuePtr;
  typedef boost::intrusive_ptr<PatternQueueEntry> PatternQueueEntryPtr;



  class PatternQueueEntry : public P44Obj
  {
    typedef P44Obj inherited;
    friend class PatternQueue;

    PatternQueueEntry() : patternLength(0) {};

    string filepath;
    string weburl;
    int patternLength;
    PatternContainerPtr pattern;

  };


  typedef std::vector<PatternQueueEntryPtr> PatternQueueVector;


  class PatternQueue : public P44Obj
  {
    typedef P44Obj inherited;
    friend class PatternQueueEntry;

    bool stateDirty;

    // the queue
    PatternQueueVector queue;
    // the cursor
    int cursorEntry; ///< the entry where the cursor is currently in
    int cursorOffset; ///< the position within the entry
    // the width
    int patternWidth;


  public:

    PatternQueue();

    /// clear queue
    void clear();

    /// load the state
    void loadState(const char *aStateDir);

    /// save the current state
    void saveState(const char *aStateDir, bool aAnyWay);

    /// @return true if end of pattern reached (cursor at end of pattern queue)
    bool endOfPattern();

    /// @return position of cursor (units from beginning of pattern queue)
    int cursorPosition();

    /// @param index of image to get start position, -1 to get end of last image (= end of queue)
    /// @return returns start pixel pos of image relative to beginning of the queue
    int imageStartPos(int aImageIndex = -1);

    /// Move cursor to new row
    /// @param aNewPos relative or absolute new position
    /// @param aRelative if set, aNewPos is relative to the current cursor position
    /// @note default is to advance the cursor one position
    void moveCursor(int aNewPos = 1, bool aRelative = true, bool aBeginningOfEntry = false);

    /// @param aAtWidth which pixel of the row
    /// @return gray value from current cursor row
    uint8_t grayAtCursor(int aAtWidth);

    /// @return width of pattern queue
    int width() { return patternWidth; };

    /// @param set new width of queue
    ErrorPtr setWidth(int aWidth);

    /// get state as JSON
    JsonObjectPtr cursorStateJSON();
    JsonObjectPtr queueEntriesJSON();
    JsonObjectPtr queueStateJSON();

    /// add a file to the queue
    /// @param aFilePath the file system path to the file to add
    /// @param aWebURL the (possibly partial) Web URL for the file
    /// @return ok if the file could be loaded, error otherwise
    ErrorPtr addFile(string aFilePath, string aWebURL);

    /// remove a file from the queue
    /// @param aIndex queue index, 0...queue size-1
    /// @param aDeleteFile actually delete the file from the file system
    /// @return ok if the file could be removed, error otherwise
    ErrorPtr removeFile(int aIndex, bool aDeleteFile);


  private:

    void loadPatternAtCursor();

  };


} // namespace p44

#endif /* defined(__p44ayabd__patternqueue__) */
