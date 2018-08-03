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
#include <guacamole/error.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/socket.h>

typedef struct guac_socket_boost_tcp_data
{
	boost::asio::io_service ios;

	boost::shared_ptr<boost::asio::ip::tcp::socket> socket;
   boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ssl_socket;

	boost::mutex socket_mutex;
	boost::mutex select_mutex;

   bool is_ssl;
} guac_socket_boost_tcp_data;

static size_t guac_socket_boost_tcp_socket_read_handler(guac_socket * socket,
	void* buf, size_t count)
{
	guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
		socket->data);

	boost::system::error_code error;

   // Read from the socket depending if SSL or not
   size_t retval =
      data->is_ssl ? data->ssl_socket->read_some(boost::asio::buffer((char*)buf, count), error)
      : data->socket->read_some(boost::asio::buffer((char*)buf, count), error);

	if (error) 
	{
		guac_error = GUAC_STATUS_SEE_ERRNO;
		guac_error_message = "Error reading data from socket";
	}

   // Return amount read
	return retval;
}

static size_t guac_socket_boost_tcp_socket_write_handler(guac_socket* socket,
	const void* buf, size_t count)
{
   guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
      socket->data);

   boost::system::error_code error;
   const char* buffer = (const char *)buf;

   // Write to the socket depending if SSL or not
   size_t retval =
	  data->is_ssl ? boost::asio::write(*data->ssl_socket,boost::asio::buffer(buffer, count), error)
	  : boost::asio::write(*data->socket, boost::asio::buffer(buffer, count), error);
   if (error)
   {
      guac_error = GUAC_STATUS_SEE_ERRNO;
	  guac_error_message = "Error writing data to socket";
	  return retval;
   }

   // Return amount written
   return retval;
}

static void guac_socket_boost_tcp_socket_lock_handler(guac_socket * socket)
{
	guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
		socket->data);

	data->socket_mutex.lock();
}

static void guac_socket_boost_tcp_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
		socket->data);

	data->socket_mutex.unlock();
}

static int guac_socket_boost_tcp_socket_free_handler(guac_socket * socket)
{
	guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
		socket->data);

   delete data;
   socket->data = nullptr;

	return 0;
}

static size_t guac_socket_boost_tcp_socket_flush_handler(guac_socket* socket)
{
   guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(
      socket->data);

   return 0;
}

static int guac_socket_boost_tcp_socket_select_handler(guac_socket* socket,
	int usec_timeout)
{
   boost::condition_variable cond;

	guac_socket_boost_tcp_data * data = static_cast<guac_socket_boost_tcp_data*>(socket->data);

   // Lock the select mutex for the condition
	boost::mutex::scoped_lock lock(data->select_mutex);

	boost::system::error_code async_error;

   // Define the handler for the async read some callback
   auto handler = [&](const boost::system::error_code & error, size_t bytes) {
      // Re-lock the select mutex and notify the conditional variable that new data is ready
      boost::mutex::scoped_lock lambda_lock(data->select_mutex);
      async_error = error;
      cond.notify_all();
   };
   
   // Add an async read some job to the io service with empty buffers to only peek for data
   data->is_ssl ? data->ssl_socket->async_read_some(boost::asio::null_buffers(), handler) :
      data->socket->async_read_some(boost::asio::null_buffers(), handler);

   // Create a thread which will run the io service to perform the job
   boost::thread runner([&]()
   {
      // Run IO service depending if SSL or not
      data->is_ssl ? data->ssl_socket->get_io_service().run_one() :
         data->socket->get_io_service().run_one();
   });

   // Wait for either a notification from the handler or a timeout pop
	if(!cond.timed_wait(lock, boost::posix_time::milliseconds(usec_timeout)))
	{
      guac_error = GUAC_STATUS_TIMEOUT;
      guac_error_message = "Timeout while waiting for data on socket";
      return 0;
	}
   else if(async_error)
   {
      guac_error = GUAC_STATUS_SEE_ERRNO;
      guac_error_message = "Error while waiting for data on socket";
      return -1;
   }
	return 1;
}

guac_socket* guac_socket_boost_tcp_socket(const boost::shared_ptr<boost::asio::ip::tcp::socket> & tcp_socket)
{
   guac_socket_boost_tcp_data * data = new guac_socket_boost_tcp_data();

	data->socket = tcp_socket;
   data->is_ssl = false;

	guac_socket* socket = guac_socket_alloc();
	socket->data = data;

	socket->read_handler = guac_socket_boost_tcp_socket_read_handler;
	socket->write_handler = guac_socket_boost_tcp_socket_write_handler;
	socket->select_handler = guac_socket_boost_tcp_socket_select_handler;
	socket->flush_handler = guac_socket_boost_tcp_socket_flush_handler;
	socket->lock_handler = guac_socket_boost_tcp_socket_lock_handler;
	socket->unlock_handler = guac_socket_boost_tcp_socket_unlock_handler;
	socket->free_handler = guac_socket_boost_tcp_socket_free_handler;
   socket->reset_handler = nullptr;

	return socket;
}

guac_socket* guac_socket_boost_tcp_socket(const boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> & tcp_ssl_socket)
{
   guac_socket_boost_tcp_data * data = new guac_socket_boost_tcp_data();

   data->ssl_socket = tcp_ssl_socket;
   data->is_ssl = true;

   guac_socket* socket = guac_socket_alloc();
   socket->data = data;

   socket->read_handler = guac_socket_boost_tcp_socket_read_handler;
   socket->write_handler = guac_socket_boost_tcp_socket_write_handler;
   socket->select_handler = guac_socket_boost_tcp_socket_select_handler;
   socket->flush_handler = guac_socket_boost_tcp_socket_flush_handler;
   socket->lock_handler = guac_socket_boost_tcp_socket_lock_handler;
   socket->unlock_handler = guac_socket_boost_tcp_socket_unlock_handler;
   socket->free_handler = guac_socket_boost_tcp_socket_free_handler;
   socket->reset_handler = nullptr;

   return socket;
}

#endif