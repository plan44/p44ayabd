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

#ifndef __p44utils__mainloop__
#define __p44utils__mainloop__

#include "p44_common.hpp"

#include <sys/poll.h>
#include <pthread.h>

// if set to non-zero, mainloop will have some code to record statistics
#define MAINLOOP_STATISTICS 1

using namespace std;

namespace p44 {

  class MainLoop;
  class ChildThreadWrapper;

  typedef boost::intrusive_ptr<MainLoop> MainLoopPtr;
  typedef boost::intrusive_ptr<ChildThreadWrapper> ChildThreadWrapperPtr;

  // Mainloop timing unit
  typedef long long MLMicroSeconds;
  const MLMicroSeconds Never = 0;
  const MLMicroSeconds Infinite = -1;
  const MLMicroSeconds MicroSecond = 1;
  const MLMicroSeconds MilliSecond = 1000;
  const MLMicroSeconds Second = 1000*MilliSecond;
  const MLMicroSeconds Minute = 60*Second;


  /// subthread/maintthread communication signals (sent via pipe)
  typedef enum {
    threadSignalNone,
    threadSignalCompleted, ///< sent to parent when child thread terminates
    threadSignalFailedToStart, ///< sent to parent when child thread could not start
    threadSignalCancelled, ///< sent to parent when child thread was cancelled (
    threadSignalUserSignal ///< first user-specified signal
  } ThreadSignals;


  /// @name Mainloop callbacks
  /// @{

  /// Generic handler without any arguments
  typedef boost::function<void ()> SimpleCB;

  /// Generic handler or returning a status (ok or error)
  typedef boost::function<void (ErrorPtr aError)> StatusCB;

  /// Handler for idle time processing (called when other mainloop tasks are done)
  /// @return true if idle handler has completed for this mainloop cycle and does not need more execution time in this cycle.
  typedef boost::function<bool (MLMicroSeconds aCycleStartTime)> IdleCB;

  /// Handler for one time processing (scheduled by executeOnce()/executeOnceAt())
  typedef boost::function<void (MLMicroSeconds aCycleStartTime)> OneTimeCB;

  /// Handler for getting signalled when child process terminates
  /// @param aPid the PID of the process that has terminated
  /// @param aStatus the exit status of the process that has terminated
  typedef boost::function<void (MLMicroSeconds aCycleStartTime, pid_t aPid, int aStatus)> WaitCB;

  /// Handler called when fork_and_execve() or fork_and_system() terminate
  /// @param aOutputString the stdout output of the executed command
  typedef boost::function<void (MLMicroSeconds aCycleStartTime, ErrorPtr aError, const string &aOutputString)> ExecCB;

  /// I/O callback
  /// @param aMainLoop the mainloop which calls this handler
  /// @param aCycleStartTime the time when the current mainloop cycle has started
  /// @param aFD the file descriptor that was signalled and has caused this call
  /// @param aPollFlags the poll flags describing the reason for the callback
  /// @return should true if callback really handled some I/O, false if it only checked flags and found nothing to do
  typedef boost::function<bool (MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)> IOPollCB;

  /// thread routine, will be called on a separate thread
  /// @param aThreadWrapper the object that wraps the thread and allows sending signals to the parent thread
  ///   Use this pointer to call signalParentThread() on
  /// @note when this routine exits, a threadSignalCompleted will be sent to the parent thread
  typedef boost::function<void (ChildThreadWrapper &aThread)> ThreadRoutine;

  /// thread signal handler, will be called from main loop of parent thread when child thread uses signalParentThread()
  /// @param aMainLoop the mainloop of the parent thread which has started the child thread
  /// @param aChildThread the ChildThreadWrapper object which sent the signal
  /// @param aSignalCode the signal received from the child thread
  typedef boost::function<void (ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode)> ThreadSignalHandler;

  /// @}


  /// subprocess execution error
  class ExecError : public Error
  {
  public:
    static const char *domain() { return "ExecError"; };
    virtual const char *getErrorDomain() const { return ExecError::domain(); };
    ExecError(int aExitStatus) : Error(ErrorCode(aExitStatus)) {};
    ExecError(int aExitStatus, std::string aErrorMessage) : Error(ErrorCode(aExitStatus), aErrorMessage) {};
    static ErrorPtr exitStatus(int aExitStatus, const char *aContextMessage = NULL);
  };


  class MainLoop;

  class FdStringCollector;
  typedef boost::intrusive_ptr<FdStringCollector> FdStringCollectorPtr;

  /// A main loop for a thread
  class MainLoop : public P44Obj
  {
    friend class ChildThreadWrapper;

