//
// Created by oiluz on 9/4/2017.
//

#include <guacservice/GuacClientProcessSocketedHandler.h>
#include <guacservice/GuacConnection.h>
#include <guacservice/GuacClientProcessHandlerMap.h>

GuacClientProcessSocketedHandler::GuacClientProcessSocketedHandler()
   : m_bIsProcessRunning(false), m_ClientChildProcess(nullptr), m_ShmSocket(nullptr)
{
   m_GuacClient = guac_client_alloc();
}

GuacClientProcessSocketedHandler::~GuacClientProcessSocketedHandler()
{
   StopProcess();

   // Cleanup the shared memory if created
   if (m_ShmSocket)
   {
      guac_socket_free(m_ShmSocket);
   }

   guac_client_free(m_GuacClient);
}

std::string GuacClientProcessSocketedHandler::GenerateSharedMemoryTag() const
{
   std::string shared_memory_name = USER_SHARED_MEMORY_PREFIX;

   // Generate a random UUID
   shared_memory_name += guac_generate_id('_');

   return shared_memory_name;
}

void GuacClientProcessSocketedHandler::UserThread(const GuacConnectionPtr & pConnectionUser, bool bOwner)
{
   guac_socket * tcp_socket = pConnectionUser->GetUnderlyingSocket()->GetGuacSocket();

   // Failure to prepare the guac sockets
   if (!tcp_socket)
   {
      GuacLogger::GetInstance()->Error() << "Could not create user shared memory, aborting user ["
         << GetProcessHandlerID() << "]";
      return;
   }

   // Add the instruction that this is to add a user to the running process
   std::string command = std::string(ADD_USER_COMMAND) + COMMAND_DELIMITER + shared_memory_name;

   // Write the shared memory tag allocated for this user
   // This will notify the child process that a new user has asked to join
   guac_socket_write(m_ShmSocket, command.c_str(), command.size());

   if (WSADuplicateSocket(pConnectionUser->GetUnderlyingSocket()->Get,
      piChild.dwProcessId,
      &ProtocolInfo) == SOCKET_ERROR)
   {

   }

   // Wait forever until the process is dead 
   m_ClientChildProcess->wait();

   // Wait for the read thread to finish
   // read_thread.interrupt();
   // read_thread.join();

   // Cleanup all the user related IO objects
   CleanupUserIO(pConnectionUser, bOwner);
}

void
GuacClientProcessSocketedHandler::CleanupUserIO(const GuacConnectionPtr & pConnectionUser, bool bOwner)
{
   // Check if the process is still running first, this means that the connection was closed
   // Note that if the process was closed, then the main GuacConnection will call StopProcess
   if (m_ClientChildProcess->running())
   {
      // This means that the connection was closed, we need to notify the process
      // If this is the owner user, we need to stop the process completely
      if (bOwner)
      {
         GuacLogger::GetInstance()->Debug() << "Stopping Process Since Owner Died [" << GetProcessHandlerID() << "]";
         StopProcess();
      }
      else
      {
         // Remove the connection from the vector since it is not alive anymore and send the process to remove the connection
         // This is done on a different thread to avoid deadlocks of removal
         // It only needs the shared memory name to construct the command
         GuacLogger::GetInstance()->Debug() << "Removing Dead User [" << pConnectionUser->GetConnectionID() << "]";
         boost::thread([this, pConnectionUser]()
         {
            boost::mutex::scoped_lock lock(m_ConnectionsMutex);

            // Erase the connection from the vector
            for (size_t i = 0; i < m_vecConnections.size(); i++)
            {
               if (m_vecConnections[i] == pConnectionUser)
               {
                  m_vecConnections.erase(m_vecConnections.begin() + i);
                  break;
               }
            }
         });
      }
   }
}

bool GuacClientProcessSocketedHandler::SendClientParams()
{
   // Write the protocol needed params to the client process
   // - Log Folder Path
   // - Protocol Name
   // - Libraries Folder Path
   // - FPS
   try
   {
      GuacLogger::GetInstance()->Debug() << "Child Process Started, Writing protocol to child ["
         << GetProcessHandlerID() << "]";
      GuacLogger::GetInstance()->Debug() << "Protocol is " << m_stProtocolName << " [" << GetProcessHandlerID() << "]";
      GuacLogger::GetInstance()->Debug() << "Writing LogOutput Folder" << " [" << GetProcessHandlerID() << "]";
      // If config says that there is no file log, then no log will happen on the child process
      if (m_Config.IsWithFileLog())
      {
         guac_socket_write(m_ShmSocket, m_Config.GetGuacLogOutputFolder().c_str(),
            m_Config.GetGuacLogOutputFolder().size());
      }
      else
      {
         guac_socket_write(m_ShmSocket, std::string("NO_LOG").c_str(),
            m_Config.GetGuacLogOutputFolder().size());
      }

      GuacLogger::GetInstance()->Debug() << "Writing Protocol Name" << " [" << GetProcessHandlerID() << "]";
      guac_socket_write(m_ShmSocket, m_stProtocolName.c_str(), m_stProtocolName.size());

      GuacLogger::GetInstance()->Debug() << "Writing Libraries Folder" << " [" << GetProcessHandlerID() << "]";
      guac_socket_write(m_ShmSocket, m_Config.GetGuacProtocolsLibrariesFolder().c_str(),
         m_Config.GetGuacProtocolsLibrariesFolder().size());

      GuacLogger::GetInstance()->Debug() << "Writing FPS" << " [" << GetProcessHandlerID() << "]";
      guac_socket_write(m_ShmSocket, std::to_string(m_Config.GetFPS()).c_str(), std::to_string(m_Config.GetFPS()).size());
   }
   catch (...)
   {
      GuacLogger::GetInstance()->Error() << "Could not write to child process [" << GetProcessHandlerID() << "]";
      m_ClientChildProcess->terminate();
      m_ClientChildProcess->wait();
      delete m_ClientChildProcess;
      m_ClientChildProcess = nullptr;
      return false;
   }

   return true;
}

