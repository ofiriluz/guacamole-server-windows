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
#include <guacamole/socket.h>
#include <boost/filesystem.hpp>

typedef struct guac_socket_boost_file_data
{
	boost::filesystem::path filepath;

	boost::mutex file_mutex;

	boost::filesystem::ofstream file_out_stream;

	char out_buf[GUAC_SOCKET_OUTPUT_BUFFER_SIZE];

	int written;
} guac_socket_boost_file_data;

static size_t guac_socket_boost_file_read_handler(guac_socket * socket,
	void* buf, size_t count)
{
	return -1;
}

size_t guac_socket_boost_file_write(guac_socket* socket,
	const void* buf, size_t count) {

	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);
	
	data->file_out_stream << buf;

	return 0;

}

static size_t guac_socket_boost_file_flush(guac_socket* socket)
{
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	if (data->written > 0)
	{
		if (guac_socket_boost_file_write(socket, data->out_buf, data->written))
			return 1;

		data->written = 0;
	}

	return 0;
}

static size_t guac_socket_boost_file_write_buffered(guac_socket* socket,
	const void* buf, size_t count)
{
	size_t original_count = count;
	const char* current = (const char * )buf;

	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
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
			if (guac_socket_boost_file_flush(socket))
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

static size_t guac_socket_boost_file_write_handler(guac_socket* socket,
	const void* buf, size_t count)
{
	int retval;
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	boost::mutex::scoped_lock(data->file_mutex);

	retval = guac_socket_boost_file_write_buffered(socket, buf, count);

	return retval;
}

static void guac_socket_boost_file_lock_handler(guac_socket * socket)
{
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	data->file_mutex.lock();
}

static void guac_socket_boost_file_unlock_handler(guac_socket * socket)
{
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	data->file_mutex.unlock();
}

static int guac_socket_boost_file_free_handler(guac_socket * socket)
{
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	data->file_out_stream.close();

	free(data);

	return 0;
}

static size_t guac_socket_boost_file_flush_handler(guac_socket* socket)
{
	int retval;
	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data*>(
		socket->data);

	boost::mutex::scoped_lock lock(data->file_mutex);

	retval = guac_socket_boost_file_flush(socket);

	return retval;
}

static int guac_socket_boost_file_select_handler(guac_socket* socket,
	int usec_timeout) {

	return -1;
}

guac_socket* guac_socket_boost_file(const char * file) {

	guac_socket_boost_file_data * data = static_cast<guac_socket_boost_file_data *>(
		malloc(sizeof(guac_socket_boost_file_data)));

	data->filepath = boost::filesystem::path(std::string(file));
	data->file_out_stream.open(data->filepath);

	/* Associate tee-specific data with new socket */
	guac_socket* socket = guac_socket_alloc();
	socket->data = data;

	/* Assign handlers */
	socket->read_handler = guac_socket_boost_file_read_handler;
	socket->write_handler = guac_socket_boost_file_write_handler;
	socket->select_handler = guac_socket_boost_file_select_handler;
	socket->flush_handler = guac_socket_boost_file_flush_handler;
	socket->lock_handler = guac_socket_boost_file_lock_handler;
	socket->unlock_handler = guac_socket_boost_file_unlock_handler;
	socket->free_handler = guac_socket_boost_file_free_handler;

	return socket;

}
#endif