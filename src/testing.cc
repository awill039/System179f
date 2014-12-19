// Here is the latest version as of noon Saturday.  It implements the 
// following commands:
// exit
// mkdir
// ls
// rmdir
// rm
// cd
// all of which have pass a superfical level of testing.
// For diagnostic purposes, their outputs are a bit different from 
// Standard Unix-systems output.

#include<iostream>
#include<vector>
#include<queue>
#include<map>
#include<string>
#include<sstream>
#include<string.h>
#include<utility>
#include<vector>
#include<cassert>
#include<unistd.h>

using namespace std;

//==================== here are some utilities =====================

// a simple utility for splitting strings at a find-pattern.
inline vector<string>
split( const string s, const string pat ) {
  // Returns the vector of fragments (substrings) obtained by
  // splitting s at occurrences of pat.  Use C++'s string::find. If s
  // begins with an occurrence of pat, it has an empty-string
  // fragment.  Similarly for trailing occurrences of pat.  After each
  // occurrence of pat, scanning resumes at the following character,
  // if any.
  vector<string> v;
  int i = 0;
  for (;;) {
    int j = s.find(pat,i);
    if ( j == (int) string::npos ) {          // pat not found.
      v.push_back( s.substr(i) );      // append final segment.
      return v;
    }
    v.push_back( s.substr(i,j-i) ); // append nonfinal segment.
    i = j + pat.size();                     // next valus of i.
  }
}

inline  // joins vector of strings separating via a pattern.
string
join( const vector<string> v, const string pat, int start=0, int end=-1 ) {
  if ( end < 0 ) end += v.size();
  //  assert ( start < v.size() );   // should throw an exception.
  if ( start >= v.size() ) return "";
  string s = v[start++];
  for ( int i = start; i <= end; ++i )  s += pat + v[i];
  return s;
}

template< typename T > string T2a( T x ) {
  // A simple utility to turn things of type T into strings
  stringstream ss;
  ss << x;
  return ss.str();
}

template< typename T > T a2T( string x ) {
  // A simple utility to turn strings into things of type T
  // Must be tagged with the desired type, e.g., a2T<float>(35)
  stringstream ss(x);
  T t;
  ss >> t;
  return t;
}

//===================== end of utilities =========================

typedef vector<string> Args;  // could as well use list<string>
typedef int App(Args);  // apps take a vector of strings and return an int.

// some forward declarations
class Device;
class Directory;


class InodeBase {
public: 
  virtual string type() = 0;
  virtual string show() = 0;
  virtual void ls() = 0; 
  static int count;
  InodeBase() { idnum = count ++;}  
  int idnum;
};
int InodeBase::count = 0;

template< typename T >
class Inode : public InodeBase {
};

template<>
class Inode<App> : public InodeBase {
public:
  App* file;
  Inode ( App* x ) : file(x) {}
  string type() { return "app"; }
  string show() {   // a simple diagnostic aid
    return "This is inode #" + T2a(idnum) + ", which describes a/an " + type() + ".\n"; 
  } 
  void ls() { cout << show(); }
};



// template< typename TT >
// class Inode : public InodeBase {
// public:
//   TT* file;
//   Inode ( TT* x ) : file(x) {}
//   string type = 
//     ( static_cast< App* >( file ) )       ? "app" :
//     ( static_cast< Directory*> ( file ) ) ? "dir" : 
//                                                "" ;
//   string show() {   // a simple diagnostic aid
//     return " This is an inode that describes a/an " + type + ".\n"; 
//   } 
// };


class Directory {
public:

  // directories point to inodes, which in turn point to files.

  // map<string, Inode*> theMap;  // the data for this directory
  map<string, InodeBase*> theMap;  // the data for this directory

  Directory() { theMap.clear(); }

  void ls() {   
    // for (auto& it : theMap) { 
    for (auto it = theMap.begin(); it != theMap.end(); ++it ) { 
      if ( it->second ) cout << it->first << " " << it->second->show(); 
    }
    if ( ! theMap.size() ) cout << "Empty Map"<< endl;  // comment lin out
  } 

  int rm( string s ) { theMap.erase(s); }

  template<typename T>                               
  int mk( string s, T* x ) {
    Inode<T>* ind = new Inode<T>(x);
    theMap[s] = ind;
  }
 
};


template<>
class Inode<Directory> : public InodeBase {
public:
  string type() { return "dir"; }
  Directory* file;
  Inode<Directory> ( Directory* x ) : file(x) {}
  string show() {   // a simple diagnostic aid
    return " This is inode #" + T2a(idnum) + ", which describes a/an " + type() + ".\n"; 
  } 
  void ls() { file->ls(); }
};



//Inode<Directory>* root = new Inode<Directory>;
Inode<Directory> temp( new Directory );  // creates new Directory 
                                                 // and its inode
Inode<Directory>* const root = &temp;  
Inode<Directory>* wdi = root;       // Inode of working directory
Directory* wd() { return wdi->file; }        // Working Directory


Inode<Directory>* search( string s ) {
  // to process a path name previx, which should lead to a directory.
  if ( s == "" ) return wdi;
  vector<string> v = split(s,"/");
  Inode<Directory>* ind = wdi;
  if ( v[0] == "" ) ind = root;
  stringstream prefix;
  queue<string> suffix;
  for ( auto it : v ) suffix.push( it ); // put everything onto a queue
  for (;;) {  // now iterate through that queue.
    while ( suffix.size() && suffix.front() == "" ) suffix.pop();
    if ( suffix.empty() ) return ind;
    string s = suffix.front();
    suffix.pop();
    if ( prefix.str().size() != 0 ) prefix << "/";
    prefix << s; 
    Directory* dir = dynamic_cast<Directory*>(ind->file);
    if ( ! dir ) {
      cerr << ": cannot access "<< prefix << ": Not a directory\n";
      return 0;
    }
    if ( (dir->theMap)[s] != 0 ) {
      ind = dynamic_cast<Inode<Directory>*>( (dir->theMap)[s] );
      if ( !ind ) {
        cerr << prefix.str() << ": No such file or directory\n";
        return 0;
      }
    }   
  }
  return 0;
}

