//
// Created by oiluz on 9/4/2017.
//

#include <guacservice/GuacClientProcessHandlerMap.h>

GuacClientProcessHandlerMap::GuacClientProcessHandlerMap()
{
   InitBuckets();
}

GuacClientProcessHandlerMap::~GuacClientProcessHandlerMap()
{
   // Free all buckets
   guac_common_list ** current = m_Buckets;

   for(int i = 0; i < GUACD_PROC_MAP_BUCKETS; i++)
   {
      guac_common_list_free(*current);
      current++;
   }
}

void GuacClientProcessHandlerMap::InitBuckets()
{
   // Init all buckets
   guac_common_list ** current = m_Buckets;

   for(int i = 0; i < GUACD_PROC_MAP_BUCKETS; i++)
   {
      *current = guac_common_list_alloc();
      current++;
   }
}

unsigned int GuacClientProcessHandlerMap::GetBucketHash(const std::string & stID) const
{
   // Simple hash function to get the index of the bucket from ID
   const char * str = stID.c_str();
   unsigned int hash_value = 0;
   int c;

   // Apply each character in string to the hash code 
   while((c = *(str++)))
   {
      hash_value = hash_value * 65599 + c;
   }

   return hash_value;
}

guac_common_list * GuacClientProcessHandlerMap::GetBucket(const std::string & stID) const
{
   const int index = GetBucketHash(stID) % GUACD_PROC_MAP_BUCKETS;
   return m_Buckets[index];
}

guac_common_list_element *
GuacClientProcessHandlerMap::GetBucketElement(guac_common_list * pBucket, const std::string & stID) const
{
   guac_common_list_element * current = pBucket->head;

   // Search for matching element within bucket
   while(current != nullptr)
   {
      // Check connection ID
      GuacClientProcessHandlerPtr proc = (GuacClientProcessHandlerPtr) current->data;
      if(proc->GetProcessHandlerID() == stID)
      {
         break;
      }

      current = current->next;
   }

   return current;
}

boost::shared_ptr<GuacClientProcessHandlerMap> GuacClientProcessHandlerMap::GetInstance()
{
   static boost::shared_ptr<GuacClientProcessHandlerMap> guac(new GuacClientProcessHandlerMap());
   return guac;
}

GuacClientProcessHandlerPtr GuacClientProcessHandlerMap::CreateProcessHandler()
{
   GuacClientProcessHandlerPtr process(new GuacClientProcessHandler());

   guac_common_list * bucket = GetBucket(process->GetProcessHandlerID());

   guac_common_list_lock(bucket);

   guac_common_list_element * found = GetBucketElement(bucket, process->GetProcessHandlerID());

   // If not found, that means the process can be added to the bucket
   if(!found)
   {
      GuacLogger::GetInstance()->Debug() << "Creating new process with ID " << process->GetProcessHandlerID();
      guac_common_list_add(bucket, process);
      guac_common_list_unlock(bucket);

      // Notify observers
      for(auto && obs : m_Observers)
      {
         obs->OnClientProcessCreated(process->GetProcessHandlerID());
      }

      return process;
   }
   guac_common_list_unlock(bucket);
   return GuacClientProcessHandlerPtr();
}

GuacClientProcessHandlerPtr GuacClientProcessHandlerMap::RetrieveProcessHandler(const std::string & stID)
{
   guac_common_list * bucket = GetBucket(stID);

   guac_common_list_lock(bucket);
   guac_common_list_element * found = GetBucketElement(bucket, stID);

   if(found)
   {
      guac_common_list_unlock(bucket);
      return (GuacClientProcessHandlerPtr) found->data;
   }
   guac_common_list_unlock(bucket);
   return GuacClientProcessHandlerPtr();
}

GuacClientProcessHandlerPtr GuacClientProcessHandlerMap::RemoveProcessHandler(const std::string & stID)
{
   guac_common_list * bucket = GetBucket(stID);

   guac_common_list_lock(bucket);
   guac_common_list_element * found = GetBucketElement(bucket, stID);

   if(found)
   {
      GuacClientProcessHandlerPtr proc = (GuacClientProcessHandlerPtr) found->data;
      GuacLogger::GetInstance()->Debug() << "Removing process with ID " << proc->GetProcessHandlerID();
      guac_common_list_remove(bucket, found);
      guac_common_list_unlock(bucket);

      // Notify observers
      for(auto && obs : m_Observers)
      {
         obs->OnClientProcessRemoved(stID);
      }

      return proc;
   }
   guac_common_list_unlock(bucket);
   return GuacClientProcessHandlerPtr();
}

bool GuacClientProcessHandlerMap::HasProcessHandler(const std::string & stID) const
{
   guac_common_list * bucket = GetBucket(stID);

   guac_common_list_element * found = GetBucketElement(bucket, stID);

   return found != nullptr;
}

void GuacClientProcessHandlerMap::RegisterMapObserver(const IGuacClientProcessHandlerMapObserverPtr & rObserver)
{
   m_Observers.push_back(rObserver);
}
