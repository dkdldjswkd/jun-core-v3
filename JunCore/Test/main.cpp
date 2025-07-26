#include <iostream>
#include "CryptoExample.h"
#include "ProtobufExample.h"
using namespace std;

int main()
{
	// Protobuf example
	ProtobufExample();

	// Crypto example
	CryptoExample::RunCryptoExample();
	
	return 0;
}