string current = "root";
int cd( Args tok ) {
  vector<string> v = split( tok[0], "/" );
  string s = v.back();
  v.pop_back();
  string s2 = join(v,"/");
  Inode<Directory>* ind = search( s2 );
  InodeBase* b = ind->file->theMap[s];
  if ( !b ) { 
    cerr << "shell: cd:" << s << ": No such file or directory\n";
    return -1;
  }
  cerr << "Hi " << s << " " <<  b-> type() << endl;
  if ( b->type() == "dir" ) {
    wdi = dynamic_cast<Inode<Directory>*>(b);
    current = s;
    return 0;
  } else {
    cerr << "shell: cd:" << s << ": Not a directory\n";
    return -1;
  }
}


int ls( Args tok ) {
  if ( tok.empty() ) {
    wd()->ls();
    return 0;
  }
  vector<string> v = split( tok[0], "/" );
  string s = v.back();
  v.pop_back();
  string s2 = join(v,"/");
  Inode<Directory>* ind = search( s2 );
  if ( ! ind ) { 
    cerr << "ls: " << s << "/" << s2 << " not found\n"; 
    return -1; 
  }
  InodeBase* b = ind->file->theMap[s];
  if ( b ) { 
    if ( b->type() != "dir" ) cout << s << ": ";
    b->ls();  
    return 0;
  } else {
    cerr << "ls: " << s << ": No such file or directory\n";
    return -1;
  }
}


int mkdir( Args tok ) {
  vector<string> v = split( tok[0], "/" );
  string s = v.back();
  v.pop_back();
  string s2 = join(v,"/");
  InodeBase* ind = search( s2 );
  if ( ind == 0 ) {
    cerr << "mkdir: " << s2 << ": no such directory\n";
    return -1;
  } 
  Inode<Directory>* dir_ptr = dynamic_cast<Inode<Directory>*>(ind);
  Directory* d = dir_ptr->file;
  d->mk( s, new Directory );
  return 0;
}   


int rmdir( Args tok ) {
  vector<string> v = split( tok[0], "/" );
  string s = v.back();
  v.pop_back();
  string s2 = join(v,"/");
  InodeBase* ind = search( s2 );
  Inode<Directory>* dir_ptr = dynamic_cast<Inode<Directory>*>(ind);
  if ( ! dir_ptr ) { 
    cerr << "rmdir: failed to remove `" << tok[0] 
         << "'; no such file or directory.\n";
  } else if ( ! dir_ptr->file->theMap[s] ) {  // oops: if not there.
    cerr << "rmdir: failed to remove `" << s
         << "'; no such file or directory.\n";
  } else {
    dir_ptr->file->rm(s);
  }
}


int rm( Args tok ) {
  vector<string> v = split( tok[0], "/" );
  string s = v.back();
  v.pop_back();
  string s2 = join(v,"/");
  Inode<Directory>* ind = search( s2 );
  if ( ind->file->theMap[s]->type() == "dir" ) {  
    cout << "rm: cannot remove `"<< s << "': is a  directory\n";
    return 0;
  } 
  cout << "rm: remove regular file `" << s << "'? ";
  string response;
  cin >> response;
  if ( response[0] != 'y' && response[0] != 'Y' )  return 0;
  ind->file->rm(s);
  return 0;
}

int exit( Args tok ) {
  _exit(0);
}


map<string, App*> apps = {
  pair<const string, App*>("ls", ls),
  pair<const string, App*>("mkdir", mkdir),
  pair<const string, App*>("rmdir", rmdir),
  pair<const string, App*>("exit", exit),
  pair<const string, App*>("rm", rm),
  pair<const string, App*>("cd", cd)
};  // app maps mames to their implementations.


int main( int argc, char* argv[] ) {
  root->file = new Directory;   // OOPS!!! review this.
  for( auto it : apps ) {
    Inode<App>* temp(new Inode<App>(rmdir));
    InodeBase* junk = static_cast<InodeBase*>(temp);
    //    InodeBase* junk = temp;
    wd()->theMap[it.first] = new Inode<App>(it.second);
  }
  // a toy shell routine.
  while ( ! cin.eof() ) {
    cout << current << "? "  ;             // prompt. add name of wd.
    string temp; 
    getline( cin, temp );                    // read user's response.
    stringstream ss(temp);             // split temp at white spaces.
    string cmd;
    ss >> cmd;                              // first get the command.
    if ( cmd == "" ) continue;
    vector<string> args;          // then get its cmd-line arguments.
    string s;
    while( ss >> s ) args.push_back(s);
    Inode<App>* junk = static_cast<Inode<App>*>( (root->file->theMap)[cmd] );
    if ( ! junk ) {
      cerr << "shell: " << cmd << " command not found\n";
      continue;
    }
    App* thisApp = static_cast<App*>(junk->file);
    if ( thisApp != 0 ) {
      thisApp(args);          // if possible, apply cmd to its args.
    } else { 
      cerr << "Instruction " << cmd << " not implemented.\n";
    }
  }

  cout << "exit" << endl;
  return 0;                                                  // exit.

}

