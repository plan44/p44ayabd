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

#include "p44_common.hpp"

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
    int cursorPosition; ///< the position within the entry


  public:

    PatternQueue();

    /// clear queue
    void clear();

    /// load the state
    void loadState(const char *aStateDir);

    /// save the current state
    void saveState(const char *aStateDir, bool aAnyWay);

    /// get state as JSON
    JsonObjectPtr cursorStateJSON();
    JsonObjectPtr queueEntriesJSON();
    JsonObjectPtr queueStateJSON();

    /// add a file to the queue
    ErrorPtr addFile(string aFilePath, string aWebURL);

  private:
    
  };


} // namespace p44

#endif /* defined(__p44ayabd__patternqueue__) */
