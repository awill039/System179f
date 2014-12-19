#include "filesystem.h"

using namespace filesystem;
int main( int argc, char* argv[] ) {
  FSInit("info.txt");
  // a toy shell routine.
  while ( ! cin.eof() ) {
    cout << "["<< current <<"]? ";            // prompt. add name of wd.
    string temp; 
    getline( cin, temp );                    // read user's response.
    stringstream ss(temp);             // split temp at white spaces.
    vector<string> args;          // then get its cmd-line arguments.
    string s;
    while( ss >> s ) args.push_back(s);
    if ( args.size() == 0 ) continue;
    string cmd = args[0];
    if ( cmd == "" ) continue;
    Inode<App>* junk = static_cast<Inode<App>*>((dynamic_cast<Inode<Directory>*>(root->file->theMap["bin"])->file->theMap)[cmd] ); //Update to put apps in a directory
    if ( ! junk ) {
	  (dynamic_cast<Inode<Directory>*>(root->file->theMap["bin"])->file->theMap).erase(cmd);
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
  preserve(root, "");	

  cout << "exit" << endl;
  return 0;                                                  // exit.

}