    typedef struct {
      void *subscriberP;
      IdleCB callback;
    } IdleHandler;
    typedef std::list<IdleHandler> IdleHandlerList;

    IdleHandlerList idleHandlers;
    bool idleHandlersChanged;

    typedef struct {
      long ticketNo;
      MLMicroSeconds executionTime;
      OneTimeCB callback;
    } OnetimeHandler;
    typedef std::list<OnetimeHandler> OnetimeHandlerList;

    OnetimeHandlerList onetimeHandlers;
    bool oneTimeHandlersChanged;

    typedef struct {
      pid_t pid;
      WaitCB callback;
    } WaitHandler;
    typedef std::map<pid_t, WaitHandler> WaitHandlerMap;

    WaitHandlerMap waitHandlers;

    typedef struct {
      int monitoredFD;
      int pollFlags;
      IOPollCB pollHandler;
    } IOPollHandler;
    typedef std::map<int, IOPollHandler> IOPollHandlerMap;

    IOPollHandlerMap ioPollHandlers;

    long ticketNo;

  protected:

    bool terminated;
    int exitCode;

    MLMicroSeconds loopCycleTime;
    MLMicroSeconds cycleStartTime;

    #if MAINLOOP_STATISTICS
    MLMicroSeconds statisticsStartTime;
    long statisticsCycles;
    size_t maxOneTimeHandlers;
    MLMicroSeconds ioHandlerTime;
    MLMicroSeconds idleHandlerTime;
    MLMicroSeconds oneTimeHandlerTime;
    MLMicroSeconds waitHandlerTime;
    MLMicroSeconds threadSignalHandlerTime;
    #endif


    // protected constructor
    MainLoop();

  public:

    /// returns or creates the current thread's mainloop
    static MainLoop &currentMainLoop();

    /// returns the current microsecond
    static MLMicroSeconds now();

    /// set the cycle time
    void setLoopCycleTime(MLMicroSeconds aCycleTime);

    /// get time left for current cycle
    MLMicroSeconds remainingCycleTime();

    /// @name register handlers for idle time (once per mainloop cycle, when all other handlers have been called)
    /// @{

    /// register routine with mainloop for being called at least once per loop cycle
    /// @param aSubscriberP usually "this" of the caller, or another unique memory address which allows unregistering later
    /// @param aCallback the functor to be called
    void registerIdleHandler(void *aSubscriberP, IdleCB aCallback);

    /// unregister all handlers registered by a given subscriber
    /// @param aSubscriberP a value identifying the subscriber
    void unregisterIdleHandlers(void *aSubscriberP);

    /// @}


    /// @name register one-time handlers (fired at specified time)
    /// @{

    /// have handler called from the mainloop once with an optional delay from now
    /// @param aCallback the functor to be called
    /// @param aExecutionTime when to execute (approximately), in now() timescale
    /// @return ticket number which can be used to cancel this specific execution request
    long executeOnceAt(OneTimeCB aCallback, MLMicroSeconds aExecutionTime);

    /// have handler called from the mainloop once with an optional delay from now
    /// @param aCallback the functor to be called
    /// @param aDelay delay from now when to execute (approximately)
    /// @return ticket number which can be used to cancel this specific execution request
    long executeOnce(OneTimeCB aCallback, MLMicroSeconds aDelay = 0);

    /// cancel pending execution by ticket number
    /// @param aTicketNo ticket of execution to cancel. Will be set to 0 on return
    void cancelExecutionTicket(long &aTicketNo);

    /// reschedule existing execution request
    /// @param aTicketNo ticket of execution to reschedule.
    /// @param aDelay delay from now when to reschedule execution (approximately)
    /// @return true if the execution specified with aTicketNo was still pending and could be rescheduled
    bool rescheduleExecutionTicket(long aTicketNo, MLMicroSeconds aDelay);

    /// reschedule existing execution request
    /// @param aTicketNo ticket of execution to reschedule.
    /// @param aExecutionTime to when to reschedule execution (approximately), in now() timescale
    /// @return true if the execution specified with aTicketNo was still pending and could be rescheduled
    bool rescheduleExecutionTicketAt(long aTicketNo, MLMicroSeconds aExecutionTime);

    /// @}


    /// @name start subprocesses and register handlers for returning subprocess status
    /// @{

    /// execute external binary or interpreter script in a separate process
    /// @param aCallback the functor to be called when execution is done (failed to start or completed)
    /// @param aPath the path to the binary or script
    /// @param aArgv a NULL terminated array of arguments, first should be program name
    /// @param aEnvp a NULL terminated array of environment variables, or NULL to let child inherit parent's environment
    /// @param aPipeBackStdOut if true, stdout of the child is collected via a pipe by the parent and passed back in aCallBack
    void fork_and_execve(ExecCB aCallback, const char *aPath, char *const aArgv[], char *const aEnvp[] = NULL, bool aPipeBackStdOut = false);

