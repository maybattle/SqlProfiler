#include "stdafx.h"

#define  C TcpMessenger

class TcpMessenger{
	TcpMessenger();
	~TcpMessenger();
public:
	void Connect(std::string ipAddress, int port);
	void TcpMessengerSendString(std::string message);
private:
	SOCKET _socket;
	void Disconnect();

};

