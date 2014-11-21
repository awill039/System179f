#include <thread>
#include <pthread.h>
#include <semaphore.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include <map>
#include <queue>
#include <iomanip>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <climits>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <vector>
#include <string.h>
using namespace std;

template< typename T >
inline string T2a( T x ) { ostringstream s; s<<x; return s.str(); }
template< typename T >
//inline string id( T x ) { return T2a( (unsigned long) x ); }
inline string id( T x ) { return T2a( x ); }

bool CDBG_IS_ON = false;	 // false - Turns off CDBG output ;  true - Turns on CDBG input
bool PRINT_QUEUE_ON = false;
#define cdbg if(CDBG_IS_ON) cerr << "\nLn " << __LINE__ << " of " << setw(8) << __FUNCTION__ << " by " << report() 

#define EXCLUSION Sentry exclusion(this); exclusion.touch();
class Thread;
extern string Him(Thread*);
extern string Me();
extern string report( );  


// ====================== priority queue ============================

// pQueue behaves exactly like STL's queue, except that push has an
// optional integer priority argument that defaults to INT_MAX.
// front() returns a reference to the item with highest priority
// (i.e., smallest priority number) with ties broken on a FIFO basis.
// It may overflow and malfunction after INT_MAX pushes.

template<class T>                // needed for priority comparisions
bool operator<( const pair<pair<int,int>,T>& a, 
                const pair<pair<int,int>,T>& b  
              ) {
  return a.first.first > b.first.first ? true  :
         a.first.first < b.first.first ? false :
               a.first.second > b.first.second ;
} 

template <class T>
class pQueue {
  priority_queue< pair<pair<int,int>,T> > q;  
  int i;  // counts invocations of push to break ties on FIFO basis.
public:
  pQueue() : i(0) {;}
  void push( T t, int n = INT_MAX ) { 
    q.push( pair<pair<int,int>,T>(pair<int,int>(n,++i),t) ); 
  }
  void pop() { q.pop(); }
  T front() { return q.top().second; }
  int size() { return q.size(); }
  bool empty() { return q.empty(); }
};

// =========================== interrupts ======================

class InterruptSystem {
public:       // man sigsetops for details on signal operations.
  static void handler(int sig);
  // exported pseudo constants
  static sigset_t on;                    
  static sigset_t alrmoff;    
  static sigset_t alloff;     
  InterruptSystem() {  
    signal( SIGALRM, InterruptSystem::handler ); 
    sigemptyset( &on );                    // none gets blocked.
    sigfillset( &alloff );                  // all gets blocked.
    sigdelset( &alloff, SIGINT );
    sigemptyset( &alrmoff );      
    sigaddset( &alrmoff, SIGALRM ); //only SIGALRM gets blocked.
    set( alloff );        // the set() service is defined below.
    struct itimerval time;
    time.it_interval.tv_sec  = 0;
    time.it_interval.tv_usec = 400000;
    time.it_value.tv_sec     = 0;
    time.it_value.tv_usec    = 400000;
    //cerr << "\nstarting timer\n";
    setitimer(ITIMER_REAL, &time, NULL);
  }
  sigset_t set( sigset_t mask ) {
    sigset_t oldstatus;
    pthread_sigmask( SIG_SETMASK, &mask, &oldstatus );
    return oldstatus;
  } // sets signal/interrupt blockage and returns former status.
  sigset_t block( sigset_t mask ) {
    sigset_t oldstatus;
    pthread_sigmask( SIG_BLOCK, &mask, &oldstatus );
    return oldstatus;
  } //like set but leaves blocked those signals already blocked.
};

// signal mask pseudo constants
sigset_t InterruptSystem::on;   
sigset_t InterruptSystem::alrmoff; 
sigset_t InterruptSystem::alloff;  

InterruptSystem interrupts;               // singleton instance.


// ========= Threads, Semaphores, Sentry's, Monitors ===========

