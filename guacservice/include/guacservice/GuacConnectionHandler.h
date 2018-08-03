//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_GUACCONNECTIONHANDLER_H
#define GUACAMOLE_GUACCONNECTIONHANDLER_H

#include <guacservice/GuacConnection.h>
#include <guacservice/IGuacClientProcessHandlerMapObserver.h>
#include <guacservice/IGuacConnectionNotifier.h>
#include <guacservice/GuacConfig.h>
#include <vector>
#include <boost/asio/ip/tcp.hpp>
#include <utility>
#include <map>

class GuacConnectionHandler : public IGuacClientProcessHandlerMapObserver, public IGuacConnectionNotifier
{
private:
   std::vector<GuacConnectionSmartPtr> m_GuacConnections;
   boost::mutex m_GuacConnectionsMutex;
   const GuacConfig & m_Config;

public:
   /**
    * Constructor
    * @param rConfig
    */
   GuacConnectionHandler(const GuacConfig & rConfig);
   /**
    * Default Destructor
    */
   virtual ~GuacConnectionHandler() = default;
   /**
    * Handles a new connection coming from the service, saves it to the connection vector and starts it
    * @param pClientConnection
    * @return
    */
   bool HandleNewConnection(const IGuacConnectionSocketPtr & pClientConnection);
   /**
    * Closes all the connections on this handler
    */
   void CloseConnections();
   /**
    * Callback for when a new client process is created, sets its config
    * @param stID
    */
   virtual void OnClientProcessCreated(const std::string & stID);
   /**
    * Callback for when a client process handler is removed, removes all the associated connections from the vector and closes them
    * @param stID
    */
   virtual void OnClientProcessRemoved(const std::string & stID);
   /**
    * Callback for when theres a connection failure, removes the connection from the vector and closes it
    * @param stConnectionID
    */
   virtual void OnConnectionFailure(const std::string & stConnectionID);
};

#endif //GUACAMOLE_GUACCONNECTIONHANDLER_H
