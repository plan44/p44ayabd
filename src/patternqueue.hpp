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

    // the row phase (for ribber mode)
    int rowPhase;

    // the pattern
    int patternWidth; ///< pattern width
    int patternShift; ///< pattern offset (+up or -down) on a left-to-right knitted banner

    // ribber and colorchanger
    bool ribber; ///< if set: mode for ribber + color changer
    int numColors; ///< number of colors

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

    /// return the currently active color(s)
    /// @return bitmask with color0=bit0, color1=bit1 etc.
    uint8_t activeColors();

    /// check if pattern has the specified color number at a certain index
    /// @param aAtWith needle number where to sample color
    /// @return color number (0=background, 1..3=other colors)
    int colorNoAtCursor(int aAtWidth);

    /// get activation state of needle at cursor in current phase
    /// @param aAtWith needle number where to check status
    bool needleAtCursor(int aAtWidth);

    /// Start new phase, auto-increments cursor when new pattern row is needed for phase started with this call
    /// @return returns phase number
    int nextPhase();

    /// Restart phase cycle, do not change cursor position
    /// @return returns phase number
    void resetPhase();

    /// Move cursor to new row
    /// @param aNewPos relative or absolute new position
    /// @param aRelative if set, aNewPos is relative to the current cursor position
    /// @note default is to advance the cursor one position
    void moveCursor(int aNewPos = 1, bool aRelative = true, bool aBeginningOfEntry = false, bool aKeepPhase = false);

    /// @return width of pattern queue
    int width() { return patternWidth; };

    /// @return true if working with ribber
    bool ribberMode() { return ribber; };

    /// @return number of colors
    int colors() { return numColors; };

    /// set new width of queue
    /// @param aWidth new width
    ErrorPtr setWidth(int aWidth);

    /// set new baseline shift (+down, -up) of all patterns in the queue
    /// @param aShift new "baseline" shift
    ErrorPtr setShift(int aShift);

    /// @param set ribber mode
    ErrorPtr setRibberMode(bool aRibber);

    /// @param set number of colors
    ErrorPtr setColors(int aNumColors);

    /// get state as JSON
    JsonObjectPtr cursorStateJSON();
    JsonObjectPtr queueEntriesJSON();
    JsonObjectPtr queueStateJSON();

    /// add a file to the queue
    /// @param aFilePath the file system path to the file to add
    /// @param aWebURL the (possibly partial) Web URL for the file
    /// @return ok if the file could be loaded, error otherwise
    ErrorPtr addFile(string aFilePath, string aWebURL);

    /// add an amount of space to the queue
    /// @param aLength the length of the space
    /// @return ok if the file could be loaded, error otherwise
    ErrorPtr addSpace(int aLength);

    /// remove a segment from the queue
    /// @param aIndex queue index, 0...queue size-1
    /// @param aDeleteFile actually delete the file from the file system (if it is not space anyway)
    /// @return ok if the file could be removed, error otherwise
    ErrorPtr removeSegment(int aIndex, bool aDeleteFile);


  private:

    void loadPatternAtCursor();

  };


} // namespace p44

#endif /* defined(__p44ayabd__patternqueue__) */
