#include <string>
#include <iostream>
#include <vector>
#include <list>
#include <sstream>
#include <sys/wait.h>
#include <errno.h>                       // man errno for information
#include <cassert>
//#include <thread>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdlib.h>
#include <stdlib.h>
#include <cstring>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "thread.h"
#include <mutex>
#include "filesystem.h"
#include <queue>
#include <deque>


using namespace filesystem;
using namespace std;

#define each(I) for( typeof((I).begin()) it=(I).begin(); it!=(I).end(); ++it )

void testCompleteMe(int x, int y){
	char *complete = readline(""); 
	rl_on_new_line(); 
	//cout << string(complete); 
	delete complete; 
}


int doit( vector<string> tok );

//Queue for command history
deque<string>histhold;

class ThreadLocalOpenFile {     
    // don't need to be Monitors
    public:
  
  OpenFile* file;
  
  
  ThreadLocalOpenFile() {}
  ThreadLocalOpenFile( OpenFile* file )     // use of RAII
    : file(file)
  {
    ++ file->accessCount;
  }
  ~ThreadLocalOpenFile() {                  // use of RAII
    -- file->accessCount;
  }
  

};
thread_local ThreadLocalOpenFile sys;
class shellThread : public Thread {
    vector<string> tok;
    int priority() {return Thread::priority(); }
    void action (){

        string progname = tok[0];
        char* arglist[ 1 + tok.size() ];   // "1+" for a terminating null ptr.
        int argct = 0;
        for ( int i = 0; i != tok.size(); ++i ) {
     
      string progname = tok[0];
      char* arglist[ 1 + tok.size() ];   // "1+" for a terminating null ptr.
      int argct = 0;
      vector<string> apptok;
      for ( int i = 0; i != tok.size(); ++i ) {
            if      ( tok[i] == "&" || tok[i] == ";" ) break;   // arglist done.
            else if ( tok[i] == "<"  ) freopen( tok[++i].c_str(), "r", stdin  );
            else if ( tok[i] == ">"  ) freopen( tok[++i].c_str(), "w", stdout );
            else if ( tok[i] == ">>" ) freopen( tok[++i].c_str(), "a", stdout );
            else if ( tok[i] == "2>" ) freopen( tok[++i].c_str(), "w", stderr );
            else if ( tok[i] == "|"  ) {                   // create a pipeline.
              int mypipe[2];  
              int& pipe_out = mypipe[0];
              int& pipe_in  = mypipe[1];
              // Find two available ports and create a pipe between them, and
              // store output portuntitled folder# into pipe_out and input port# to pipe_in.
              if ( pipe( mypipe ) ) {     // All that is done here by pipe().
                //cerr << "myshell: " << strerror(errno) << endl; // report err
                return;
              } else if ( fork() ) {  // you're the parent and consumer here.
            dup2( pipe_out, STDIN_FILENO ); // connect pipe_out to stdin.
                close( pipe_out );        // close original pipe connections.
                close( pipe_in );  
                while ( tok.front() != "|" ) tok.erase( tok.begin() );
                tok.erase(tok.begin());                    // get rid of "|".
                exit( doit( tok ) );        // recurse on what's left of tok.
              } else {                 // you're the child and producer here.
                dup2( pipe_in, STDOUT_FILENO ); // connect pipe_in to stdout.
                close( pipe_out );        // close original pipe connections.
                close( pipe_in );
                break;                      // exec with the current arglist.
              }
            } 
            else {           // add this token a C-style argv for execvp().
              // Append tok[i].c_str() to arglist
              apptok.push_back(tok[i]);
	    }
          }
         

          // tilde expansion
          if ( progname[0] == '~' ) progname = getenv("HOME")+progname.substr(1);
//          execvp( progname.c_str(), arglist );         // execute the command.

		  Inode<App>* junk = static_cast<Inode<App>*>((dynamic_cast<Inode<Directory>*>(root->file->theMap["bin"])->file->theMap)[tok[0]] ); //Update to put apps in a directory

		    if ( ! junk ) {
			  (dynamic_cast<Inode<Directory>*>(root->file->theMap["bin"])->file->theMap).erase(apptok[0]);
			  cerr << "shell: " << apptok[0] << " command not found\n";
			  continue;
			}
			App* thisApp = static_cast<App*>(junk->file);
			if ( thisApp != 0 ) {
			  thisApp(apptok);
			  freopen("/dev/tty", "a", stdout);          // if possible, apply cmd to its args.
			  return;
			} else { 
			  cerr << "Instruction " << tok[0] << " not implemented.\n";
			}

        }
    }
   
   
 
   
    public:
        shellThread(string name, int priority, vector<string> v)
        :tok(v), Thread(name, priority)
        {}
       
};

int doit( vector<string> tok ) { 
  // Executes a parsed command line returning command's exit status.

  if ( tok.size() == 0 ) return 0;             // nothing to be done.

  string progname = tok[0]; 
  assert( progname != "" );
  if(progname == "help"){
    cout << "ls: list contents of the current directory" << endl; 
  cout << "mkdir: creates directory with specified name" << endl; 
  cout << "rmdir: removes directory with specified name" << endl; 
  cout << "exit: ends execution of the shell"<< endl; 
  cout <<  "rm: removes file with specified name"<< endl; 
  cout << "cd: changes current working directory to specified directory"<< endl;
 cout << "touch: access/change access and modification timestamps"<< endl;
  cout << "pwd: displays current working directory"<< endl;
cout << "tree: outputs list of directories and associated directories"<< endl;
cout << "echo: sends specified string to standard output"<< endl;
cout << "cat: concatenates specified string onto standard output or specified file" << endl;
cout << "wc: prints newline, word, and byte counts for each file" << endl;
cout << "write: writes to a file descriptor" << endl;
cout << "read: read specified amount of bytes from file descriptor into buffer"<< endl;
cout << "cp: copies specified file or directory"<< endl;
return -1;
  }
  if(progname == "pstree"){
    
    
  }
  shellThread thread1 ("Temp name", INT_MAX,tok);
  //cerr << "thread exiting\n";
  thread1.join();
  //cerr << "returning\n";
  return 0;
}




int main( int argc, char* argv[] ) {

  FSInit("info.txt");

  while ( ! cin.eof() ) {
    cout << "? " ;                                         // prompt.

    string temp = "";
    getline( cin, temp );

/*
	rl_command_func_t testCompleteMe; 
	rl_bind_key('\t', testCompleteMe); 
	*/


	if(histhold.size() < 10){
		histhold.push_back(temp);
	}
	else{
		histhold.pop_front();
		histhold.push_back(temp);
	}
    cout.flush();
	if (temp == "clear"){
		cout << string(100,'\n'); 
		temp=""; 
	} else{
     if(temp == "history"){
       int i = 1;
       int cmd;
       for (auto it = histhold.cbegin(); it != histhold.cend(); ++it){
          cout << i << ": " <<  *it << endl;
          ++i;
       }
   
       cout << "Command #: ";
       cin >> cmd;
       cin.clear(); cin.ignore(100,'\n');
       //cout << "selected is " << histhold[cmd - 1] << endl;
       temp = histhold[cmd - 1];
    }

		stringstream ss(temp);      // split temp at white spaces into v.
		while ( ss ) {
		vector<string> v;
		string s;
		while ( ss >> s ) {
			v.push_back(s);
			if ( s == "&" || s == ";" ) break;   
		}

		int status = doit( v );           // FIX make status available.
     
		}
	}
  }
  //cerr << "exit" << endl;
  return 0;                                                  // exit.
  //*/
//    testCompleteMe();
}

