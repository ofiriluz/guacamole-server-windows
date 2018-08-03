//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCONNECTION_H
#define GUACAMOLE_SERVER_SRC_GUACCONNECTION_H

#include <guacservice/IGuacConnectionSocket.h>
#include <guacservice/GuacClientProcessHandler.h>
#include <guacservice/GuacClientProcessHandlerMap.h>
#include <guacservice/IGuacConnectionNotifier.h>
#include <guacservice/GuacClientUser.h>
#include <boost/shared_ptr.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <guacamole/socket.h>
#include <guacamole/parser.h>

class GuacConnection
{
private:
   guac_parser * m_GuacParser;
   guac_client * m_GuacConnectionClient;
   IGuacConnectionSocketPtr m_ConnectionSocket;
   GuacClientProcessHandlerPtr m_ClientProcessHandler;
   IGuacConnectionNotifierPtr m_ConnectionNotifier;
   boost::thread * m_GuacConnectionThread;
   bool m_IsGuacConnectionRunning;

private:
   /**
    * Expects an instruction to arrive from the parser with the socket
    * @param stInstructionName
    * @return
    */
   bool ExpectInstruction(const std::string & stInstructionName);
   /**
    * Creates a new process handler or retrieves using the parser args
    * @param bNewProcess
    * @return
    */
   GuacClientProcessHandlerPtr GetClientProcess(bool & bNewProcess);
   /**
    * The actual connection thread that is responsible for running the client process for this connection or joining this connection to an existing process
    */
   void ConnectionThread();
   /**
    * Helper function to finish the connection when a failure happens, notifies the listener
    */
   void FinishOnFailure();

public:
   /**
    * Constructor
    * @param pNotifier
    */
   GuacConnection(const IGuacConnectionNotifierPtr & pNotifier);
   /**
    * Destructor
    */
   virtual ~GuacConnection();
   /**
    * Prepares the given guac connection socket on this connection
    * @param pSocket
    * @return
    */
   bool PrepareGuacConnection(const IGuacConnectionSocketPtr & pSocket);
   /**
    * Starts the guac connection thread
    */
   void StartGuacConnection();
   /**
    * Stops the connection and the associated connection thread
    */
   void StopGuacConnection();
   /**
    * Getter for if the connection is running
    * @return
    */
   bool IsGuacConnectionRunning();
   /**
    * Getter for the underlying socket that is associated to this connection
    * @return
    */
   IGuacConnectionSocketPtr GetUnderlyingSocket() const;
   /**
    * Getter for the client process that is associated to this connection
    * @return
    */
   GuacClientProcessHandlerPtr GetClientProcess() const;
   /**
    * Getter for the generated connection ID
    * @return
    */
   std::string GetConnectionID() const;
};

typedef boost::shared_ptr<GuacConnection> GuacConnectionSmartPtr;
typedef GuacConnection * GuacConnectionPtr;

#endif /* GUACAMOLE_SERVER_SRC_GUACCONNECTION_H */