// class Semaphore {           // C++ wrapper for Posix semaphores.
//   sem_t s;                               // the Posix semaphore.
// public:
//   Semaphore( int n = 0 ) { assert( ! sem_init(&s, 0, n) ); }
//   ~Semaphore()           { assert( ! sem_destroy(&s) );    }
//   void release()         { assert( ! sem_post(&s) );       }
//   void acquire() {
//     sigset_t old = interrupts.set( InterruptSystem::alloff );
//     sem_wait( &s );
//     interrupts.set( old );
//   }
// };


class Semaphore {
private:
    mutex mtx;
    condition_variable available;
    int count;

public:
    Semaphore(int count = 0):count(count){;}
    void release() {
        unique_lock<mutex> lck(mtx);
        ++count;
        available.notify_one();
    }
    void acquire() {
        unique_lock<mutex> lck(mtx);
        while(count == 0) available.wait(lck);
        count--;
    }
};


class Lock : public Semaphore {
public:             // A Semaphore initialized to one is a Lock.
  Lock() : Semaphore(1) {} 
};


class Monitor : Lock {
  friend class Sentry;
  friend class Condition;
  sigset_t mask;
public:
  void lock()   { Lock::acquire(); }
  void unlock() { Lock::release(); }
  Monitor( sigset_t mask = InterruptSystem::alrmoff ) 
    : mask(mask) 
  {}
};


class Sentry {            // An autoreleaser for monitor's lock.
  Monitor&  mon;           // Reference to local monitor, *this.
  const sigset_t old;    // Place to store old interrupt status.
public:
  void touch() {}          // To avoid unused-variable warnings.
  Sentry( Monitor* m ) 
    : mon( *m ), 
      old( interrupts.block(mon.mask) )
  { 
    mon.lock(); 
  }
  ~Sentry() {
    mon.unlock();
    interrupts.set( old ); 
  }
};

template< typename T1, typename T2 >
class ThreadSafeMap : Monitor {
  map<T1,T2> m;
public:
  T2& operator [] ( T1 x ) {
    EXCLUSION
    return m[x];
  }
};


class Thread {
  friend class Condition;
  friend class CPUallocator;                      // NOTE: added.
  //pthread_t pt;                                    // pthread ID.
  thread pt;                                  // C++14 thread.
  Thread* parent_thread;
  static void* start( Thread* );
  virtual void action() = 0;
  Semaphore go;
  static ThreadSafeMap<thread::id,Thread*> whoami;  
  int pri;

  void suspend() { 
    cdbg << "Suspending thread \n";
    go.acquire(); 
    cdbg << "Unsuspended thread \n";
  }

  void resume() { 
    cdbg << "Resuming \n";
    go.release(); 
  }

  //int self() { return pthread_self(); }
  thread::id self() { return this_thread::get_id(); }

  

public:

  string name; 
  thread::id thread_id;
  static Thread* me();

  virtual ~Thread() { 
    //pthread_cancel(pt);
  }

  Thread( string name = "", int priority = INT_MAX ) 
    : name(name), pri(priority), parent_thread(me())
  {
    //cerr << "\ncreating thread " << Him(this) << endl;
    //assert( ! pthread_create(&pt,NULL,(void*(*)(void*))start,this));
    pt = thread((void*(*)(void*))start,this);
  }
  
  virtual int priority() { 
    return pri;      // place holder for complex CPU policies.
  }   
  
  //void join() { assert( pthread_join( pt, null); ) }
  void join() { 
    //assert( pt.joinable() );
    pt.thread::join();
  }
  thread::id get_thread_id()
  {
	  return thread_id;
  }

};


class Condition : pQueue< Thread* > {
  Monitor&  mon;                     // reference to local monitor
public:
  Condition( Monitor* m ) : mon( *m ) {;}
  int waiting() { return size(); }
  bool awaited() { return waiting() > 0; }
  void wait( int pr = INT_MAX );    // wait() is defined after CPU
  void signal() { 
    if ( awaited() ) {
      Thread* t = front();
      pop();
      cdbg << "Signalling" << endl;
      t->resume();
    }
  }
  void broadcast() { while ( awaited() ) signal(); }
}; 