    /// execute command line in external shell
    /// @param aCallback the functor to be called when execution is done (failed to start or completed)
    /// @param aCommandLine the command line to execute
    /// @param aPipeBackStdOut if true, stdout of the child is collected via a pipe by the parent and passed back in aCallBack
    void fork_and_system(ExecCB aCallback, const char *aCommandLine, bool aPipeBackStdOut = false);


    /// have handler called when a specific process delivers a state change
    /// @param aCallback the functor to be called when given process delivers a state change, NULL to remove callback
    /// @param aPid the process to wait for
    void waitForPid(WaitCB aCallback, pid_t aPid);

    /// @}


    /// @name register handlers for I/O events
    /// @{

    /// register handler to be called for activity on specified file descriptor
    /// @param aFD the file descriptor to poll
    /// @param aPollFlags POLLxxx flags to specify events we want a callback for
    /// @param aFdEventCB the functor to be called when poll() reports an event for one of the flags set in aPollFlags
    void registerPollHandler(int aFD, int aPollFlags, IOPollCB aPollEventHandler);

    /// change the poll flags for an already registered handler
    /// @param aFD the file descriptor
    /// @param aPollFlags POLLxxx flags to specify events we want a callback for
    void changePollFlags(int aFD, int aSetPollFlags, int aClearPollFlags=-1);

    /// unregister all handlers registered by a given subscriber
    /// @param aSubscriberP a value identifying the subscriber
    /// @param aFD the file descriptor
    void unregisterPollHandler(int aFD);
    
    /// @}


    /// @name run handler in separate thread
    /// @{

    /// execute handler in a separate thread
    /// @param aThreadRoutine the routine to be executed in a separate thread
    /// @param aThreadSignalHandler will be called from main loop of parent thread when child thread uses signalParentThread()
    /// @return wrapper object for child thread.
    ChildThreadWrapperPtr executeInThread(ThreadRoutine aThreadRoutine, ThreadSignalHandler aThreadSignalHandler);

    /// @}


    /// terminate the mainloop
    /// @param aExitCode the code to return from run()
    void terminate(int aExitCode);

    /// run the mainloop
    /// @return returns a exit code
    int run();

    /// description (shows some mainloop key numbers)
    string description();

    /// reset statistics
    void statistics_reset();


  protected:

    bool runOnetimeHandlers();
    long scheduleOneTimeHandler(OnetimeHandler &aHandler);
    bool runIdleHandlers();
    bool checkWait();
    bool handleIOPoll(MLMicroSeconds aTimeout);

  private:

    void execChildTerminated(ExecCB aCallback, FdStringCollectorPtr aAnswerCollector, pid_t aPid, int aStatus);
    void childAnswerCollected(ExecCB aCallback, FdStringCollectorPtr aAnswerCollector, ErrorPtr aError);
    void IOPollHandlerForFd(int aFD, IOPollHandler &h);

  };



  class ChildThreadWrapper : public P44Obj
  {
    typedef P44Obj inherited;

    pthread_t pthread; ///< the pthread
    bool threadRunning; ///< set if thread is active

    MainLoop &parentThreadMainLoop; ///< the parent mainloop which created this thread
    int childSignalFd; ///< the pipe used to transmit signals from the child thread
    int parentSignalFd; ///< the pipe monitored by parentThreadMainLoop to get signals from child

    ThreadSignalHandler parentSignalHandler; ///< the handler to call to deliver signals to the main thread
    ThreadRoutine threadRoutine; ///< the actual thread routine to run

    ChildThreadWrapperPtr selfRef;

  public:

    /// constructor
    ChildThreadWrapper(MainLoop &aParentThreadMainLoop, ThreadRoutine aThreadRoutine, ThreadSignalHandler aThreadSignalHandler);

    /// destructor
    virtual ~ChildThreadWrapper();

    /// @name methods to call from child thread
    /// @{

    /// signal parent thread
    /// @param aSignalCode a signal code to be sent to the parent thread
    void signalParentThread(ThreadSignals aSignalCode);

    /// @}


    /// @name methods to call from parent thread
    /// @{

    /// cancel execution and wait for cancellation to complete
    void cancel();

    /// confirm termination
    void terminated();

    /// @}

    /// method called from thread_start_function from this child thread
    void *startFunction();

  private:

    bool signalPipeHandler(int aPollFlags);
    void finalizeThreadExecution();

  };


} // namespace p44

#endif /* defined(__p44utils__mainloop__) */
