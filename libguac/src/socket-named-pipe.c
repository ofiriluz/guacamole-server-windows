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
typedef struct guac_socket_named_pipe_data
{
   boost::mutex socket_mutex, select_mutex;
   HANDLE pipe;
   char out_buf[GUAC_SOCKET_OUTPUT_BUFFER_SIZE];	
   int written;
   bool is_creator;
   bool first;
} guac_socket_named_pipe_data;

static size_t guac_socket_named_pipe_socket_read_handler(guac_socket * socket,
                                                       void* buf, size_t count)
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);

   DWORD bRead = 0;
   char * buffer = (char*)buf;
   
	if (!ReadFile(data->pipe, buffer, count, &bRead, NULL))
	{
		guac_error = GUAC_STATUS_SEE_ERRNO;
		guac_error_message = "Error reading data from socket";
	}

	return bRead;
}

size_t guac_socket_named_pipe_socket_write(guac_socket* socket,
	const void* buf, size_t count) 
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);
	
	const char* buffer = (const char *)buf;
   DWORD bytes_wrote = 0;
	while(count > 0)
	{
      if(!WriteFile(data->pipe, buffer, count, &bytes_wrote, NULL))
      {
         guac_error = GUAC_STATUS_SEE_ERRNO;
			guac_error_message = "Error writing data to socket";
			return -1;
      }

		buffer += bytes_wrote;
		count -= bytes_wrote;
	}
	
	return 0;
}

static size_t guac_socket_named_pipe_socket_flush(guac_socket* socket)
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);

	if (data->written > 0)
	{
		if (guac_socket_named_pipe_socket_write(socket, data->out_buf, data->written))
			return 1;

		data->written = 0;
	}

	return 0;
}

static size_t guac_socket_named_pipe_socket_write_buffered(guac_socket* socket,
	const void* buf, size_t count)
{
	size_t original_count = count;
	const char* current = (const char *)buf;

   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);

	/* Append to buffer, flush if necessary */
	while (count > 0)
	{
		int chunk_size;
		int remaining = sizeof(data->out_buf) - data->written;

		/* If no space left in buffer, flush and retry */
		if (remaining == 0)
		{

			/* Abort if error occurs during flush */
			if (guac_socket_named_pipe_socket_flush(socket))
				return -1;

			/* Retry buffer append */
			continue;

		}

		/* Calculate size of chunk to be written to buffer */
		chunk_size = count;
		if (chunk_size > remaining)
			chunk_size = remaining;

		/* Update output buffer */
		memcpy(data->out_buf + data->written, current, chunk_size);
		data->written += chunk_size;

		/* Update provided buffer */
		current += chunk_size;
		count -= chunk_size;
	}

	/* All bytes have been written, possibly some to the internal buffer */
	return original_count;
}

size_t guac_socket_named_pipe_socket_write_handler(guac_socket* socket,
                                                 const void* buf, size_t count) 
{
	int retval;
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);

	retval = guac_socket_named_pipe_socket_write_buffered(socket, buf, count);

	return retval;
}

static size_t guac_socket_named_pipe_socket_flush_handler(guac_socket* socket)
{		
   int retval;
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
		socket->data);

	retval = guac_socket_named_pipe_socket_flush(socket);

	return retval;
}

static void guac_socket_named_pipe_socket_lock_handler(guac_socket * socket)
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
           socket->data);

   data->socket_mutex.lock();
}

static void guac_socket_named_pipe_socket_unlock_handler(guac_socket * socket)
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
           socket->data);

   data->socket_mutex.unlock();
}

static int guac_socket_named_pipe_socket_free_handler(guac_socket * socket)
{
   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(
           socket->data);

   if(data->is_creator)
   {
      DisconnectNamedPipe(data->pipe);
   }
   else
   {
      CloseHandle(data->pipe);
   }

   delete data;
   socket->data = nullptr;

   return 0;
}

