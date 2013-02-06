using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace ProfilerLauncher
{
    public class TcpReceiveServer
    {
        private const int _port = 28015;
        private Socket _socket;
        private readonly byte[] _buffer = new byte[1024];
        private readonly StringBuilder _dataReceived = new StringBuilder();
        private Thread _socketThread;

        public void Close()
        {
            if (_socket != null) _socket.Close(1000);
            _socket = null;
        }

        public void Open()
        {
            
            _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            _socket.Bind(new IPEndPoint(IPAddress.Any,_port));
            _socket.Listen(1);
            _socketThread = new Thread(ListenAndProcessData);
            _socketThread.Start();
  
        }

        private void ListenAndProcessData()
        {
            while (_socket!=null)
            {
                try
                {
                    var acceptedClient = _socket.Accept();
                    int bytesRead;
                    while ((bytesRead = acceptedClient.Receive(_buffer)) > 0)
                    {
                        _dataReceived.Append(Encoding.UTF8.GetString(_buffer, 0, bytesRead));
                    }
                    File.AppendAllText("ReceiveLog.txt", _dataReceived.ToString());
                    ResetDataReceived();
                }
                catch (SocketException e)
                {
                    
                }
            }
        }

        private void ResetDataReceived()
        {
            _dataReceived.Length = 0;
            _dataReceived.Capacity = 16; //default capacity
        }
    }
}