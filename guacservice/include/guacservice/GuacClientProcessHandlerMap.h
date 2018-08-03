//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESSMAP_H
#define GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESSMAP_H

#include <common/list.h>
#include <boost/process/child.hpp>
#include <guacservice/GuacClientUser.h>
#include <guacservice/GuacClientProcessHandler.h>
#include <guacservice/IGuacClientProcessHandlerMapObserver.h>

#define GUACD_PROC_MAP_BUCKETS GUACD_CLIENT_MAX_CONNECTIONS*2

// Wrapper for native guac classes
class GuacClientProcessHandlerMap
{
private:
   guac_common_list * m_Buckets[GUACD_PROC_MAP_BUCKETS];
   std::vector<IGuacClientProcessHandlerMapObserverPtr> m_Observers;

private:
   /**
    * Private constructor for singleton
    */
   GuacClientProcessHandlerMap();
   /**
    * Deleted for singleton
    * @param other
    */
   GuacClientProcessHandlerMap(const GuacClientProcessHandlerMap & other) = delete;
   /**
    * Deleted for singleton
    * @param other
    */
   GuacClientProcessHandlerMap(GuacClientProcessHandlerMap && other) = delete;
   /**
    * Deleted for singleton
    * @param other
    * @return
    */
   GuacClientProcessHandlerMap operator=(const GuacClientProcessHandlerMap & other) = delete;
   /**
    * Deleted for singleton
    * @param other
    * @return
    */
   GuacClientProcessHandlerMap operator=(GuacClientProcessHandlerMap && other) = delete;
   /**
    * Inits the bucket linked list
    */
   void InitBuckets();
   /**
    * Getter for the bucket hash position for a given id
    * @param stID
    * @return
    */
   unsigned int GetBucketHash(const std::string & stID) const;
   /**
    * Getter for the bucket list from the linked list for a given id
    * @param stID
    * @return
    */
   guac_common_list * GetBucket(const std::string & stID) const;
   /**
    * Getter for the element in the list of a bucket from the linked list
    * @param pBucket
    * @param stID
    * @return
    */
   guac_common_list_element * GetBucketElement(guac_common_list * pBucket, const std::string & stID) const;

public:
   /**
    * Singleton getter
    * @return
    */
   static boost::shared_ptr<GuacClientProcessHandlerMap> GetInstance();
   /**
    * Destructor
    */
   virtual ~GuacClientProcessHandlerMap();
   /**
    * Creates a new process handler and adds it to the linked list map
    * Notifies the observers
    * @return
    */
   GuacClientProcessHandlerPtr CreateProcessHandler();
   /**
    * Retrieves a process handler for a given ID from the linked list map
    * @param stID
    * @return
    */
   GuacClientProcessHandlerPtr RetrieveProcessHandler(const std::string & stID);
   /**
    * Removes a process handler from the map and returns it
    * Notifies the observers
    * @param stID
    * @return
    */
   GuacClientProcessHandlerPtr RemoveProcessHandler(const std::string & stID);
   /**
    * Checks if a process handler with a given ID exists
    * @param stID
    * @return
    */
   bool HasProcessHandler(const std::string & stID) const;
   /**
    * Registers a new observer to the map
    * @param rObserver
    */
   void RegisterMapObserver(const IGuacClientProcessHandlerMapObserverPtr & rObserver);
};

#endif /* GUACAMOLE_SERVER_SRC_GUACCLIENTPROCESSMAP_H */