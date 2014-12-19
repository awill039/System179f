
EXECUTABLES = shell
OBJECTS = 
CXXFLAGS= -ggdb
CXX = g++
STDFLAGS= -std=c++11
all: $(EXECUTABLES)

source: 
	./sourcec11

shell: myshell.cc
	$(CXX) $(CXXFLAGS) $(STDFLAGS) -lreadline -pthread myshell.cc filesystem.h thread.h -o shell
	
test: testing.cc
	$(CXX) $(CXXFLAGS) $(STDFLAGS) testing.cc -o test

devices: devices.cpp
	$(CXX) $(CXXFLAGS) -std=c++11 -pthread devices.cpp devices.h devicemain.cpp -w -o devices
	
history: history.cc
	$(CXX) $(CXXFLAGS) $(STDFLAGS) history.cc -o history
	
clean:
	rm -f $(OBJECTS) $(EXECUTABLES) *.o *~
