#include <string>
#include <iostream>
#include <sstream>
#include "devices.h"

using namespace std;


void myPrint(char* buf, int size)
{
	cout << "myPrint: ";
	for(int i = 0; i < size; i++)
	{
		cout << buf[i];
	}
	cout << "." << endl;
}

int main(int argc, char *argv[])
{
	/*
		DO NOT PASS STDIN/STDOUT TO iDevice()
	*/

	char buf1[50];
	memset(buf1,0,50);

	/*
		iDevice Tests
	*/
	string str("Hello World!");
	stringstream ss;
	ss << str;
	cout << "ss: " << ss.str() << endl;

	iDevice<char> in(&ss);

	cout << "Testing iDevice read()\n";
	in.read(buf1,15);
	myPrint(buf1,12);
	
	cout << "Testing iDevice seek(SEEK_SET)\n";
	in.seek(-2,SEEK_SET);
	in.read(buf1,2);
	myPrint(buf1,12);


	cout << "Testing iDevice seek(SEEK_CUR)\n";
	in.seek(0,SEEK_SET);
	in.seek(-2,SEEK_CUR);
	in.read(buf1,2);
	myPrint(buf1,12);

	cout << "Testing iDevice seek(SEEK_END)\n";
	in.seek(-2,SEEK_END);
	in.read(buf1,2);
	myPrint(buf1,12);



	/*
		oDevice Tests
	*/
	stringstream ss2;
	ss2 << str;
	cout << "ss2: " << ss2.str() << endl;
	cout << "ss2 length: " << ss2.str().length() << endl;
	oDevice<char> out(&ss2);

	cout << "Testing oDevice write()\n";
	cout << "buf1: " << buf1 << endl;
	//Programmer's responsibility to NOT write beyond buffer size!
	out.write(buf1,10);

	cout << "Testing oDevice seek(SEEK_SET)\n";
	out.seek(-1,SEEK_SET);

	cout << "Testing oDevice seek(SEEK_CUR)\n";
	out.seek(100,SEEK_CUR);

	cout << "Testing oDevice seek(SEEK_END)\n";
	out.seek(-10,SEEK_END);



	/*
		ioDevice Tests
	*/
	stringstream ss3;
	ss3 << str;
	cout << "ss3: " << ss3.str() << endl;
	cout << "ss3 length: " << ss3.str().length() << endl;
	ioDevice<char> inout(&ss3);

	cout << "Testing ioDevice open()\n";
	inout.open("ioDevice",ODD_RDWR);

	cout << "Testing ioDevice seek()\n";
	inout.seek(5,SEEK_SET);

	cout << "Testing ioDevice rewind()\n";
	inout.rewind();

	cout << "Testing ioDevice read()\n";
	cout << "buf1: " << buf1 << endl;
	inout.seek(5,SEEK_SET);
	inout.read(buf1,12);
	myPrint(buf1,12);

	cout << "Testing ioDevice write()\n";
	cout << "buf1: " << buf1 << endl;
	inout.write(buf1,12);
	myPrint(buf1,12);

	cout << drivers["ioDevice"]->driverName << endl;// << drivers[0]->second << endl;
	
/*
	char buftest[10];
	string hello = "hello";
	iDevice<char> testStr(hello);
	testStr.read(buftest,10);
	myPrint(buftest,10);
	//*/

/*
	char buftest[] = "world";
	string hello = "hello";
	oDevice<char> testStr(hello);
	testStr.write(buftest,10);
	myPrint(buftest,10);
//*/

	char buftest[] = "world";
	string hello = "hello";
	ioDevice<char> testStr(hello);

	testStr.write(buftest,10);
	myPrint(buftest,10);

	testStr.read(buftest,10);
	myPrint(buftest,10);


 	return 0;
}
