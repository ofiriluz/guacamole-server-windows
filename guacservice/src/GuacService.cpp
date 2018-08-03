//
// Created by oiluz on 9/3/2017.
//

#include <guacservice/GuacService.h>

GuacService::GuacService() : m_IsServiceRunning(false), m_ConnectionsHandler(nullptr)
{
   m_Acceptor = new boost::asio::ip::tcp::acceptor(m_IOService);
}

GuacService::~GuacService()
{
   CleanupService();
   delete m_Acceptor;
   delete m_ConnectionsHandler;
}

bool GuacService::BindAcceptor()
{
   boost::system::error_code ec;
   // First open the acceptor for v4 listening
   m_Acceptor->open(boost::asio::ip::tcp::v4(), ec);
   if(ec)
   {
	  GuacLogger::GetInstance()->Debug() << "Could not open Acceptor For Listening";
      return false;
   }

   // Create a resolver for resolving the host name and query it
   boost::asio::ip::tcp::resolver resolver(m_IOService);
   boost::asio::ip::tcp::resolver::iterator endpoint_iter =
           resolver.resolve(boost::asio::ip::tcp::resolver::query(
                   m_Config.GetBindHost(), boost::lexical_cast<std::string>(m_Config.GetBindPort())), ec);

   if (ec)
   {
	  GuacLogger::GetInstance()->Debug() << "Could not find endpoint";
	  return false;
   }

   boost::asio::ip::tcp::resolver::iterator end;
   ec = boost::asio::error::host_not_found;

   // If there is no hostname, bind to all addresses
   if(m_Config.GetBindHost() == "")
   {
      m_Acceptor->bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
                                                      m_Config.GetBindPort()), ec);
	  if(ec)
	  {
		 // Reaching here means we could not bind to any host
		 GuacLogger::GetInstance()->Debug() << "Could not bind to endpoint";
		 return false;
	  }
   }
   else
   {
      // Iterate over the resolved hosts until we bind to one successfully
      while(ec && endpoint_iter != end)
      {
         m_Acceptor->bind(*endpoint_iter++, ec);
      }
      if(ec)
      {
         // Reaching here means we could not bind to any host
		 GuacLogger::GetInstance()->Debug() << "Could not find endpoint to bind to";
         return false;
      }
   }
   GuacLogger::GetInstance()->Debug() << "Endpoint is : "
                                      << m_Acceptor->local_endpoint().address().to_string() << ":"
                                      << m_Acceptor->local_endpoint().port();
   // Successfully binded to a host
   return true;
}

void GuacService::CleanupService()
{
   // Close all the handler connections
   m_ConnectionsHandler->CloseConnections();

   // Close the service
   m_IsServiceRunning = false;
}

void GuacService::ListenAndAcceptNewConnections()
{
   GuacLogger::GetInstance()->Debug() << "Starting to listen to connections";
   if(m_Config.IsWithSSL())
   {
      GuacLogger::GetInstance()->Debug() << "Using SSL";
   }

   while(m_IsServiceRunning)
   {
      boost::system::error_code ec;

      // Get the connection depending on the type
      auto newConnectionSocket =
              m_Config.IsWithSSL() ?
              IGuacConnectionSocketPtr(
                      new GuacConnectionSSLSocket(m_Config.GetSSLPEMFilePath(), m_Config.GetSSLCertFilePath(),
                                                  m_Config.GetSSLDiffieHellmanPEMFilePath()))
                                   :
              IGuacConnectionSocketPtr(new GuacConnectionTCPSocket());

      // Start waiting for a connection, this will block 
      newConnectionSocket->WaitForSocket(m_Acceptor, ec);

      GuacLogger::GetInstance()->Debug() << "New connection accepted";

      // Send this to another thread to be added to the connections handler and start his connection thread
      // Pass the connection as a param due to the loop releasing the reference to it
      if(!ec)
      {
         boost::thread([this](const IGuacConnectionSocketPtr & connection)
                       {
                          boost::system::error_code establishEc;

                          GuacLogger::GetInstance()->Debug() << "Trying to establish connection";
                          // Establish the connection, this will establish needed objects to begin the connection
                          // Notice that this might block if this is SSL with the handshake
                          connection->EstablishSocket(establishEc);

                          if(!establishEc)
                          {
                             GuacLogger::GetInstance()->Debug() << "Connection finished establishing.";

                             // Add new connection to the guac handler
                             m_ConnectionsHandler->HandleNewConnection(connection);
                          }
                          else
                          {
                             GuacLogger::GetInstance()->Error() << "Establish socket failed - "
                                                                << establishEc.message();
                          }

                       }, newConnectionSocket);
      }
      else
      {
         GuacLogger::GetInstance()->Error() << "Connection could not be accepted, server issue, aborting";
         break;
      }
   }
}

bool GuacService::StartGuacamoleService(const GuacConfig & rConfig)
{
   boost::system::error_code ec;

   // Service is already running
   if(m_IsServiceRunning)
   {
      return false;
   }

   // Init log
   GuacLogger::GetInstance()->InitializeLog(rConfig.GetGuacLogOutputFolder(),
                                            "GuacamoleService",
                                            rConfig.GetMaxLogLevel(),
                                            rConfig.IsWithConsoleLog(),
                                            rConfig.IsWithFileLog());

   GuacLogger::GetInstance()->Debug() << "Starting Server.";
   if(rConfig.IsWithFileLog())
   {
      GuacLogger::GetInstance()->Debug() << "Log Files will be saved to " << rConfig.GetGuacLogOutputFolder();
   }

   m_Config = rConfig;

   if(m_ConnectionsHandler)
   {
      delete m_ConnectionsHandler;
   }

   m_ConnectionsHandler = new GuacConnectionHandler(m_Config);

   GuacLogger::GetInstance()->Debug() << "Starting Guac Service";

   // Bind ourselves to the given host name / port on the config
   if(!BindAcceptor())
   {
      GuacLogger::GetInstance()->Error() << "Could not bind to any endpoint";
      return false;
   }

   // Change the acceptor state to listening
   m_Acceptor->listen(MAX_GUAC_SERVICE_BACKLOG, ec);

   if(ec)
   {
      GuacLogger::GetInstance()->Error() << "Could not listen from the acceptor " << ec.message();
      CleanupService();
      return false;
   }

   // Start the accepting cycle, sync
   m_IsServiceRunning = true;
   ListenAndAcceptNewConnections();

   // If the service ends, we cleanup resources
   CleanupService();

   return true;
}

bool GuacService::IsServiceRunning() const
{
   return m_IsServiceRunning;
}

