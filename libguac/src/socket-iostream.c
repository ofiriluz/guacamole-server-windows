/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/
#include <guacamole/config.h>
#ifdef HAVE_BOOST
#include <boost/algorithm/string.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <iostream>
typedef struct guac_socket_iostream_data
{
   boost::mutex socket_mutex;
   std::string current_line;
   size_t curr_pos;
   bool done;
} guac_socket_iostream_data;

static size_t guac_socket_iostream_socket_read_handler(guac_socket * socket,
                                                        void* buf, size_t count)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   size_t left = count;
   char * b = (char*)buf;
   if(!data->done)
   {
      while(data->curr_pos < data->current_line.size() && left > 0)
      {
         b[count - left] = data->current_line[data->curr_pos];
         left--;
         data->curr_pos++;
      }
      if (data->curr_pos >= data->current_line.size())
      {
         data->done = true;
         data->curr_pos = 0;
         data->current_line = "";
      }
      if (left <= 0)
         return count;
   }
   // If were not done adding to the buffer
   // Read next line and fill the rest
   if (std::getline(std::cin, data->current_line))
   {
      boost::algorithm::trim(data->current_line);
      while (data->curr_pos < data->current_line.size() && left > 0)
      {
         b[count - left] = data->current_line[data->curr_pos];
         left--;
         data->curr_pos++;
      }
      if (data->curr_pos >= data->current_line.size())
      {
         data->done = true;
         data->curr_pos = 0;
         data->current_line = "";
      }
   }
   return count - left;
}

size_t guac_socket_iostream_socket_write_handler(guac_socket* socket,
                                          const void* buf, size_t count) {

   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   char * b = (char*)buf;
   std::string s(b, b + count);

   std::cout << s << std::endl;

   return count;
}

static size_t guac_socket_iostream_socket_flush_handler(guac_socket* socket)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   std::cout.flush();

   return 0;
}

static void guac_socket_iostream_socket_lock_handler(guac_socket * socket)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   data->socket_mutex.lock();
}

static void guac_socket_iostream_socket_unlock_handler(guac_socket * socket)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   data->socket_mutex.unlock();
}

static int guac_socket_iostream_socket_free_handler(guac_socket * socket)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(
           socket->data);

   delete data;
   socket->data = nullptr;

   return 0;
}

static int guac_socket_iostream_socket_select_handler(guac_socket* socket,
                                                       int usec_timeout)
{
   guac_socket_iostream_data * data = static_cast<guac_socket_iostream_data*>(socket->data);

   if(std::cin.peek() != std::iostream::traits_type::eof())
      return 1;

   return -1;
}

guac_socket* guac_socket_iostream_socket()
{
   guac_socket_iostream_data * data = new guac_socket_iostream_data();

   data->current_line = "";
   data->done = true;
   data->curr_pos = 0;
   guac_socket* socket = guac_socket_alloc();
   socket->data = data;

   /* Assign handlers */
   socket->read_handler = guac_socket_iostream_socket_read_handler;
   socket->write_handler = guac_socket_iostream_socket_write_handler;
   socket->select_handler = guac_socket_iostream_socket_select_handler;
   socket->flush_handler = guac_socket_iostream_socket_flush_handler;
   socket->lock_handler = guac_socket_iostream_socket_lock_handler;
   socket->unlock_handler = guac_socket_iostream_socket_unlock_handler;
   socket->free_handler = guac_socket_iostream_socket_free_handler;
   
   return socket;
}

#endif