bool GuacClientProcessSocketedHandler::RunProcess()
{
   GuacLogger::GetInstance()->Debug() << "Starting child process for protocol " << m_stProtocolName << " ["
      << GetProcessHandlerID() << "]";

   // Open the process and the associated PID shared memory
   try
   {
      // Pass the current env to the child process
      boost::process::environment env = boost::this_process::environment();
      m_ClientChildProcess = new boost::process::child(boost::filesystem::path(m_Config.GetGuacServiceClientProcessPath()),
         env);

      // Create the shared memory of this process commands by the child process ID
      m_ShmSocket = guac_socket_shared_memory_socket_create(
         std::string(GLOBAL_SHARED_MEMORY_NAME) + "_" + std::to_string(m_ClientChildProcess->id()), false, false,
         GUAC_SHARED_MEMORY_GLOBAL_QUEUE_SIZE, GUAC_SHARED_MEMORY_GLOBAL_PACKET_SIZE);

      // Failure to create the shared memory, terminate the process
      if (!m_ShmSocket)
      {
         GuacLogger::GetInstance()->Error() << "Could not create shared memory socket [" << GetProcessHandlerID()
            << "]";
         m_ClientChildProcess->terminate();
         m_ClientChildProcess->join();
         return false;
      }
   }
   catch (boost::process::process_error & err)
   {
      GuacLogger::GetInstance()->Error() << "Could not create child process [" << GetProcessHandlerID() << "]";
      GuacLogger::GetInstance()->Error() << err.what();
      return false;
   }

   // Process failed to run, cleanup
   if (!m_ClientChildProcess->running())
   {
      GuacLogger::GetInstance()->Error() << "Could not create child process [" << GetProcessHandlerID() << "]";
      delete m_ClientChildProcess;
      m_ClientChildProcess = nullptr;
      return false;
   }

   return SendClientParams();
}

bool GuacClientProcessSocketedHandler::StartProcess(const std::string & stProtocolName)
{
   // Close an existing socket just incase
   if (m_ShmSocket)
   {
      guac_socket_free(m_ShmSocket);
      m_ShmSocket = nullptr;
   }

   // Save the info
   m_stProtocolName = stProtocolName;

   // Run the process
   GuacLogger::GetInstance()->Debug() << "Starting Process Thread [" << GetProcessHandlerID() << "]";
   bool result = RunProcess();

   // Process has been succesfully created
   if (result)
   {
      m_bIsProcessRunning = true;
   }

   return result;
}

bool GuacClientProcessSocketedHandler::StartConnectionUser(const GuacConnectionPtr & pConnectionUser)
{
   // Start the IO Thread for this user
   GuacLogger::GetInstance()->Debug() << "Starting IO Thread [" << GetProcessHandlerID() << "]["
      << pConnectionUser->GetConnectionID() << "]";

   // The owner is the first actual connection of this process
   bool owner = m_vecConnections.size() == 0;

   // Save the connection with the thread
   m_vecConnections.push_back(pConnectionUser);

   // Start the user IO 
   UserThread(pConnectionUser, owner);

   return true;
}

bool GuacClientProcessSocketedHandler::IsProcessRunning() const
{
   return m_bIsProcessRunning;
}

bool GuacClientProcessSocketedHandler::StopProcess()
{
   if (m_bIsProcessRunning)
   {
      GuacLogger::GetInstance()->Debug() << "Stopping Process  [" << GetProcessHandlerID() << "]";
      m_bIsProcessRunning = false;

      // Stop the actual process
      if (m_ClientChildProcess && m_ClientChildProcess->running())
      {
         std::error_code ec;
         // Send stop command and wait for process to end
         GuacLogger::GetInstance()->Debug() << "Sending STOP to child process [" << GetProcessHandlerID() << "]";
         std::string command = std::string(STOP_PROCESS_COMMAND) + COMMAND_DELIMITER;
         guac_socket_write(m_ShmSocket, command.c_str(), command.size());

         // Wait for the child process to end after the stop command and clean it
         GuacLogger::GetInstance()->Debug() << "Waiting for child process to end [" << GetProcessHandlerID() << "]";
         m_ClientChildProcess->wait(ec);
         delete m_ClientChildProcess;
         m_ClientChildProcess = nullptr;
      }

      // If the process stopped this means that we also need to stop and remove all the connections
      // The process is not responsible for terminating the connections, only the connection handler is
      GuacLogger::GetInstance()->Debug() << "Closing all connections to process [" << GetProcessHandlerID() << "]";
      boost::thread([&]()
      {
         m_vecConnections.clear();

         GuacLogger::GetInstance()->Debug() << "Finished Stopping Process [" << GetProcessHandlerID()
            << "]";
      });
   }

   return true;
}

std::string GuacClientProcessSocketedHandler::GetClientProtocol() const
{
   return m_stProtocolName;
}

std::string GuacClientProcessSocketedHandler::GetProcessHandlerID() const
{
   return m_GuacClient->connection_id;
}

void GuacClientProcessSocketedHandler::SetGuacConfig(const GuacConfig & rConfig)
{
   m_Config = rConfig;
}