// ====================== AlarmClock ===========================

class AlarmClock : Monitor {
  unsigned long now;
  unsigned long alarm;
  Condition wakeup;
public:
  AlarmClock() 
    : now(0),
      alarm(INT_MAX),
      wakeup(this)
  {;}
  int gettime() { EXCLUSION return now; }
  void wakeme_at(int myTime) {
    EXCLUSION
    if ( now >= myTime ) return;  // don't wait
    if ( myTime < alarm ) alarm = myTime;      // alarm min= myTime
    while ( now < myTime ) {
      cdbg << " ->wakeup wait " << endl;
      wakeup.wait(myTime);
      cdbg << " wakeup-> " << endl;
      if ( alarm < myTime ) alarm = myTime;
    }
    alarm = INT_MAX;
    wakeup.signal();
  }
  void wakeme_in(int time) { wakeme_at(now+time); }
  void tick() { 
    EXCLUSION
    cdbg << "now=" << now << " alarm=" << alarm
         << " sleepers=" << wakeup.waiting() << endl;
    if ( now++ >= alarm ) wakeup.signal();
  }
};

extern AlarmClock dispatcher;                  // singleton instance.


/*
class Idler : Thread {                       // awakens periodically.
  // Idlers wake up periodically and then go back to sleep.
  string name;
  int period;
  int priority () { return 0; }                  // highest priority.
  void action() { 
    cdbg << "Idler running\n";
    int n = 0;
    for(;;) { 
      cdbg << "calling wakeme_at\n";
      dispatcher.wakeme_at( ++n * period ); 
    } 
  }
public:
  Idler( string name, int period = 5 )    // period defaults to five.
    : Thread(name), period(period)
  {}
};

*/

//==================== CPU-related stuff ========================== 


// The following comment is left over from an old assignment, but is
// has some useful comments embedded in it.  (10/7/2014)

// /*
// class CPUallocator : Monitor {                            
//   // a scheduler for the pool of CPUs or anything else.  (In reality
//   // we are imlementing a prioritized semaphore.)

//   friend string report();
//   pQueue<Thread*> ready;       // queue of Threads waiting for a CPU.
//   int cpu_count;           // count of unallocated (i.e., free) CPUs.

// public:

//   CPUallocator( int n ) : cpu_count(n) {;}

//   void release()  {
//     EXCLUSION
//     // Return your host to the pool by incrementing cpu_count.
//     // Awaken the highest priority sleeper on the ready queue.

//     // Fill in your code.

//   }

//   void acquire( int pr = Thread::me()->priority() ) {
//     EXCLUSION

//     // While the cpu_count is zero, repeatedly, put yourself to 
//     // sleep on the ready queue at priority pr.  When you finally 
//     // find the cpu_count greater than zero, decrement it and 
//     // proceed.
//     // 
//     // Always unlock the monitor the surrounding Monitor before 
//     // going to sleep and relock it upon awakening.  There's no
//     // need to restore the preemption mask upon going to sleep; 
//     // this host's preemption mask will automatically get reset to 
//     // that of its next guest.

//     // Fill in your code here.

//   }

//   void defer( int pr = Thread::me()->priority() ) {
//     release(); 
//     acquire(pr); 
//     // This code works but is a prototype for testing purposes only:
//     //  * It unlocks the monitor and restores preemptive services
//     //    at the end of release, only to undo that at the beginning 
//     //    of acquire.
//     //  * It gives up the CPU even if this thread has the higher 
//     //    priority than any of the threads waiting on ready.
//   }

//   void defer( int pr = Thread::me()->priority() ) {
//     EXCLUSION
//     if ( ready.empty() ) return;
//     //  Put yourself onto the ready queue at priority pr.  Then,
//     //  extract the highest priority thread from the ready queue.  If
//     //  that thread is you, return.  Otherwise, put yourself to sleep.
//     //  Then proceed as in acquire(): While the cpu_count is zero,
//     //  repeatedly, put yourself to sleep on the ready queue at
//     //  priority pr.  When you finally find the cpu_count greater than
//     //  zero, decrement it and proceed.

