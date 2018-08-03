//
// Created by oiluz on 9/4/2017.
//

#include <guacservice/GuacClientProcess.h>
#include <chrono>
#include <thread>
using namespace std::chrono_literals;
void GuacLogHandler(guac_client * client, guac_client_log_level level, const char * format, va_list args)
{
   char message[2048];

   vsnprintf(message, sizeof(message), format, args);
   switch (level)
   {
   case GUAC_LOG_DEBUG:
	   GuacLogger::GetInstance()->Debug() << "GuacLog = " << std::string(message);
	   break;
   case GUAC_LOG_INFO:
	   GuacLogger::GetInstance()->Info() << "GuacLog = " << std::string(message);
	   break;
   case GUAC_LOG_ERROR:
	   GuacLogger::GetInstance()->Error() << "GuacLog = " << std::string(message);
	   break;
   case GUAC_LOG_WARNING:
	   GuacLogger::GetInstance()->Warning() << "GuacLog = " << std::string(message);
	   break;
   }
}

GuacClientProcess::GuacClientProcess() : m_Client(nullptr), m_bIsProcessRunning(false)
{
   // Joins the existing shared memory by the process id
   // Busy wait for 10 seconds to try and join the shared memory
   static const std::size_t SHARED_MEMORY_WAIT_COUNT = 10;
   static const std::size_t SHARED_MEMORY_WAIT_DELAY = 314;

   int count = SHARED_MEMORY_WAIT_COUNT;
   while (count > 0)
   {
      m_ShmSocket = guac_socket_shared_memory_socket_join(
         std::string(GLOBAL_SHARED_MEMORY_NAME) + "_" + std::to_string(boost::this_process::get_id()), false, false);
      if (m_ShmSocket)
      {
         break;
      }

      boost::this_thread::sleep(boost::posix_time::milliseconds(SHARED_MEMORY_WAIT_DELAY));
      count--;
   }
   if (!m_ShmSocket)
   {
      exit(1);
   }
}

std::string GuacClientProcess::ReadLoggerPath()
{
   GuacLogger::GetInstance()->Debug() << "Trying to read logger path [" << m_Client->connection_id << "]";

   char buffer[LOGGER_FOLDER_BUFFER_MAX_LEN];
   std::string loggerFolder;

   // Select first to see if data is ready
   if(guac_socket_select(m_ShmSocket, SHM_SELECT_MS))
   {
      GuacLogger::GetInstance()->Debug() << "Select Popped, Reading [" << m_Client->connection_id << "]";
      // Read the next data
      size_t size = guac_socket_read(m_ShmSocket, buffer, LOGGER_FOLDER_BUFFER_MAX_LEN);
      if(size > 0)
      {
         loggerFolder = std::string(buffer, buffer + size);
         GuacLogger::GetInstance()->Debug() << "Read Logger Path : " << loggerFolder << " [" << m_Client->connection_id
                                            << "]";
         return loggerFolder;
      }
   }
   else
   {
      GuacLogger::GetInstance()->Error() << "Select Timeout on protocol read [" << m_Client->connection_id << "]";
   }
   return "";
}

std::string GuacClientProcess::ReadProtocol()
{
   GuacLogger::GetInstance()->Debug() << "Trying to read protocol type [" << m_Client->connection_id << "]";

   char buffer[PROTOCOL_BUFFER_MAX_LEN];
   std::string protocol;

   // Select first to see if data is ready
   if(guac_socket_select(m_ShmSocket, SHM_SELECT_MS))
   {
      GuacLogger::GetInstance()->Debug() << "Select Popped, Reading [" << m_Client->connection_id << "]";
      // Read the next data
      size_t size = guac_socket_read(m_ShmSocket, buffer, PROTOCOL_BUFFER_MAX_LEN);
      if(size > 0)
      {
         protocol = std::string(buffer, buffer + size);
         GuacLogger::GetInstance()->Debug() << "Read Protocol : " << protocol << " [" << m_Client->connection_id << "]";
         return protocol;
      }
   }
   else
   {
      GuacLogger::GetInstance()->Error() << "Select Timeout on protocol read [" << m_Client->connection_id << "]";
   }
   return "";
}

std::string GuacClientProcess::ReadLibraryPath()
{
   GuacLogger::GetInstance()->Debug() << "Trying to read libraries folder [" << m_Client->connection_id << "]";

   char buffer[LIBRARY_FOLDER_BUFFER_MAX_LEN];
   std::string lib;
   // Select first to see if data is ready
   if(guac_socket_select(m_ShmSocket, SHM_SELECT_MS))
   {
      GuacLogger::GetInstance()->Debug() << "Select Popped, Reading [" << m_Client->connection_id << "]";
      // Read the next data
      size_t size = guac_socket_read(m_ShmSocket, buffer, LIBRARY_FOLDER_BUFFER_MAX_LEN);
      if(size > 0)
      {
         lib = std::string(buffer, buffer + size);
         GuacLogger::GetInstance()->Debug() << "Read Lib Successfully" << " [" << m_Client->connection_id << "]";
         return lib;
      }
   }
   else
   {
      GuacLogger::GetInstance()->Error() << "Select Timeout on library path read [" << m_Client->connection_id << "]";
   }
   return "";
}

