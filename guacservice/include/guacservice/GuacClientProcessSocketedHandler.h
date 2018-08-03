#pragma once
//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESS_H
#define GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESS_H

#include <boost/shared_ptr.hpp>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <guacamole/client.h>
#include <guacamole/id.h>
#include <iostream>
#include <cstddef>
#include <guacservice/GuacDefines.h>
#include <guacservice/GuacLogger.h>
#include <guacservice/GuacConfig.h>
#include <regex>

class GuacConnection;

typedef GuacConnection * GuacConnectionPtr;

//struct GuacConnectionUser
//{
//   GuacConnectionPtr userConnection;
//   boost::shared_ptr<boost::thread> userThread;
//   boost::mutex userMutex;
//   boost::condition_variable userCond;
//};

class GuacClientProcessSocketedHandler
{
private:
   boost::process::child * m_ClientChildProcess;
   boost::mutex m_ConnectionsMutex;
   std::string m_stProtocolName;
   GuacConfig m_Config;
   bool m_bIsProcessRunning;
   std::vector<GuacConnectionPtr> m_vecConnections;
   guac_client * m_GuacClient;
   guac_socket * m_ShmSocket;

private:
   /**
   * Generates a random shared memory tag for the actual read write pipe communication between the child process and the socket
   * @return
   */
   std::string GenerateSharedMemoryTag() const;
   /**
   * The actual IOThread that runs, handles both read and write redirection on seperate threads and waits for them to finish
   * Cleans up everything at the end
   * @param pConnectionUser
   * @param bOwner
   */
   void UserThread(const GuacConnectionPtr & pConnectionUser, bool bOwner);
   /**
   * Cleans up all the needed objects that are related to the user that was running the IOThreads
   * @param pConnectionUser
   * @param bOwner
   * @param pUserShm
   * @param stSharedMemoryName
   */
   void CleanupUserIO(const GuacConnectionPtr & pConnectionUser, bool bOwner);
   /**
   * Runs the process, does not block, only starts the process which will wait for new users to join
   * @return
   */
   bool RunProcess();
   /**
   * Sends the client needed parameters to begin execution
   */
   bool SendClientParams();

public:
   /**
   * Constructor
   */
   GuacClientProcessSocketedHandler();
   /**
   * Destructor
   */
   virtual ~GuacClientProcessSocketedHandler();
   /**
   * Starts the process for a given protocol name, calls RunProcess
   * @param stProtocolName
   * @return
   */
   bool StartProcess(const std::string & stProtocolName);
   /**
   * Adds a new connection user to the process, begins its associated IOThread
   * @param pConnectionUser
   * @return
   */
   bool StartConnectionUser(const GuacConnectionPtr & pConnectionUser);
   /**
   * Checks if the process is still running
   * @return
   */
   bool IsProcessRunning() const;
   /**
   * Stops the process and all the associated users and IOThreads
   * @return
   */
   bool StopProcess();
   /**
   * Getter for process handler generated ID
   * @return
   */
   std::string GetProcessHandlerID() const;
   /**
   * Getter for the protocol name
   * @return
   */
   std::string GetClientProtocol() const;
   /**
   * Setter for the guac config to be used
   * @param rConfig
   */
   void SetGuacConfig(const GuacConfig & rConfig);
};

typedef GuacClientProcessSocketedHandler * GuacClientProcessSocketedHandlerPtr;

#endif /* GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESS_H */