//     // Fill in your code.

//   }

// };
// */


// class CPUallocator {
//   // a scheduler for the pool of CPUs.
//   Semaphore sem;         // a semaphore appearing as a pool of CPUs.
// public:
//   int cpu_count;  // not used here and set to visibly invalid value.
//   pQueue<Thread*> ready;
//   CPUallocator( int count ) : sem(count), cpu_count(-1) {}
//   void defer() { sem.release(); sem.acquire(); }
//   void acquire( int pr = INT_MAX ) { sem.acquire(); }
//   void release() { sem.release(); }
// };


// This is my current CPUallocator (thp 10/8/2014)
class CPUallocator : Monitor {                            

  // a prioritized scheduler for the pool of CPUs or anything else.
  friend string report();
  pQueue<Thread*> ready;       // queue of Threads waiting for a CPU.
  int cpu_count;           // count of unallocated (i.e., free) CPUs.

public:

  CPUallocator( int n ) : cpu_count(n) {;}

  void release() {
    // called from Conditon::wait()
    EXCLUSION
    ++ cpu_count;                     // return this CPU to the pool.
    assert( cpu_count > 0 );
    if ( ready.empty() ) return;          // caller will now suspend.
    Thread* t = ready.front();
    ready.pop();
    t->resume();
  }

  void acquire( int pr = Thread::me()->priority() ) {
    EXCLUSION
    while ( cpu_count == 0 ) { // sleep, waiting for a nonempty pool.
      ready.push( Thread::me(), pr );
      unlock();            // release the lock before going to sleep.
      // The host's next guest will reset you host's preemption mask.
      Thread::me()->suspend();
      lock();                 // reacquire lock after being awakened.
    }
    assert( cpu_count > 0 );
    -- cpu_count;         // decrement the CPU pool (i.e., take one).
  }

  void print_ready_queue()
  {
	  int i = 1;
	  pQueue<Thread*> readyCpy = ready;
	  cerr << "Ready Queue: \n";
	  Thread* it = NULL;
	  for(it = readyCpy.front(); !readyCpy.empty(); it = readyCpy.front())
	  {
		  readyCpy.pop();
		  cerr << i <<": \"" << Me() << "\"\n";  
	  } 
  }
  void defer( int pr = Thread::me()->priority() ) {
    EXCLUSION
    if ( ready.empty() ) return;
    assert ( cpu_count == 0 );   
    ready.push( Thread::me(), pr );
 
    if(PRINT_QUEUE_ON) print_ready_queue(); // ******************* print the ready queue for debugging purposes
    
    Thread* t = ready.front();            // now ready is not empty.
    ready.pop();
    if ( t == Thread::me() ) return;
    ++cpu_count;  // leaving a CPU for *t or whoever beat him to it.
    assert( cpu_count == 1 );
    t->resume();    
    unlock();
    Thread::me()->suspend();
    lock();                 // reacquire lock after getting resumed.
    while ( cpu_count == 0 ) { 
      ready.push( Thread::me(), pr );
      unlock();
      Thread::me()->suspend();
      lock();
    }
    assert( cpu_count > 0 );
    --cpu_count;                          // taking a CPU for *me().
  }

};

extern CPUallocator CPU;  // single instance, init declaration here.


void InterruptSystem::handler(int sig) {                  // static.
  static int tickcount = 0;
  cdbg << "TICK " << tickcount << endl; 
  
  dispatcher.tick(); 
  if ( ! ( (tickcount++) % 3 ) ) {
    cdbg << "DEFERRING \n"; 
    CPU.defer();                              // timeslice: 3 ticks.
  }
  //assert( tickcount < 35 );		// Debugging purposes.
} 