short GuacClientProcess::ReadFPS()
{
   GuacLogger::GetInstance()->Debug() << "Trying to read FPS [" << m_Client->connection_id << "]";

   char buffer[FPS_BUFFER_MAX_LEN];
   short fps;
   // Select first to see if data is ready
   if(guac_socket_select(m_ShmSocket, SHM_SELECT_MS))
   {
      GuacLogger::GetInstance()->Debug() << "Select Popped, Reading [" << m_Client->connection_id << "]";
      // Read the next data
      size_t size = guac_socket_read(m_ShmSocket, buffer, FPS_BUFFER_MAX_LEN);
      if(size > 0)
      {
		 fps = boost::lexical_cast<short>(std::string(buffer, buffer + size));
         GuacLogger::GetInstance()->Debug() << "Read FPS : " << fps << " [" << m_Client->connection_id << "]";
         return fps;
      }
   }
   else
   {
      GuacLogger::GetInstance()->Error() << "Select Timeout on FPS read [" << m_Client->connection_id << "]";
   }
   return -1;
}

std::string GuacClientProcess::ReadProtocolPath()
{
   // Wait for the protocol type to arrive from the parent process
   std::string protocol = ReadProtocol();
   if(protocol == "")
   {
      GuacLogger::GetInstance()->Error() << "Could not read protocol [" << m_Client->connection_id << "]";
      guac_client_free(m_Client);
      exit(1);
   }

   // Wait for library path to arrive from the parent process
   std::string libraryPath = ReadLibraryPath();

   if(libraryPath == "")
   {
      GuacLogger::GetInstance()->Error() << "Could not read library path [" << m_Client->connection_id << "]";

      guac_client_free(m_Client);
      exit(1);
   }

   GuacLogger::GetInstance()->Debug() << "Trying to load plugin " << protocol << " [" << m_Client->connection_id << "]";
   std::string protocolPath = StitchProtocolPath(libraryPath, protocol);

   return protocolPath;
}

std::string
GuacClientProcess::StitchProtocolPath(const std::string & stLibraryFolder, const std::string & stProtocolName) const
{
   return stLibraryFolder + "/" + stProtocolName + "." + std::string(LIB_EXTENSION);
}

void GuacClientProcess::LoadPlugin(const std::string & protocolPath)
{
   int rc = guac_client_load_plugin(m_Client, protocolPath.c_str());
   if(rc < 0)
   {
      GuacLogger::GetInstance()->Error() << "Failed Loading Plugin [" << m_Client->connection_id << "]";
      // Log error
      if(rc == -1)
      {
         GuacLogger::GetInstance()->Error() << "Could not find plugin [" << m_Client->connection_id << "]";
      }
      else if(rc == -2)
      {
         GuacLogger::GetInstance()->Error() << "Unknown Plugin error [" << m_Client->connection_id << "]";
      }
      GuacLogger::GetInstance()->Error() << guac_error_message;
      guac_client_free(m_Client);
      exit(1);
   }
   else
   {
      GuacLogger::GetInstance()->Debug() << "Finished Loading Plugin [" << m_Client->connection_id << "]";
   }
}

void GuacClientProcess::InitLoggerFromParent()
{
   std::string loggerPath = ReadLoggerPath();

   if(loggerPath == "")
   {
      guac_client_free(m_Client);
      exit(1);
   }
   GuacLogger::GetInstance()->InitializeLog(loggerPath,
                                            "GuacServiceProcess_" + m_Client->connection_id,
                                            boost::log::trivial::debug,
                                            false, !(loggerPath == "NO_LOG"));
}

void GuacClientProcess::AddUser(const std::string & stSharedMemoryTag, bool bOwner)
{
   // Create the user   
   guac_user * user = guac_user_alloc();
   user->client = m_Client;
   user->owner = bOwner;

   // Create and store the user
   GuacClientUserPtr clientUser(new GuacClientUser(user, stSharedMemoryTag));

   // Run the user, Blocked untill the user finishes
   GuacLogger::GetInstance()->Debug() << "Running User On Client " << m_Client->connection_id;
   boost::shared_ptr<boost::thread> userThread(new boost::thread([this, clientUser, user]()
                                                                 {
                                                                    clientUser->RunUser();

                                                                    GuacLogger::GetInstance()->Debug()
                                                                            << "User Done Running, Freeing ["
                                                                            << m_Client->connection_id;
                                                                    // Free the user
                                                                    guac_user_free(user);

                                                                    // Remove the user from the vector
                                                                    // Shared ptr so will be deleted at the end of this method
                                                                    for(size_t i = 0; i < m_ClientUsers.size(); i++)
                                                                    {
                                                                       if(std::get<0>(m_ClientUsers[i]).get() ==
                                                                          clientUser.get())
                                                                       {
                                                                          GuacLogger::GetInstance()->Debug()
                                                                                  << "Removing User From List ["
                                                                                  << m_Client->connection_id;
                                                                          boost::mutex::scoped_lock lock(
                                                                                  m_ClientUsersMutex);
                                                                          m_ClientUsers.erase(
                                                                                  m_ClientUsers.begin() + i);
                                                                          break;
                                                                       }
                                                                    }

                                                                    GuacLogger::GetInstance()->Debug()
                                                                            << "Done Releasing User";
                                                                 }));

   m_ClientUsers.push_back(std::make_tuple(clientUser, userThread));
}

