//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_IGUACCONNECTIONSOCKET_H
#define GUACAMOLE_SERVER_SRC_IGUACCONNECTIONSOCKET_H

#include <guacamole/socket-types.h>
#include <boost/system/error_code.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

class IGuacConnectionSocket
{
public:
   /**
    * Default Constructor
    */
   IGuacConnectionSocket() = default;
   /**
    * Default Destructor
    */
   virtual ~IGuacConnectionSocket() = default;
   /**
    * Waits for a socket with a given acceptor
    * On a new connection arrival from this acceptor, the connection will be accepted and saved on this socket
    * @param pAcceptor
    * @param rErrorCode
    */
   virtual void WaitForSocket(boost::asio::ip::tcp::acceptor * pAcceptor, boost::system::error_code & rErrorCode) = 0;
   /**
    * Establishes a socket connection for this GuacConnectionSocket
    * @param rErrorCode
    */
   virtual void EstablishSocket(boost::system::error_code & rErrorCode) = 0;
   /**
    * Closes the connection
    * @param rErrorCode
    */
   virtual void CloseSocket(boost::system::error_code & rErrorCode) = 0;
   /**
    * Getter for the underlying guac_socket to be used by the inner guac libraries
    * @return
    */
   virtual guac_socket * GetGuacSocket() = 0;
   /**
    * Reads some bytes from the socket into the buffer for the given max size
    * @param pBuffer
    * @param sSize
    * @param rErrorCode
    * @return
    */
   virtual size_t ReadSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode) = 0;
   /**
   * Reads some bytes from the socket into the buffer for the given max size
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t ReadSome(char * pBuffer, size_t sSize) = 0;
   /**
    * Writes the buffer with the given size to the socket
    * @param pBuffer
    * @param sSize
    * @param rErrorCode
    * @return
    */
   virtual size_t WriteSome(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode) = 0;
   /**
   * Writes the buffer with the given size to the socket
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t WriteSome(char * pBuffer, size_t sSize) = 0;
   /**
   * Writes the buffer with the given size to the socket with ensurance that all is written
   * @param pBuffer
   * @param sSize
   * @param rErrorCode
   * @return
   */
   virtual size_t WriteAll(char * pBuffer, size_t sSize, boost::system::error_code & rErrorCode) = 0;
   /**
   * Writes the buffer with the given size to the socket with ensurance that all is written
   * @param pBuffer
   * @param sSize
   * @throws
   * @return
   */
   virtual size_t WriteAll(char * pBuffer, size_t sSize) = 0;
   /**
    * Getter for whether the socket is opened or not
    * @return
    */
   virtual bool IsSocketOpened() const = 0;
   /**
    * Closes the read side of the socket
    * @param rErrorCode
    */
   virtual void CloseRead(boost::system::error_code & rErrorCode) = 0;
   /**
    * Closes the write side of the socket
    * @param rErrorCode
    */
   virtual void CloseWrite(boost::system::error_code & rErrorCode) = 0;
};

typedef boost::shared_ptr<IGuacConnectionSocket> IGuacConnectionSocketPtr;

#endif /* GUACAMOLE_SERVER_SRC_IGUACCONNECTIONSOCKET_H */