void Condition::wait( int pr ) {
  push( Thread::me(), pr );
  cdbg << "releasing CPU just prior to wait.\n";
  mon.unlock();
  CPU.release();  
  cdbg << "WAITING\n";
  Thread::me()->suspend();
  CPU.acquire();  
  mon.lock(); 
}

class ThreadGraveyard : Monitor {
	private:
		Condition deadThread;
		vector<Thread*> graveyard;
	public:
		ThreadGraveyard ()
		:deadThread(this)
		{}
		void thread_cancel()
		{
			interrupts.set(InterruptSystem::alloff);
			graveyard.push_back(Thread::me()); //Push thread into graveyard
			deadThread.wait();
			
		}
		
	
} threadGraveyard;


void* Thread::start(Thread* myself) {                     // static.
	myself->thread_id = this_thread::get_id();
  interrupts.set(InterruptSystem::alloff);
  
  //********************** commenting out the debugging stuff.
  
  //cerr << "\nStarting thread " << Him(myself)  // cdbg crashes here.
       //<< " pt=" << id(pthread_self()) << endl; 
       //<< " pt=" << id(this_thread::get_id()) << endl; 
  assert( myself );
  //whoami[ pthread_self() ] = myself;
  whoami[ this_thread::get_id() ] = myself;
  assert ( Thread::me() == myself );
  interrupts.set(InterruptSystem::on);
  cdbg << "waiting for my first CPU ...\n";
  CPU.acquire();  
  cdbg << "got my first CPU\n";
  myself->action();
  cdbg << "exiting and releasing cpu.\n";
  CPU.release();
  //pthread_exit(NULL);   
  //threadGraveyard.thread_cancel();
  exit(0); // exit this thread so that thread_join() can return;
}





// ================ application stuff  ==========================

class InterruptCatcher : Thread {
public:
  // A thread to catch interrupts, when no other thread will.
  int priority () { return 1; }       // second highest priority.
  void action() { 
    cdbg << "now running\n";
    for(;;) {
      CPU.release();         
      pause(); 
      cdbg << " requesting CPU.\n";
      CPU.acquire();
    }
  }
public:
  InterruptCatcher( string name ) : Thread(name) {;}
};


class Pauser {                            // none created so far.
public:
  Pauser() { pause(); }
};




// ================== threads.cc ===================================

// Single-instance globals

//ThreadSafeMap<pthread_t,Thread*> Thread::whoami;         // static
ThreadSafeMap<thread::id,Thread*> Thread::whoami;         // static
//Idler idler(" Idler ");                        // single instance.
InterruptCatcher theInterruptCatcher("IntCatcher");  // singleton.
AlarmClock dispatcher;                         // single instance.
CPUallocator CPU(1);                 // single instance, set here.
string Him( Thread* t ) { 
  string s = t->name;
  return s == "" ? id(t) : s ; 
}
//Thread* Thread::me() { return whoami[pthread_self()]; }  // static
Thread* Thread::me() { return whoami[this_thread::get_id()]; }  // static
string Me() { return Him(Thread::me()); }

               // NOTE: Thread::start() is defined after class CPU


// ================== diagnostic functions =======================

string report() {               
  // diagnostic report on number of unassigned cpus, number of 
  // threads waiting for cpus, and which thread is first in line.
  ostringstream s;
  s << Me() << "/" << CPU.cpu_count << "/" << CPU.ready.size() << "/" 
    << (CPU.ready.empty() ? "none" : Him(CPU.ready.front())) << ": ";
  return s.str(); 
}

// ===================== mulitor.cc ================================

// #include "mulitortest.H"


// =================== main.cc ======================================

//
// main.cc 
//

// applied monitors and thread classes for testing

class SharableInteger: public Monitor {
public:
  int data;
  SharableInteger() : data(0) {;}
  void increment() {
    EXCLUSION
    ++data;
  }
  string show() {
    EXCLUSION
    return T2a(data);
  }  
} counter;                                        // single instance


