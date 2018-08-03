//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACSERVICE_H
#define GUACAMOLE_SERVER_SRC_GUACSERVICE_H

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <guacservice/GuacConnectionHandler.h>
#include <guacservice/GuacConfig.h>
#include <guacservice/GuacLogger.h>
#include <guacservice/GuacConnectionSSLSocket.h>
#include <guacservice/GuacConnectionTCPSocket.h>

#define MAX_GUAC_SERVICE_BACKLOG 5

class GuacService
{
private:
   boost::asio::ip::tcp::acceptor * m_Acceptor;
   boost::asio::io_service m_IOService;
   GuacConnectionHandler * m_ConnectionsHandler;
   GuacConfig m_Config;
   bool m_IsServiceRunning;

private:
   /**
    * Binds the acceptor to the given configured host and port
    * @return
    */
   bool BindAcceptor();
   /**
    * Clenas the service, closes all the connections and the IOServices
    */
   void CleanupService();
   /**
    * Listens the new connection, each connection has its seperate IOService
    */
   void ListenAndAcceptNewConnections();

public:
   /**
    * Constructor
    */
   GuacService();
   /**
    * Destructor
    */
   virtual ~GuacService();
   /**
    * Starts the actual guac service with the given config
    * @param rConfig
    * @return
    */
   bool StartGuacamoleService(const GuacConfig & rConfig);
   /**
    * Checks whether the service is running or not
    * @return
    */
   bool IsServiceRunning() const;
};

#endif //GUACAMOLE_SERVER_SRC_GUACSERVICE_H
