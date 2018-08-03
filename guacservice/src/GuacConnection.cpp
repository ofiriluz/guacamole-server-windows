//
// Created by oiluz on 9/3/2017.
//

#include <guacservice/GuacConnection.h>

GuacConnection::GuacConnection(const IGuacConnectionNotifierPtr & pNotifier)
        : m_GuacParser(nullptr),
          m_GuacConnectionThread(nullptr), m_IsGuacConnectionRunning(false), m_ConnectionNotifier(pNotifier)
{
   m_GuacConnectionClient = guac_client_alloc();
}

GuacConnection::~GuacConnection()
{
   StopGuacConnection();

   if(m_GuacParser)
   {
      guac_parser_free(m_GuacParser);
      m_GuacParser = nullptr;
   }

   guac_client_free(m_GuacConnectionClient);
}

GuacClientProcessHandlerPtr GuacConnection::GetClientProcess(bool & bNewProcess)
{
   // If the identifier indicates trying to get an existing process, we first try to find it on the map
   // If it does not exist, we create a new process by the given identifier
   std::string id = m_GuacParser->argv[0];
   GuacClientProcessHandlerPtr process;

   if(id[0] == GUAC_CLIENT_ID_PREFIX)
   {
      bNewProcess = false;
      // Process exists and is already alive
      process = GuacClientProcessHandlerMap::GetInstance()->RetrieveProcessHandler(id);
   }
   else
   {
      bNewProcess = true;
      // Process will be created
      GuacLogger::GetInstance()->Debug() << "Creating new process for protocol " << id << " [" << GetConnectionID()
                                         << "]";
      process = GuacClientProcessHandlerMap::GetInstance()->CreateProcessHandler();
   }

   return process;
}

bool GuacConnection::ExpectInstruction(const std::string & stInstructionName)
{
   GuacLogger::GetInstance()->Debug() << "Expecting Instruction <" << stInstructionName << "> [" << GetConnectionID()
                                      << "]";
   if(guac_parser_expect(m_GuacParser, m_ConnectionSocket->GetGuacSocket(), GUACD_TIMEOUT, stInstructionName.c_str()))
   {
      // Reaching here means there was an instruction error
      GuacLogger::GetInstance()->Error() << "Failed Expecting Instruction <" << stInstructionName << ">";
      return false;
   }
   return true;
}

void GuacConnection::FinishOnFailure()
{
   // Finishing on another thread to not go into deadlock on destructor
   boost::thread([this]()
                 {
                    GuacLogger::GetInstance()->Error() << "Cleaning GuacConnection Due to Failure ["
                                                       << GetConnectionID() << "]";
                    m_ConnectionNotifier->OnConnectionFailure(GetConnectionID());
                 });
}

void GuacConnection::ConnectionThread()
{
   // Reallocate a fresh parser
   if(m_GuacParser)
   {
      guac_parser_free(m_GuacParser);
   }
   m_GuacParser = guac_parser_alloc();

   bool newProcess = false;

   // First we get the protocol from select instruction
   if(!ExpectInstruction("select") || m_GuacParser->argc != 1)
   {
      // Failure on getting the instruction
      GuacLogger::GetInstance()->Error() << "Could not get select instruction [" << GetConnectionID() << "]";
      FinishOnFailure();
      return;
   }

   GuacLogger::GetInstance()->Debug() << "Getting the client process [" << GetConnectionID() << "]";
   // Get a new process or an existing one depending on the instruction arguments
   m_ClientProcessHandler = GetClientProcess(newProcess);

   if(!m_ClientProcessHandler)
   {
      // Couldnt get the client process
      GuacLogger::GetInstance()->Error() << "Could not retrieve / create process for protocol " << m_GuacParser->argv[0]
                                         << " [" << GetConnectionID() << "]";
      FinishOnFailure();
      return;
   }

   m_IsGuacConnectionRunning = true;

   std::string protocolName = m_GuacParser->argv[0];

   // This is the main handler of the process, other users will only be added and nothing else
   if(newProcess)
   {
      GuacLogger::GetInstance()->Debug() << "Starting new process for protocol " << protocolName << " ["
                                         << GetConnectionID() << "]";

      // Start the process
	  if (m_ClientProcessHandler->StartProcess(m_GuacParser->argv[0]))
	  {
		  m_ClientProcessHandler->StartConnectionUser(GuacConnectionPtr(this));

		  GuacLogger::GetInstance()->Debug() << "Process Finished, Cleaning Up [" << GetConnectionID() << "]";

		  // We Finish the process, not alive anymore
		  m_ClientProcessHandler->StopProcess();
	  }
	  // Failed to start the process
	  else
	  {
		  GuacLogger::GetInstance()->Error() << "Could not Start the process " << " ["
			  << GetConnectionID() << "]";
	  }
      m_IsGuacConnectionRunning = false;

      // Remove the handler from the map, this will notify and remove all the connections that are related to the process
      GuacClientProcessHandlerPtr proc =
              GuacClientProcessHandlerMap::GetInstance()->RemoveProcessHandler(m_ClientProcessHandler->GetProcessHandlerID());

      // This is the last reference to the process so we can finally delete it
	  delete proc;
   }
   else
   {
      // Add the user to an already existing guac process
      m_ClientProcessHandler->StartConnectionUser(GuacConnectionPtr(this));

      // Notify that the connection is to be removed (Failure can also be on process ending)
      m_ConnectionNotifier->OnConnectionFailure(GetConnectionID());
   }
}

bool GuacConnection::PrepareGuacConnection(const IGuacConnectionSocketPtr & pSocket)
{
   if(m_IsGuacConnectionRunning)
   {
      return false;
   }

   GuacLogger::GetInstance()->Debug() << "Preparing Guac connection [" << GetConnectionID() << "]";
   m_ConnectionSocket = pSocket;
   return true;
}

void GuacConnection::StartGuacConnection()
{
   if(m_IsGuacConnectionRunning)
   {
      return;
   }

   GuacLogger::GetInstance()->Debug() << "Starting Guac Connection [" << GetConnectionID() << "]";

   ConnectionThread();
}

void GuacConnection::StopGuacConnection()
{
   if(!m_IsGuacConnectionRunning)
   {
      return;
   }

   GuacLogger::GetInstance()->Debug() << "Stopping Guac Connection [" << GetConnectionID() << "]";

   m_IsGuacConnectionRunning = false;

   // Wait for the connection thread to end, only if its not the same thread (should not happen)
   if(m_GuacConnectionThread && boost::this_thread::get_id() != m_GuacConnectionThread->get_id())
   {
      m_GuacConnectionThread->interrupt();
      m_GuacConnectionThread->join();
   }
   boost::system::error_code ec;
   m_ConnectionSocket->CloseSocket(ec);

   GuacLogger::GetInstance()->Debug() << "Guac Connection Stopped [" << GetConnectionID() << "]";
}

bool GuacConnection::IsGuacConnectionRunning()
{
   return m_IsGuacConnectionRunning;
}

IGuacConnectionSocketPtr GuacConnection::GetUnderlyingSocket() const
{
   return m_ConnectionSocket;
}

GuacClientProcessHandlerPtr GuacConnection::GetClientProcess() const
{
   return m_ClientProcessHandler;
}

std::string GuacConnection::GetConnectionID() const
{
   return m_GuacConnectionClient->connection_id;
}