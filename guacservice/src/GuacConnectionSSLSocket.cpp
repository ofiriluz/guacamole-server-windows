#include <guacservice/GuacConnectionSSLSocket.h>
#include "guacservice/GuacConnectionTCPSocket.h"

GuacConnectionSSLSocket::GuacConnectionSSLSocket(const std::string & stPEMFile, const std::string & stCertFile,
                                                 const std::string & stDHPEMFile)
        : m_GuacSocket(nullptr), m_stPEMFile(stPEMFile), m_stCertFile(stCertFile), m_stDHPEMFile(stDHPEMFile),
          m_CurrentSSLContext(nullptr)
{

}

GuacConnectionSSLSocket::~GuacConnectionSSLSocket()
{
   boost::system::error_code ec;
   CloseSocket(ec);
}

void GuacConnectionSSLSocket::CreateSSLContext(boost::system::error_code & rOutErrorCode)
{
   // Delete the old ssl context if exists
   if(m_CurrentSSLContext)
   {
      delete m_CurrentSSLContext;
   }

   // Create the context and fill it
   m_CurrentSSLContext = new boost::asio::ssl::context(m_IOService, boost::asio::ssl::context::sslv23_server);
   m_CurrentSSLContext->set_options(boost::asio::ssl::context::default_workarounds
                                    | boost::asio::ssl::context::no_sslv2
                                    | boost::asio::ssl::context::single_dh_use, rOutErrorCode);
   if(!rOutErrorCode)
   {
      m_CurrentSSLContext->use_private_key_file(m_stPEMFile, boost::asio::ssl::context::pem, rOutErrorCode);
   }
   if(!rOutErrorCode)
   {
      m_CurrentSSLContext->use_certificate_chain_file(m_stCertFile, rOutErrorCode);
   }
   if(!rOutErrorCode)
   {
      m_CurrentSSLContext->use_tmp_dh_file(m_stDHPEMFile, rOutErrorCode);
   }
   if(rOutErrorCode)
   {
      delete m_CurrentSSLContext;
      m_CurrentSSLContext = nullptr;
   }
}

void GuacConnectionSSLSocket::WaitForSocket(boost::asio::ip::tcp::acceptor * pAcceptor,
                                            boost::system::error_code & rErrorCode)
{
   // Create the socket and close the old one if open
   if(IsSocketOpened())
   {
      CloseSocket(rErrorCode);
   }

   CreateSSLContext(rErrorCode);

   if(!rErrorCode)
   {
      m_Socket.reset(new SSLSocket(m_IOService, *m_CurrentSSLContext));

      // Wait for new connection
      pAcceptor->accept(m_Socket->lowest_layer(), rErrorCode);

      if(!rErrorCode)
      {
         // Clear the error just incase
         rErrorCode.clear();
      }
   }
}

void GuacConnectionSSLSocket::EstablishSocket(boost::system::error_code & rErrorCode)
{
   // Handle SSL handshake
   m_Socket->handshake(boost::asio::ssl::stream_base::server, rErrorCode);

   if(!rErrorCode)
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
         m_Socket->lowest_layer().close();
         rErrorCode.assign(boost::system::errc::no_buffer_space, boost::system::system_category());
      }
   }
   else
   {
      m_Socket->lowest_layer().close();
   }
}

void GuacConnectionSSLSocket::CloseSocket(boost::system::error_code & rErrorCode)
{
   if(m_CurrentSSLContext)
   {
      delete m_CurrentSSLContext;
      m_CurrentSSLContext = nullptr;
   }

   if(!IsSocketOpened())
   {
      return;
   }

   m_Socket->shutdown(rErrorCode);
   guac_socket_free(m_GuacSocket);
   m_GuacSocket = nullptr;
}

guac_socket * GuacConnectionSSLSocket::GetGuacSocket()
{
   return m_GuacSocket;
}

size_t GuacConnectionSSLSocket::ReadSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return m_Socket->read_some(boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionSSLSocket::ReadSome(char * pBuffer, size_t sSize)
{
   return m_Socket->read_some(boost::asio::buffer(pBuffer, sSize));
}

size_t GuacConnectionSSLSocket::WriteSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return m_Socket->write_some(boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionSSLSocket::WriteSome(char * pBuffer, size_t sSize)
{
   return m_Socket->write_some(boost::asio::buffer(pBuffer, sSize));
}

size_t GuacConnectionSSLSocket::WriteAll(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode)
{
   return boost::asio::write(*m_Socket, boost::asio::buffer(pBuffer, sSize), rErrorCode);
}

size_t GuacConnectionSSLSocket::WriteAll(char * pBuffer, size_t sSize)
{
   return boost::asio::write(*m_Socket, boost::asio::buffer(pBuffer, sSize));
}

bool GuacConnectionSSLSocket::IsSocketOpened() const
{
   return m_Socket && m_Socket->lowest_layer().is_open();
}

void GuacConnectionSSLSocket::CloseRead(boost::system::error_code & rErrorCode)
{
   if(!IsSocketOpened())
   {
      return;
   }
   m_Socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_receive, rErrorCode);
}

void GuacConnectionSSLSocket::CloseWrite(boost::system::error_code & rErrorCode)
{
   if(!IsSocketOpened())
   {
      return;
   }
   m_Socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, rErrorCode);
}