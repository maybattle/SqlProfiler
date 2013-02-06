#include "stdafx.h"
#include "TcpMessenger.h"




C::TcpMessenger()
{
}

C::~TcpMessenger()
{
	WSACleanup();
}


void C::Connect(std::string ipAddress, int port){
	WSADATA wsadata;
	if(WSAStartup(MAKEWORD(2,0),&wsadata) <0) return;
	SOCKADDR_IN target;
	target.sin_family = AF_INET;
	target.sin_port = htons(port);
	target.sin_addr.S_un.S_addr = inet_addr(ipAddress.c_str());
	if(::connect(_socket,(sockaddr*)&target,sizeof(target))){
		string error = strerror(WSAGetLastError());
		throw error;
	}
}


