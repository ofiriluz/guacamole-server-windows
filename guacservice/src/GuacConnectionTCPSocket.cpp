#include <guacservice/GuacConnectionTCPSocket.h>

GuacConnectionTCPSocket::GuacConnectionTCPSocket()
        : m_GuacSocket(nullptr)
{

}

GuacConnectionTCPSocket::~GuacConnectionTCPSocket()
{
   boost::system::error_code ec;
   CloseSocket(ec);
}

void GuacConnectionTCPSocket::WaitForSocket(boost::asio::ip::tcp::acceptor * pAcceptor,
                                            boost::system::error_code & rErrorCode)
{
   // Create the socket and close the old one if open
   if(IsSocketOpened())
   {
      CloseSocket(rErrorCode);
   }
   m_Socket.reset(new TCPSocket(m_IOService));

   // Wait for new connection
   pAcceptor->accept(*m_Socket, rErrorCode);

   if(!rErrorCode)
   {
      // Clear the error just incase
      rErrorCode.clear();
   }
}

void GuacConnectionTCPSocket::EstablishSocket(boost::system::error_code & rErrorCode)
{
   // Open the guac socket
   m_GuacSocket = guac_socket_boost_tcp_socket(m_Socket);
   if(m_GuacSocket)
   {
      // Clear the error just incase
      rErrorCode.clear();
   }
   else
   {
      m_Socket->close();
      rErrorCode.assign(boost::system::errc::no_buffer_space, boost::system::system_category());
   }
}

void GuacConnectionTCPSocket::CloseSocket(boost::system::error_code & rErrorCode)
{
   if(!IsSocketOpened())
   {
      return;
   }

   m_Socket->close(rErrorCode);
   guac_socket_free(m_GuacSocket);
   m_GuacSocket = nullptr;
}

guac_socket * GuacConnectionTCPSocket::GetGuacSocket()
{
   return m_GuacSocket;
}

size_t GuacConnectionTCPSocket::ReadSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return m_Socket->read_some(boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionTCPSocket::ReadSome(char * pBuffer, size_t sSize)
{
   return m_Socket->read_some(boost::asio::buffer(pBuffer, sSize));
}

size_t GuacConnectionTCPSocket::WriteSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return m_Socket->write_some(boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionTCPSocket::WriteSome(char * pBuffer, size_t sSize)
{
   return m_Socket->write_some(boost::asio::buffer(pBuffer, sSize));
}

size_t GuacConnectionTCPSocket::WriteAll(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return boost::asio::write(*m_Socket, boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionTCPSocket::WriteAll(char * pBuffer, size_t sSize)
{
   return boost::asio::write(*m_Socket, boost::asio::buffer(pBuffer, sSize));
}

bool GuacConnectionTCPSocket::IsSocketOpened() const
{
   return m_Socket && m_Socket->is_open();
}

void GuacConnectionTCPSocket::CloseRead(boost::system::error_code & rErrorCode)
{
   if(!IsSocketOpened())
   {
      return;
   }
   m_Socket->shutdown(boost::asio::ip::tcp::socket::shutdown_receive, rErrorCode);
}

void GuacConnectionTCPSocket::CloseWrite(boost::system::error_code & rErrorCode)
{
   if(!IsSocketOpened())
   {
      return;
   }
   m_Socket->shutdown(boost::asio::ip::tcp::socket::shutdown_send, rErrorCode);
}