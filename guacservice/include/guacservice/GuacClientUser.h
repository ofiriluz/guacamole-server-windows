//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACSERVICEUSER_H
#define GUACAMOLE_SERVER_SRC_GUACSERVICEUSER_H

#include <boost/shared_ptr.hpp>
#include <guacamole/socket.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/parser.h>
#include <guacamole/user.h>
#include <guacamole/error.h>
#include <guacservice/GuacLogger.h>

#define GUACD_TIMEOUT 15000
#define GUACD_CLIENT_MAX_CONNECTIONS 65536
#define GUAC_DEFAULT_OPTIMAL_RESOLUTION 96

class GuacClientUser
{
private:
   guac_user * m_User;
   guac_parser * m_Parser;
   std::string m_stShmName;
   char ** m_AudioMimeTypes,
           ** m_VideoMimeTypes,
           ** m_ImageMimeTypes;
   bool m_bIsRunning;

private:
   /**
    * The actual IO Thread of the user on the child process
    * Handles incoming instructions from the associated socket
    */
   void IOThread();
   /**
    * Frees the given mimetypes
    * @param mimetypes
    */
   void FreeMimeTypes(char ** mimetypes);
   /**
    * Copies the mimetypes from the parser
    * @return
    */
   char ** CopyMimeTypes();
   /**
    * Expects an instruction to come from the piped socket
    * @param stInstruction
    * @param iArgc
    * @return
    */
   bool Expect(const std::string & stInstruction, int iArgc);
   /**
    * Sends the arguments needed as on the guac protocol to the piped socket
    * @return
    */
   bool SendClientArgs();
   /**
    * Performs the handshake as described on the guac protocol with the associated piped socket
    * @return
    */
   bool PerformHandshake();
   /**
    * Stops the user IOThread and RDP session, releases all the relevent objects
    */
   void StopUser();

public:
   /**
    * Constructor
    * @param pUser
    * @param stShmName
    */
   GuacClientUser(guac_user * pUser, const std::string & stShmName);
   /**
    * Default Destructor
    */
   virtual ~GuacClientUser() = default;
   /**
    * Runs the user, performs the handshake and begins the IOThread, this is blocked untill the user is done
    */
   void RunUser();
   /**
    * Stops the running user, does not block, just notifies abort
    */
   void AbortUser();
   /**
    * Getter for the user shared memory tag
    * @return
    */
   std::string GetUserTag() const;
};

typedef boost::shared_ptr<GuacClientUser> GuacClientUserPtr;


#endif /* GUACAMOLE_SERVER_SRC_GUACSERVICEUSER_H */