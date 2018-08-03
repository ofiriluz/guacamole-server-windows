//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCONNECTIONSSLSOCKET_H
#define GUACAMOLE_SERVER_SRC_GUACCONNECTIONSSLSOCKET_H

#include <guacservice/IGuacConnectionSocket.h>
#include <guacservice/GuacLogger.h>
#include <guacamole/socket.h>
#include <boost/asio/ssl.hpp>

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSLSocket;
typedef boost::shared_ptr<SSLSocket> SSLSocketPtr;

class GuacConnectionSSLSocket : public IGuacConnectionSocket
{
private:
   std::string m_stPEMFile, m_stCertFile, m_stDHPEMFile;
   boost::asio::io_service m_IOService;
   SSLSocketPtr m_Socket;
   guac_socket * m_GuacSocket;
   boost::asio::ssl::context * m_CurrentSSLContext;

private:
   /**
    * Creates an SSL context from the given PEM file and Cert file
    * @param rOutErrorCode
    */
   void GuacConnectionSSLSocket::CreateSSLContext(boost::system::error_code & rOutErrorCode);

public:
   /**
    * Constructor
    * @param rContext
    */
   GuacConnectionSSLSocket(const std::string & stPEMFile, const std::string & stCertFile,
                           const std::string & stDHPEMFile);
   /**
    * Destructor
    */
   virtual ~GuacConnectionSSLSocket();
   /**
   * @see IGuacConnectionSocket::WaitForSocket
   * @param pAcceptor
   * @param rErrorCode
   */
   virtual void WaitForSocket(boost::asio::ip::tcp::acceptor * pAcceptor, boost::system::error_code & rErrorCode);
   /**
    * @see IGuacConnectionSocket::EstablishSocket
    * @param rErrorCode
    */
   virtual void EstablishSocket(boost::system::error_code & rErrorCode);
   /**
    * @see IGuacConnectionSocket::CloseSocket
    * @param rErrorCode
    */
   virtual void CloseSocket(boost::system::error_code & rErrorCode);
   /**
    * @see IGuacConnectionSocket::GetGuacSocket
    * @return
    */
   virtual guac_socket * GetGuacSocket();
   /**
   * @see IGuacConnectionSocket::ReadSome
   * @param pBuffer
   * @param sSize
   * @param rErrorCode
   * @return
   */
   virtual size_t ReadSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode);
   /**
   * @see IGuacConnectionSocket::ReadSome
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t ReadSome(char * pBuffer, size_t sSize);
   /**
   * @see IGuacConnectionSocket::WriteSome
   * @param pBuffer
   * @param sSize
   * @param rErrorCode
   * @return
   */
   virtual size_t WriteSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode);
   /**
   * @see IGuacConnectionSocket::WriteSome
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t WriteSome(char * pBuffer, size_t sSize);
   /**
   * @see IGuacConnectionSocket::WriteAll
   * @param pBuffer
   * @param sSize
   * @param rErrorCode
   * @return
   */
   virtual size_t WriteAll(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode);
   /**
   * @see IGuacConnectionSocket::WriteAll
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t WriteAll(char * pBuffer, size_t sSize);
   /**
    * @see IGuacConnectionSocket::IsSocketOpened
    * @return
    */
   virtual bool IsSocketOpened() const;
   /**
    * @see IGuacConnectionSocket::CloseRead
    * @param rErrorCode
    */
   virtual void CloseRead(boost::system::error_code & rErrorCode);
   /**
    * @see IGuacConnectionSocket::CloseWrite
    * @param rErrorCode
    */
   virtual void CloseWrite(boost::system::error_code & rErrorCode);
};

#endif /* GUACAMOLE_SERVER_SRC_GUACCONNECTIONSSLSOCKET_H */