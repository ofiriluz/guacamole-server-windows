//
// Created by oiluz on 9/4/2017.
//

#ifndef GUACAMOLE_GUACCLIENTPROCESS_H
#define GUACAMOLE_GUACCLIENTPROCESS_H

#include <guacservice/GuacClientUser.h>
#include <guacservice/GuacDefines.h>
#include <guacamole/error.h>
#include <guacamole/user.h>
#include <boost/algorithm/string.hpp>
#include <boost/process/environment.hpp>

class GuacClientProcess
{
private:
   std::vector<std::tuple<GuacClientUserPtr, boost::shared_ptr<boost::thread>>> m_ClientUsers;
   boost::mutex m_ClientUsersMutex;
   guac_socket * m_ShmSocket;
   guac_client * m_Client;
   bool m_bIsProcessRunning;

private:
   /**
    * Adds a new user to the client process, the user comes from the parent process and is communicated via the shared memory tag using the commands
    * The actual user operatins are done on a seperate thread so this function does not control the user, only adds it to the client users vector
    * @param stSharedMemoryTag
    * @param bOwner
    */
   void AddUser(const std::string & stSharedMemoryTag, bool bOwner);
   /**
    * Removes a user from the client, the user tag is accepted from the parent process via the shared memory commands
    * @param stSharedMemoryTag
    */
   void RemoveUser(const std::string & stSharedMemoryTag);
   /**
    * Reads the logger path from the parent shared memory and initialized the logger
    */
   void InitLoggerFromParent();
   /**
    * Reads the logger path from the parent shared memory
    * @return
    */
   std::string ReadLoggerPath();
   /**
    * Reads the protocol path from the parent shared memory
    * @return
    */
   std::string ReadProtocolPath();
   /**
    * Reads the protocol from the parent shared memory
    * @return
    */
   std::string ReadProtocol();
   /**
    * Reads the library path from the parent shared memory
    * @return
    */
   std::string ReadLibraryPath();
   /**
    * Reads the FPS from the parent shared memory
    */
   short ReadFPS();
   /**
    * Creates the actual full path to the plugin library from the given params
    * @param stLibraryFolder
    * @param stProtocolName
    * @return
    */
   std::string StitchProtocolPath(const std::string & stLibraryFolder, const std::string & stProtocolName) const;
   /**
    * Loads the plugin given the path to the memory
    * @param stProtocolPath
    */
   void LoadPlugin(const std::string & stProtocolPath);
   /**
    * Waits for the command with default SHM timeout, returns the command or empty string if timeout
    * @return
    */
   std::string WaitForCommand();
   /**
    * Handles a given command and performs the associated operation
    * @param stCommand
    */
   void HandleCommand(const std::string & stCommand);

public:
   /**
    * Constructor
    */
   GuacClientProcess();
   /**
    * Default Destructor
    */
   virtual ~GuacClientProcess() = default;
   /**
    * Starts the client process, reads the initial arguments from the parent shared memory
    */
   void StartClientProcess();
};

#endif //GUACAMOLE_GUACCLIENTPROCESS_H
