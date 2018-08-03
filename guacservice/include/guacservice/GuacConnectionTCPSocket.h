//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCONNECTIONTCPSOCKET_H
#define GUACAMOLE_SERVER_SRC_GUACCONNECTIONTCPSOCKET_H

#include <guacservice/IGuacConnectionSocket.h>
#include <guacservice/GuacLogger.h>
#include <guacamole/socket.h>
#include <boost/asio.hpp>

typedef boost::asio::ip::tcp::socket TCPSocket;
typedef boost::shared_ptr<TCPSocket> TCPSocketPtr;

class GuacConnectionTCPSocket : public IGuacConnectionSocket
{
private:
   boost::asio::io_service m_IOService;
   TCPSocketPtr m_Socket;
   guac_socket * m_GuacSocket;

public:
   /**
    * Constructor
    */
   GuacConnectionTCPSocket();
   /**
    * Destructor
    */
   virtual ~GuacConnectionTCPSocket();
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

#endif /* GUACAMOLE_SERVER_SRC_GUACCONNECTIONTCPSOCKET_H */