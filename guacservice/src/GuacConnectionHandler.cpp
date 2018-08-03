//
// Created by oiluz on 9/3/2017.
//

#include <guacservice/GuacConnectionHandler.h>

GuacConnectionHandler::GuacConnectionHandler(const GuacConfig & rConfig)
        : m_Config(rConfig)
{
   // Register the handler on the process map
   GuacClientProcessHandlerMap::GetInstance()->RegisterMapObserver(IGuacClientProcessHandlerMapObserverPtr(this));
}

bool GuacConnectionHandler::HandleNewConnection(const IGuacConnectionSocketPtr & pClientConnection)
{
   // Check that the socket does not exist
   {
      boost::mutex::scoped_lock lock(m_GuacConnectionsMutex);
      for(auto && guacConnection : m_GuacConnections)
      {
         if(guacConnection->GetUnderlyingSocket() == pClientConnection)
         {
            return false;
         }
      }
   }

   GuacLogger::GetInstance()->Debug() << "Handling new connection";
   // Create the connection
   GuacConnectionSmartPtr connection(new GuacConnection(this));

   if(connection->PrepareGuacConnection(pClientConnection))
   {
      // If we managed to prepare the guac connection, add it to the list and start it
      {
         boost::mutex::scoped_lock lock(m_GuacConnectionsMutex);
         m_GuacConnections.push_back(connection);
      }
      // Will start the guac connection, create / use process and add the user to it
      connection->StartGuacConnection();

      return true;
   }

   return false;
}

void GuacConnectionHandler::CloseConnections()
{
   // Closes all the connections, will evantually also close all the processes
   for(auto && conn : m_GuacConnections)
   {
      conn->StopGuacConnection();
   }
   m_GuacConnections.clear();
}

void GuacConnectionHandler::OnClientProcessCreated(const std::string & stID)
{
   // When a process is created, we add the config to it
   GuacClientProcessHandlerPtr handler = GuacClientProcessHandlerMap::GetInstance()->RetrieveProcessHandler(stID);
   if(handler)
   {
      handler->SetGuacConfig(m_Config);
   }
}

void GuacConnectionHandler::OnClientProcessRemoved(const std::string & stID)
{
   boost::mutex::scoped_lock lock(m_GuacConnectionsMutex);

   // When the process ends either from here or from another place we remove all the connections from the connections vector
   int index = 0;

   while(index != m_GuacConnections.size())
   {
	   if (!m_GuacConnections[index]->GetClientProcess() ||
		   m_GuacConnections[index]->GetClientProcess()->GetProcessHandlerID() == stID)
      {
         //boost::mutex::scoped_lock lock(m_GuacConnectionsMutex);

         GuacConnectionSmartPtr conn = m_GuacConnections[index];

         m_GuacConnections.erase(m_GuacConnections.begin() + index);

         // Send to another thread for safe cleanup
         // This is due to the posbillity of the deletion coming from the connection thread itself
         // Pass the connection since the local var will die so we cant pass it on the scope
         boost::thread([](GuacConnectionSmartPtr & connection)
                       {
                          // Cleanup Connection
                          connection->StopGuacConnection();

                          // Explicit reset to the pointer which will delete the guac connection due to it being the last reference
                          connection.reset();

                       }, conn);

         // Update the position
         if(index > 0)
         {
            index--;
         }
      }
      else
      {
         index++;
      }
   }
   GuacLogger::GetInstance()->Debug() << "Finished Removing all process related connections";
}

void GuacConnectionHandler::OnConnectionFailure(const std::string & stConnectionID)
{
   boost::mutex::scoped_lock lock(m_GuacConnectionsMutex);

   // On connection failure we remove the connection and clean it up
   for(size_t i = 0; i < m_GuacConnections.size(); i++)
   {
      if(m_GuacConnections[i]->GetConnectionID() == stConnectionID)
      {
         // Cleanup Connection
         m_GuacConnections[i]->StopGuacConnection();

         // Remove the connection from the list
         m_GuacConnections.erase(m_GuacConnections.begin() + i);

         break;
      }
   }
}