void GuacClientProcess::RemoveUser(const std::string & stSharedMemoryTag)
{
   // Find the user
   for(auto && user : m_ClientUsers)
   {
      if(std::get<0>(user)->GetUserTag() == stSharedMemoryTag)
      {
         // Stop the user, will be removed by the user thread
         std::get<0>(user)->AbortUser();
         std::get<1>(user)->join();
         break;
      }
   }
}

std::string GuacClientProcess::WaitForCommand()
{
   // Select and read command
   //if(guac_socket_select(m_ShmSocket, 3000))
   {
      char buffer[SOCKET_BUFFER_SIZE];
      size_t size = guac_socket_read(m_ShmSocket, buffer, SOCKET_BUFFER_SIZE);

      if(size > 0)
      {
         std::string command = std::string(buffer, buffer + size);
         boost::trim(command);
         return command;
      }
   }
   return "";
}

void GuacClientProcess::HandleCommand(const std::string & stCommand)
{
   if(stCommand == "")
   {
      return;
   }

   GuacLogger::GetInstance()->Debug() << "Got New Command : " << stCommand << " [" << m_Client->connection_id << "]";

   std::vector<std::string> command_args;
   boost::split(command_args, stCommand, boost::is_any_of(std::string(COMMAND_DELIMITER)));

   if(command_args[0] == ADD_USER_COMMAND)
   {
      GuacLogger::GetInstance()->Debug() << "Adding New User - " << command_args[1] << " [" << m_Client->connection_id
                                         << "]";
      AddUser(command_args[1], m_ClientUsers.size() == 0);
   }
   else if(command_args[0] == REMOVE_USER_COMMAND)
   {
      GuacLogger::GetInstance()->Debug() << "Removing User - " << command_args[1] << " [" << m_Client->connection_id
                                         << "]";
      RemoveUser(command_args[1]);
   }
   else if(command_args[0] == STOP_PROCESS_COMMAND)
   {
      GuacLogger::GetInstance()->Debug() << "Stopping Process [" << m_Client->connection_id << "]";

	  // TODO: Temporary patch, please fix me
	  exit(1);

      // Stop all the users 
      for(auto && user : m_ClientUsers)
      {
         std::get<0>(user)->AbortUser();
         std::get<1>(user)->join();
      }
      GuacLogger::GetInstance()->Debug() << "All Users Stopped [" << m_Client->connection_id << "]";

      // Stop the running main
      m_bIsProcessRunning = false;
   }
}

void GuacClientProcess::StartClientProcess()
{
   // Create the client
   m_Client = guac_client_alloc();
   m_Client->log_handler = GuacLogHandler;

   try
   {
	   // Initialize the logger from parent
	   InitLoggerFromParent();

	   GuacLogger::GetInstance()->Debug() << "Initializing client process [" << m_Client->connection_id << "]";

	   // Read the protocol full path from the parent process
	   std::string protocolPath = ReadProtocolPath();

	   // Load the given protocol plugin
	   LoadPlugin(protocolPath);

	   // Read the FPS from the parent process
	   short fps = ReadFPS();

	   // Failed reading FPS, aborting
	   if (fps == -1)
	   {
		   guac_client_free(m_Client);
		   exit(1);
	   }

	   // Set the FPS on the client
	   m_Client->client_fps = fps;

	   m_bIsProcessRunning = true;

	   // This is the main thread, we will wait for new users notification from the parent process here
	   // Each user will run on a different thread with the ID given from the parent as the shared memory tag
	   GuacLogger::GetInstance()->Debug() << "Running Commands Loop [" << m_Client->connection_id << "]";
	   // Start the waiting loop
	   while (m_bIsProcessRunning)
	   {
		   std::string cmd = WaitForCommand();
		   HandleCommand(cmd);
		   std::this_thread::sleep_for(1000ms);
	   }
   }
   catch (...)
   {
	   GuacLogger::GetInstance()->Error() << "Internal error occured.";
   }
   GuacLogger::GetInstance()->Debug() << "Command Loop Over, Finishing process [" << m_Client->connection_id << "]";

   // TODO: Temporary patch, please fix me
   exit(1);

   // Stop the client and free it   
   GuacLogger::GetInstance()->Debug() << "Stopping Client [" << m_Client->connection_id << "]";
   guac_client_stop(m_Client);
   GuacLogger::GetInstance()->Debug() << "Freeing Client [" << m_Client->connection_id << "]";
   guac_client_free(m_Client);
}