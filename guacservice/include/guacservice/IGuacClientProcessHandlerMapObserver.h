//
// Created by oiluz on 9/4/2017.
//

#ifndef GUACAMOLE_GUACCLIENTPROCESSMAPOBSERVER_H
#define GUACAMOLE_GUACCLIENTPROCESSMAPOBSERVER_H

#include <string>

class IGuacClientProcessHandlerMapObserver
{
public:
   /**
    * Default Constructor
    */
   IGuacClientProcessHandlerMapObserver() = default;
   /**
    * Default Destructor
    */
   virtual ~IGuacClientProcessHandlerMapObserver() = default;
   /**
    * Callback for when a new process was created
    * @param stID
    */
   virtual void OnClientProcessCreated(const std::string & stID) = 0;
   /**
    * Callback for when a process was removed from the map
    * @param stID
    */
   virtual void OnClientProcessRemoved(const std::string & stID) = 0;
};

typedef IGuacClientProcessHandlerMapObserver * IGuacClientProcessHandlerMapObserverPtr;
#endif //GUACAMOLE_GUACCLIENTPROCESSMAPOBSERVER_H