static int guac_socket_named_pipe_socket_select_handler(guac_socket* socket,
                                                      int usec_timeout)
{  
   boost::condition_variable cond;

   guac_socket_named_pipe_data * data = static_cast<guac_socket_named_pipe_data*>(socket->data);

   if(data->first && data->is_creator)
   {
      data->first = false;
      if(ConnectNamedPipe(data->pipe, NULL))
      {
         return 1;
      }
      return 0;
   }

	boost::mutex::scoped_lock lock(data->select_mutex);

   volatile bool timeout = false;

   boost::thread runner([&]()
   {
      LPDWORD totalBytesReady = 0;
      while(!timeout)
      {
         boost::mutex::scoped_lock lambda_lock(data->select_mutex);
         if(PeekNamedPipe(data->pipe, NULL, 0, NULL, totalBytesReady, NULL))
         {
            if(totalBytesReady > 0)
            {
               cond.notify_all();
               return;
            }
         }
      }
   });

	if(!cond.timed_wait(lock, boost::posix_time::milliseconds(usec_timeout)))
	{
      timeout = true;
      guac_error = GUAC_STATUS_TIMEOUT;
      guac_error_message = "Timeout while waiting for data on socket";
      return 0;
	}
   timeout = true;
	return 1;
}

guac_socket* guac_socket_named_pipe_socket_create(const std::string & pipe_name)
{
   guac_socket_named_pipe_data * data = new guac_socket_named_pipe_data();

   data->pipe = CreateNamedPipe(TEXT(pipe_name.c_str()),
      PIPE_ACCESS_DUPLEX, 
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      1,
      1024*16,
      1024*16,
      NMPWAIT_USE_DEFAULT_WAIT,
      NULL);

   if (data->pipe == INVALID_HANDLE_VALUE)
   {
      delete data;
      return nullptr;
   }

   data->is_creator = true;
   data->first = true;

   guac_socket* socket = guac_socket_alloc();
   socket->data = data;

   /* Assign handlers */
   socket->read_handler = guac_socket_named_pipe_socket_read_handler;
   socket->write_handler = guac_socket_named_pipe_socket_write_handler;
   socket->select_handler = guac_socket_named_pipe_socket_select_handler;
   socket->flush_handler = guac_socket_named_pipe_socket_flush_handler;
   socket->lock_handler = guac_socket_named_pipe_socket_lock_handler;
   socket->unlock_handler = guac_socket_named_pipe_socket_unlock_handler;
   socket->free_handler = guac_socket_named_pipe_socket_free_handler;

   return socket;
}

guac_socket* guac_socket_named_pipe_socket_join(const std::string & pipe_name)
{
   guac_socket_named_pipe_data * data = new guac_socket_named_pipe_data();

   data->pipe = CreateFile(TEXT(pipe_name.c_str()),
      GENERIC_READ | GENERIC_WRITE,
      0,
      NULL,
      OPEN_EXISTING,
      0,
      NULL);

   if (data->pipe == INVALID_HANDLE_VALUE)
   {
      delete data;
      return nullptr;
   }

   DWORD mode = PIPE_READMODE_MESSAGE;
   SetNamedPipeHandleState(data->pipe, &mode, NULL, NULL);

   data->is_creator = false;
   data->first = false;

   guac_socket* socket = guac_socket_alloc();
   socket->data = data;

   /* Assign handlers */
   socket->read_handler = guac_socket_named_pipe_socket_read_handler;
   socket->write_handler = guac_socket_named_pipe_socket_write_handler;
   socket->select_handler = guac_socket_named_pipe_socket_select_handler;
   socket->flush_handler = guac_socket_named_pipe_socket_flush_handler;
   socket->lock_handler = guac_socket_named_pipe_socket_lock_handler;
   socket->unlock_handler = guac_socket_named_pipe_socket_unlock_handler;
   socket->free_handler = guac_socket_named_pipe_socket_free_handler;

   return socket;
}

#endif