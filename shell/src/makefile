
EXECUTABLES = shell
OBJECTS = 
CXXFLAGS= -ggdb
CXX = g++
STDFLAGS= -std=c++0x

all: $(EXECUTABLES)

shell: myshell.cc
	$(CXX) $(CXXFLAGS) $(STDFLAGS) -lreadline -pthread myshell.cc thread.h -o shell
	
test: testing.cc
	$(CXX) $(CXXFLAGS) $(STDFLAGS) testing.cc -o test

clean:
	rm -f $(OBJECTS) $(EXECUTABLES) *.o *~
