//
// Created by oiluz on 9/4/2017.
//

#ifndef GUACAMOLE_GUACCONNECTIONNOTIFIER_H
#define GUACAMOLE_GUACCONNECTIONNOTIFIER_H

#include <string>

class GuacConnection;

typedef GuacConnection * GuacConnectionPtr;

class GuacConnectionNotifier
{
public:
   /**
    * Default Constructor
    */
   GuacConnectionNotifier() = default;
   /**
    * Default Destructor
    */
   virtual ~GuacConnectionNotifier() = default;
   /**
    * Callback for when a connection failure occures
    * @param stConnectionID
    */
   virtual void OnConnectionFailure(const std::string & stConnectionID) = 0;
};

typedef GuacConnectionNotifier * GuacConnectionNotifierPtr;
#endif //GUACAMOLE_GUACCONNECTIONNOTIFIER_H
