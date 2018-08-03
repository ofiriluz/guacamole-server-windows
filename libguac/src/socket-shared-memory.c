// #define LOCK_FREE_TEST
// #define RW_QUEUE_IMPL
// #define BOOST_SPSC_QUEUE
// #define BOOST_MSG_QUEUE
#define LOCKING_NORMAL_QUEUE
// #define LOCK_FREE_NORMAL_QUEUE
#ifdef LOCK_FREE_TEST
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#include <boost/detail/winapi/semaphore.hpp>


#pragma once

#pragma warning(push)
#include <boost/lockfree/queue.hpp>
#include <boost/optional.hpp>
#pragma warning(pop)

#include <array>
#include <thread>
#include <chrono>


namespace IPC
{
	namespace detail
	{
		namespace LockFree
		{
			/// Provides a multi-reader/multi-writer queue with a compile-time fixed capacity
			/// where pushing and popping is lock-free. Supports any element type. Can also
			/// be allocated directly in shared memory as along as T has same capability.
			template <typename T, std::size_t Capacity, typename Enable = void>
			class FixedQueue;


			/// Specialization for complex (non-trivially copyable) types.
			template <typename T, std::size_t Capacity, typename Enable>
			class FixedQueue
			{
			public:
				FixedQueue()
				{
					for (std::size_t i = 0; i < Capacity; ++i)
					{
						m_pool.push(i);
					}
				}

				FixedQueue(const FixedQueue& other) = delete;
				FixedQueue& operator=(const FixedQueue& other) = delete;

				~FixedQueue()
				{
					m_queue.consume_all(
						[this](std::size_t index)
					{
						auto obj = reinterpret_cast<T*>(&m_storage[index]);
						obj->~T();
						(void)obj;
					});
				}

				bool IsEmpty() const
				{
					return m_queue.empty();
				}

				template <typename U>
				bool Push(U&& value)
				{
					std::size_t index;
					if (m_pool.pop(index))
					{
						try
						{
							new (&m_storage[index]) T(std::forward<U>(value));
						}
						catch (...)
						{
							auto result = m_pool.push(index);
							assert(result);
							(void)result;
							throw;
						}

						auto result = m_queue.push(index);
						assert(result);

						return result;
					}

					return false;
				}

				boost::optional<T> Pop()
				{
					boost::optional<T> value;

					std::size_t index;
					if (m_queue.pop(index))
					{
						auto obj = reinterpret_cast<T*>(&m_storage[index]);

						auto restoreIndex = [&]
						{
							auto result = m_pool.push(index);
							assert(result);
							(void)result;
						};

						try
						{
							value = std::move(*obj);
							obj->~T();
						}
						catch (...)
						{
							restoreIndex();
							throw;
						}

						restoreIndex();
					}

					return value;
				}

				template <typename Function>
				std::size_t ConsumeAll(Function&& func)
				{
					return m_queue.consume_all(
						[this, &func](std::size_t index)
					{
						auto obj = reinterpret_cast<T*>(&m_storage[index]);

						auto restoreIndex = [&]
						{
							auto result = m_pool.push(index);
							assert(result);
							(void)result;
						};

						try
						{
							func(std::move(*obj));
							obj->~T();
						}
						catch (...)
						{
							restoreIndex();
							throw;
						}

						restoreIndex();
					});
				}

			private:
				using Queue = boost::lockfree::queue<std::size_t, boost::lockfree::capacity<Capacity>>;

				Queue m_pool;
				Queue m_queue;
				std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, Capacity> m_storage;
			};


			/// Specialization for trivially copyable types.
			template <typename T, std::size_t Capacity>
			class FixedQueue<T, Capacity, std::enable_if_t<boost::has_trivial_destructor<T>::value && boost::has_trivial_assign<T>::value>>
			{
			public:
				bool IsEmpty() const
				{
					return m_queue.empty();
				}

				bool Push(const T& value)
				{
					return m_queue.push(value);
				}

				boost::optional<T> Pop()
				{
					T value;
					return m_queue.pop(value) ? boost::optional<T>{ std::move(value) } : boost::none;
				}

				template <typename Function>
				std::size_t ConsumeAll(Function&& func)
				{
					return m_queue.consume_all([this, &func](T& value) { func(std::move(value)); });
				}

			private:
				boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>> m_queue;
			};

		} // LockFree
		class SpinLock
		{
		public:
			void lock()
			{
				std::size_t count{ 0 };
				bool locked{ false };

				while (++count <= c_maxYieldCount)
				{
					if (m_lock.test_and_set(std::memory_order_acquire))
					{
						std::this_thread::yield();
					}
					else
					{
						locked = true;
						break;
					}
				}

				if (!locked)
				{
					std::chrono::milliseconds delay{ c_sleepDuration };

					while (m_lock.test_and_set(std::memory_order_acquire))
					{
						std::this_thread::sleep_for(delay);
					}
				}
			}

			void unlock()
			{
				m_lock.clear(std::memory_order_release);
			}

		private:
			static constexpr std::size_t c_maxYieldCount = 1000;
			static constexpr std::size_t c_sleepDuration = 1;

			std::atomic_flag m_lock{ ATOMIC_FLAG_INIT };
		};
		namespace
		{
			HANDLE VerifyHandle(HANDLE handle, bool verifyValidHandle)
			{
				if (!handle || handle == INVALID_HANDLE_VALUE || verifyValidHandle)
				{
					auto error = ::GetLastError();
					if (error != ERROR_SUCCESS)
					{
						try
						{
							throw std::system_error{ static_cast<int>(error), std::system_category() };
						}
						catch (const std::system_error&)
						{
							std::throw_with_nested(std::exception{ "Invalid handle." });
						}
					}
				}

				return handle;
			}
		}
		class KernelObject
		{
		public:
			KernelObject(std::nullptr_t) {}

			explicit KernelObject(void* handle, bool verifyValidHandle = false) : m_handle{ VerifyHandle(handle, verifyValidHandle), ::CloseHandle } {}

			void Wait() const { ::WaitForSingleObject(m_handle.get(), INFINITE); }

			bool Wait(std::chrono::milliseconds timeout) const { return ::WaitForSingleObject(m_handle.get(), static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0; }

			bool IsSignaled() const { return Wait(std::chrono::milliseconds::zero()); }

			explicit operator bool() const { return static_cast<bool>(m_handle); }

			explicit operator void*() const { return m_handle.get(); }

		private:
			std::shared_ptr<void> m_handle;
		};
		class KernelEvent : public KernelObject
		{
		public:
			KernelEvent(nullptr_t) :KernelObject(nullptr) {}

			KernelEvent(boost::interprocess::create_only_t co,
				bool manualReset, bool initialState = false, const char* name = nullptr)
				:KernelObject{ ::CreateEventA(nullptr, manualReset, initialState, name), true } {}

			KernelEvent(boost::interprocess::open_only_t co, const char* name) 
				:KernelObject{ ::OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name) }
			{}

			bool Signal() { return ::SetEvent(static_cast<void*>(*this)) != FALSE; }

			bool Reset() { return ::ResetEvent(static_cast<void*>(*this)) != FALSE;  }
		};
		class KernelProcess : public KernelObject
		{
		public:
			explicit KernelProcess(std::uint32_t pid)
				: KernelObject{ ::OpenProcess(SYNCHRONIZE, false, pid) } {}

			static std::uint32_t GetCurrentProcessId() { return ::GetCurrentProcessId(); }
		};
	} // detail
} // IPC


#define PACKET_SIZE 1024
#define PACKETS_AMOUNT 512

typedef struct guac_packet {
	char buffer[PACKET_SIZE];
	size_t size;
	size_t offset;
} guac_packet;

using ring_buffer = boost::lockfree::spsc_queue<
	guac_packet,
	boost::lockfree::capacity<PACKETS_AMOUNT>
>;
typedef struct guac_socket_shared_memory_msgqueue_data
{
	IPC::detail::LockFree::FixedQueue<guac_packet, PACKETS_AMOUNT> queue;
} guac_socket_shared_memory_msgqueue_data;

typedef struct guac_socket_shared_memory_msgqueue
{
	boost::shared_ptr<boost::interprocess::managed_shared_memory> shm;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_msgqueue_data> data;
	boost::shared_ptr<IPC::detail::KernelEvent> event;
	boost::optional<guac_packet> curr_packet;
} guac_socket_shared_memory_msgqueue;
typedef struct guac_socket_shared_memory_data
{
	guac_socket_shared_memory_msgqueue * parent_to_child_msgqueue;
	guac_socket_shared_memory_msgqueue * child_to_parent_msgqueue;
	boost::mutex local_mutex;
	std::string region_name;
	bool is_parent;
	bool multi_packet_read;
	bool streamlined;
} guac_socket_shared_memory_data;

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;


	char * buffer = (char *)buf;
	size_t left = count;

	while (left > 0 && !msgqueue->data->queue.IsEmpty())
	{
		boost::optional<guac_packet> p;
		if (msgqueue->curr_packet.is_initialized())
		{
			p = msgqueue->curr_packet;
		}
		else
		{
			p = msgqueue->data->queue.Pop();
		}
		if (!p.is_initialized())
		{
			msgqueue->event->Reset();
			return count - left;
		}

		size_t s = std::min(left, p->size);
		std::memmove(buffer + (count - left), p->buffer + p->offset, s);
		if (s == left && (p->size != left))
		{
			p->size -= left;
			p->offset += left;
			msgqueue->curr_packet = p;
		}
		else if(msgqueue->curr_packet.is_initialized())
		{
			msgqueue->curr_packet.reset();
		}
		left -= s;
	}

	if (msgqueue->data->queue.IsEmpty())
	{
		msgqueue->event->Reset();
	}

	return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring   
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;
	char * buffer = (char *)buf;

	boost::optional<guac_packet> p;
	if (msgqueue->curr_packet.is_initialized())
	{
		p = msgqueue->curr_packet;
	}
	else
	{
		p = msgqueue->data->queue.Pop();
		if (!p.is_initialized())
		{
			msgqueue->event->Reset();
			// fprintf(stdout, "SAD %s\n", data->is_parent ? "Parent" : "Child");
			return 0;
		}
	}
	//fprintf(stdout, "Read : %s, %d/%d - %d, %s\n",
	//	p->buffer, p->size, p->offset, count, data->is_parent ? "Parent" : "Child");
	size_t s = std::min(count, p->size);
	std::memcpy(buffer, p->buffer + p->offset, s);
	if (s == count && (p->size != count))
	{
		p->size -= count;
		p->offset += count;
		msgqueue->curr_packet = p;
	}
	else if (msgqueue->curr_packet.is_initialized())
	{
		msgqueue->curr_packet.reset();
	}

	if (msgqueue->data->queue.IsEmpty())
	{
		msgqueue->event->Reset();
	}

	return s;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
	void * buf, size_t count)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	// Multi packet read, fill as much as possible to the buffer from the readable packets
	if (data->multi_packet_read)
	{
		return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
	}
	// Single packet read, read a single packet from the queue to the buffer
	else
	{
		return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	}
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
	const void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->parent_to_child_msgqueue
		: data->child_to_parent_msgqueue;

	char * buffer = (char *)buf;
	size_t left = count;
	while (left > 0)
	{
		guac_packet p;
		p.offset = 0;
		size_t s = std::min(left, (size_t)PACKET_SIZE);
		std::memcpy(p.buffer, buffer, s);
		p.size = s;
		msgqueue->data->queue.Push(p);
		left -= s;
	}
	msgqueue->event->Signal();
	return count - left;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.lock();
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.unlock();
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Reset the queues to notify everyone
	guac_socket_shared_memory_socket_reset_handler(socket);

	std::string strSM_PTC = data->region_name + "_PTC";
	std::string strSM_CTP = data->region_name + "_CTP";

	boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
	boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

	delete data;
	socket->data = nullptr;

	return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
	int usec_timeout)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;

	// First check if there is already packets in the queue
	if (!msgqueue->data->queue.IsEmpty())
	{
		return 1;
	}

	if (!msgqueue->event->Wait(std::chrono::milliseconds(usec_timeout)))
	{
		guac_error = GUAC_STATUS_TIMEOUT;
		guac_error_message = "Timeout while waiting for data on socket";
		return 0;
	}

	return !msgqueue->data->queue.IsEmpty();
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
	int packet_size)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = true;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();
	boost::interprocess::shared_memory_object::remove((shname + "_PTC").c_str());
	boost::interprocess::shared_memory_object::remove((shname + "_CTP").c_str());

	data->parent_to_child_msgqueue->event.reset(
		new IPC::detail::KernelEvent(boost::interprocess::create_only, true, false, (shname + "_PTC_EVENT").c_str()));
	data->child_to_parent_msgqueue->event.reset(
		new IPC::detail::KernelEvent(boost::interprocess::create_only, true, false, (shname + "_CTP_EVENT").c_str()));
	data->parent_to_child_msgqueue->curr_packet.reset();
	data->child_to_parent_msgqueue->curr_packet.reset();

	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_PTC").c_str(), 1024 * 1024));
	data->parent_to_child_msgqueue->data =
		data->parent_to_child_msgqueue->shm->construct<guac_socket_shared_memory_msgqueue_data>("Ring")();

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_CTP").c_str(), 1024 * 1024));
	data->child_to_parent_msgqueue->data =
		data->child_to_parent_msgqueue->shm->construct<guac_socket_shared_memory_msgqueue_data>("Ring")();

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = false;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->parent_to_child_msgqueue->curr_packet.reset();
	data->child_to_parent_msgqueue->curr_packet.reset();

	boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

	data->parent_to_child_msgqueue->event.reset(
		new IPC::detail::KernelEvent(boost::interprocess::open_only, (shname + "_PTC_EVENT").c_str()));
	data->child_to_parent_msgqueue->event.reset(
		new IPC::detail::KernelEvent(boost::interprocess::open_only, (shname + "_CTP_EVENT").c_str()));

	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_PTC").c_str()));
	data->parent_to_child_msgqueue->data =
		data->parent_to_child_msgqueue->shm->find<guac_socket_shared_memory_msgqueue_data>("Ring").first;

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_CTP").c_str()));
	data->child_to_parent_msgqueue->data =
		data->child_to_parent_msgqueue->shm->find<guac_socket_shared_memory_msgqueue_data>("Ring").first;


	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

#endif
#endif
#ifdef RW_QUEUE_IMPL
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>
#include <guacamole/readerwriterqueue.h>

typedef struct guac_packet {
	char buffer[512];
	size_t size;
	size_t offset;
} guac_packet;

using BRWQueue = moodycamel::BlockingReaderWriterQueue<guac_packet>;

typedef struct guac_socket_shared_memory_msgqueue
{
	boost::shared_ptr<boost::interprocess::managed_shared_memory> shm;
	BRWQueue * queue;
	guac_packet curr;
	bool rdy;
} guac_socket_shared_memory_msgqueue;
typedef struct guac_socket_shared_memory_data
{
	guac_socket_shared_memory_msgqueue * parent_to_child_msgqueue;
	guac_socket_shared_memory_msgqueue * child_to_parent_msgqueue;
	boost::mutex local_mutex;
	std::string region_name;
	bool is_parent;
	bool multi_packet_read;
	bool streamlined;
} guac_socket_shared_memory_data;

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;


	char * buffer = (char *)buf;
	size_t left = count;

	while (left > 0 && !(msgqueue->queue->size_approx() == 0))
	{
		if (msgqueue->rdy)
		{
			size_t s = std::min(left, msgqueue->curr.size);
			std::memmove(buffer + (count-left), msgqueue->curr.buffer, s);
			if (s == left)
			{
				msgqueue->curr.offset += count;
				msgqueue->curr.size -= count;
			}
			else
			{
				msgqueue->rdy = false;
			}
			left -= s;
		}
		else
		{
			guac_packet * p = msgqueue->queue->peek();
			size_t s = std::min(left, p->size);
			std::memmove(buffer+(count-left), p->buffer + p->offset, s);
			if (s == left)
			{
				p->size -= left;
				p->offset += left;
			}
			else
			{
				msgqueue->queue->pop();
			}
			left -= s;
		}
	}

	fprintf(stdout, "Finished Reading %d/%d - %s\n", count - left, count, data->is_parent ? "Parent" : "Child");
	return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring   
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;

	char * buffer = (char *)buf;

	if (msgqueue->rdy)
	{
		size_t s = std::min(count, msgqueue->curr.size);
		std::memmove(buffer, msgqueue->curr.buffer, s);
		if (s == count)
		{
			msgqueue->curr.offset += count;
			msgqueue->curr.size -= count;
		}
		else
		{
			msgqueue->rdy = false;
		}
		return s;
	}

	guac_packet * p = msgqueue->queue->peek();
	size_t s = std::min(count, p->size);
	std::memmove(buffer, p->buffer + p->offset, s);
	if (s == count)
	{
		p->size -= count;
		p->offset += count;
	}
	else
	{
		msgqueue->queue->pop();
	}
	return s;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
	void * buf, size_t count)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
	return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	// Multi packet read, fill as much as possible to the buffer from the readable packets
	if (data->multi_packet_read)
	{
		return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
	}
	// Single packet read, read a single packet from the queue to the buffer
	else
	{
		return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	}
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
	const void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->parent_to_child_msgqueue
		: data->child_to_parent_msgqueue;

	char * buffer = (char *)buf;
	fprintf(stdout, "Writing %s - %s\n", buffer, data->is_parent ? "Parent" : "Child");
	int left = count;
	guac_packet p;
	p.offset = 0;
	while (left > 0)
	{
		size_t s = std::min(left, 512);
		std::memmove(p.buffer, buffer, s);
		p.size = s;
		msgqueue->queue->enqueue(p);
		left -= s;
	}
	fprintf(stdout, "Wrote : %d/%d - %s\n", count - left, count, data->is_parent ? "Parent" : "Child");
	return count - left;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.lock();
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.unlock();
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Reset the queues to notify everyone
	guac_socket_shared_memory_socket_reset_handler(socket);

	std::string strSM_PTC = data->region_name + "_PTC";
	std::string strSM_CTP = data->region_name + "_CTP";

	boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
	boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

	delete data;
	socket->data = nullptr;

	return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
	int usec_timeout)
{
	static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;

	// First check if there is already packets in the queue
	if (msgqueue->queue->size_approx() > 0)
	{
		return 1;
	}

	fprintf(stdout, "Selecting - %s\n", data->is_parent ? "Parent" : "Child");
	// Wait for the interprocess lock to receive a notification that there are packets ready
	if (!msgqueue->queue->wait_dequeue_timed(msgqueue->curr, std::chrono::milliseconds(usec_timeout)))
	{
		msgqueue->rdy = false;
		guac_error = GUAC_STATUS_TIMEOUT;
		guac_error_message = "Timeout while waiting for data on socket";
		return 0;
	}
	msgqueue->rdy = true;
	fprintf(stdout, "Select Popped Properly - %s\n", data->is_parent ? "Parent" : "Child");
	// Make sure that there is actually data on the queue
	return 1;
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
	int packet_size)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = true;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();
	boost::interprocess::shared_memory_object::remove((shname + "_PTC").c_str());
	boost::interprocess::shared_memory_object::remove((shname + "_CTP").c_str());

	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_PTC").c_str(), 4 * 1024 * 1024));

	data->parent_to_child_msgqueue->queue =
		data->parent_to_child_msgqueue->shm->construct<BRWQueue>("RingBuffer")
			(data->parent_to_child_msgqueue->shm, 512);

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_CTP").c_str(), 4 * 1024 * 1024));
	data->child_to_parent_msgqueue->queue =
		data->child_to_parent_msgqueue->shm->construct<BRWQueue>("RingBuffer")
			(data->child_to_parent_msgqueue->shm, 512);

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = false;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();

	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_PTC").c_str()));
	data->parent_to_child_msgqueue->queue =
		data->parent_to_child_msgqueue->shm->find<BRWQueue>("RingBuffer").first;

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_CTP").c_str()));
	data->child_to_parent_msgqueue->queue =
		data->child_to_parent_msgqueue->shm->find<BRWQueue>("RingBuffer").first;

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

#endif
#endif
#ifdef BOOST_SPSC_QUEUE
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#include <boost/detail/winapi/semaphore.hpp>

#define PACKET_SIZE 1024
#define PACKETS_AMOUNT 512

typedef struct guac_packet {
	char buffer[PACKET_SIZE];
	size_t size;
	size_t offset;
} guac_packet;

using ring_buffer = boost::lockfree::spsc_queue<
	guac_packet,
	boost::lockfree::capacity<PACKETS_AMOUNT>
>;
typedef struct guac_socket_shared_memory_msgqueue_data
{
	ring_buffer queue;
	boost::interprocess::interprocess_condition cond;
	boost::interprocess::interprocess_mutex mutex;
} guac_socket_shared_memory_msgqueue_data;

typedef struct guac_socket_shared_memory_msgqueue
{
	boost::shared_ptr<boost::interprocess::managed_shared_memory> shm;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_msgqueue_data> data;
	HANDLE semaphore;
} guac_socket_shared_memory_msgqueue;
typedef struct guac_socket_shared_memory_data
{
	guac_socket_shared_memory_msgqueue * parent_to_child_msgqueue;
	guac_socket_shared_memory_msgqueue * child_to_parent_msgqueue;
	boost::mutex local_mutex;
	std::string region_name;
	bool is_parent;
	bool multi_packet_read;
	bool streamlined;
} guac_socket_shared_memory_data;

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;

	
	char * buffer = (char *)buf;
	size_t left = count;
	//if (data->is_parent)
	//	fprintf(stdout, "Starting To Read - %s/%s\n", data->is_parent ? "Parent" : "Child", data->region_name);
	while (left > 0 && msgqueue->data->queue.read_available())
	{
		guac_packet & p = msgqueue->data->queue.front();
		size_t s = std::min(left, p.size);
		std::memmove(buffer + (count - left), p.buffer + p.offset, s);
		if (s == left && (p.size != left))
		{
			p.size -= left;
			p.offset += left;
		}
		else
		{
			msgqueue->data->queue.pop();
		}
		left -= s;
	}
	//if (data->is_parent)
	//	fprintf(stdout, "Finished Reading %d/%d - %s/%s\n", count-left,count, data->is_parent ? "Parent" : "Child", data->region_name);
	return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring   
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;
	//if (data->is_parent)
	//{
	//	fprintf(stdout, "Starting To Read - %s/%s\n", data->is_parent ? "Parent" : "Child", data->region_name);
	//	fprintf(stdout, "Available Write %d - %s/%s\n", msgqueue->data->queue.write_available(), data->is_parent ? "Parent" : "Child", data->region_name);
	//	fprintf(stdout, "Available Read %d - %s/%s\n", msgqueue->data->queue.read_available(), data->is_parent ? "Parent" : "Child", data->region_name);
	//}
	char * buffer = (char *)buf;

	guac_packet & p = msgqueue->data->queue.front();
	size_t s = std::min(count, p.size);
	std::memcpy(buffer, p.buffer + p.offset, s);
	if (s == count && (p.size != count))
	{
		p.size -= count;
		p.offset += count;
	}
	else
	{
		msgqueue->data->queue.pop();
	}
	//if (data->is_parent)
	//	fprintf(stdout, "Finished Reading %d/%d - %s/%s\n", s, count, data->is_parent ? "Parent" : "Child", data->region_name);
	return s;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
	void * buf, size_t count)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
	return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	// Multi packet read, fill as much as possible to the buffer from the readable packets
	if (data->multi_packet_read)
	{
		return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
	}
	// Single packet read, read a single packet from the queue to the buffer
	else
	{
		return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	}
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
	const void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->parent_to_child_msgqueue
		: data->child_to_parent_msgqueue;

	char * buffer = (char *)buf;
	//if (data->is_parent)
	//{
	//	fprintf(stdout, "Writing %d size of %s - %s/%s\n", count, buffer, data->is_parent ? "Parent" : "Child", data->region_name);
	//	fprintf(stdout, "Available Write %d - %s/%s\n", msgqueue->data->queue.write_available(), data->is_parent ? "Parent" : "Child", data->region_name);
	//	fprintf(stdout, "Available Read %d - %s/%s\n", msgqueue->data->queue.read_available(), data->is_parent ? "Parent" : "Child", data->region_name);
	//}
	size_t left = count;
	while (left > 0 && msgqueue->data->queue.write_available() > 0)
	{
		guac_packet p;
		p.offset = 0;
		size_t s = std::min(left, (size_t)PACKET_SIZE);
		std::memcpy(p.buffer, buffer, s);
		p.size = s;
		msgqueue->data->queue.push(p);
		ReleaseSemaphore(msgqueue->semaphore, 1, NULL);
		// msgqueue->data->cond.notify_one();
		left -= s;
	}
	
	//if (data->is_parent)
	//	fprintf(stdout, "Wrote : %d/%d - %s/%s\n", count-left,count, data->is_parent ? "Parent" : "Child", data->region_name);
	return count - left;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.lock();
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.unlock();
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Reset the queues to notify everyone
	guac_socket_shared_memory_socket_reset_handler(socket);

	std::string strSM_PTC = data->region_name + "_PTC";
	std::string strSM_CTP = data->region_name + "_CTP";

	boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
	boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

	delete data;
	socket->data = nullptr;

	return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
	int usec_timeout)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
		: data->parent_to_child_msgqueue;

	// First check if there is already packets in the queue
	if (msgqueue->data->queue.read_available() > 0)
	{
		return 1;
	}

	// boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(msgqueue->data->mutex);

	// Get the target time which is the current time and the given extra MS
	//boost::posix_time::ptime target_time = boost::posix_time::microsec_clock::universal_time() +
	//	boost::posix_time::milliseconds(usec_timeout);
	//if (data->is_parent)
	//	fprintf(stdout, "Selecting - %s/%s\n", data->is_parent ? "Parent" : "Child", data->region_name);
	// Wait for the interprocess lock to receive a notification that there are packets ready
	if(WaitForSingleObject(msgqueue->semaphore, usec_timeout) == WAIT_TIMEOUT)
	// if(!msgqueue->data->cond.timed_wait(lock, target_time))
	{
		guac_error = GUAC_STATUS_TIMEOUT;
		guac_error_message = "Timeout while waiting for data on socket";
		return 0;
	}
	//if (data->is_parent)
	//	fprintf(stdout, "Select Popped Properly - %s/%s\n", data->is_parent ? "Parent" : "Child", data->region_name);
	// Make sure that there is actually data on the queue
	return 1;
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
	int packet_size)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = true;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();
	boost::interprocess::shared_memory_object::remove((shname + "_PTC").c_str());
	boost::interprocess::shared_memory_object::remove((shname + "_CTP").c_str());

	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_PTC").c_str(), 1024*1024));
	data->parent_to_child_msgqueue->data = 
		data->parent_to_child_msgqueue->shm->construct<guac_socket_shared_memory_msgqueue_data>("Ring")();

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::create_only, (shname + "_CTP").c_str(), 1024*1024));
	data->child_to_parent_msgqueue->data =
		data->child_to_parent_msgqueue->shm->construct<guac_socket_shared_memory_msgqueue_data>("Ring")();

	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;
	if (0 == InitializeSecurityDescriptor(&sd,
		SECURITY_DESCRIPTOR_REVISION) ||
		0 == SetSecurityDescriptorDacl(&sd,
			TRUE,
			(PACL)0,
			FALSE))
	{
		return nullptr;
	}
	else
	{
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = &sd;
		sa.bInheritHandle = FALSE;
	}

	data->parent_to_child_msgqueue->semaphore = CreateSemaphore(
		&sa,
		0,
		1024,
		("Global\\" + shname + "_PTC_SEM").c_str()
	);

	data->child_to_parent_msgqueue->semaphore = CreateSemaphore(
		&sa,
		0,
		1024,
		("Global\\" + shname + "_CTP_SEM").c_str()
	);


	/*
	fprintf(stdout, "%d/%d - %s/%s\n",
		data->parent_to_child_msgqueue->semaphore,
		data->child_to_parent_msgqueue->semaphore,
		data->is_parent ? "Parent" : "Child",
		shname.c_str());
		*/
	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = false;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;

	data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
	data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();

	/*
	fprintf(stdout, "%d/%d - %s/%s\n",
		data->parent_to_child_msgqueue->semaphore,
		data->child_to_parent_msgqueue->semaphore,
		data->is_parent ? "Parent" : "Child",
		shname.c_str());
		*/
	boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
	data->parent_to_child_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_PTC").c_str()));
	data->parent_to_child_msgqueue->data =
		data->parent_to_child_msgqueue->shm->find<guac_socket_shared_memory_msgqueue_data>("Ring").first;

	data->child_to_parent_msgqueue->shm.reset(
		new boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only, (shname + "_CTP").c_str()));
	data->child_to_parent_msgqueue->data =
		data->child_to_parent_msgqueue->shm->find<guac_socket_shared_memory_msgqueue_data>("Ring").first;


	data->parent_to_child_msgqueue->semaphore = OpenSemaphore(
		NULL,
		TRUE,
		("Global\\" + shname + "_PTC_SEM").c_str()
	);

	data->child_to_parent_msgqueue->semaphore = OpenSemaphore(
		NULL,
		TRUE,
		("Global\\" + shname + "_CTP_SEM").c_str()
	);

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

#endif
#endif
#ifdef BOOST_MSG_QUEUE
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

typedef struct guac_socket_shared_memory_msgqueue
{
   boost::interprocess::message_queue * msgqueue;
   char * read_buffer, *write_buffer;
   bool rdy;
   size_t curr_size;
   // HANDLE semaphore;
} guac_socket_shared_memory_msgqueue;
typedef struct guac_socket_shared_memory_data
{
   guac_socket_shared_memory_msgqueue * parent_to_child_msgqueue;
   guac_socket_shared_memory_msgqueue * child_to_parent_msgqueue;
   boost::mutex local_mutex;
   std::string region_name;
   bool is_parent;
   bool multi_packet_read;
   bool streamlined;
} guac_socket_shared_memory_data;

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
   // No buffer size, ignoring
   if(count == 0)
   {
      return 0;
   }

   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);


   // fprintf(stdout, "RC=%d,%s\n", count, data->is_parent ? "PARENT" : "CHILD");

   guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
      : data->parent_to_child_msgqueue;

   // We read as much as we can according to the buffer size
   char * buffer = (char *)buf;
   char * read_buffer = nullptr;
   size_t left = count;
   while (left > 0 && (msgqueue->rdy || msgqueue->msgqueue->get_num_msg() > 0))
   {
      size_t final_size;
      if (msgqueue->rdy)
      {
 		 read_buffer = msgqueue->read_buffer;
		 std::memcpy(buffer + (count - left),
			msgqueue->read_buffer, std::min(msgqueue->curr_size, left));
		
         if (msgqueue->curr_size > left)
         {
            std::memcpy(msgqueue->read_buffer, msgqueue->read_buffer + left,
               msgqueue->curr_size - left);
            msgqueue->rdy = true;
            msgqueue->curr_size = msgqueue->curr_size - left;
            final_size = left;
         }
         else
         {
            final_size = msgqueue->curr_size;
            msgqueue->rdy = false;
         }
      }
      else
      {
         try
         {
            size_t rcvd;
			uint32_t prio;
			if (count == msgqueue->msgqueue->get_max_msg_size() && left == count)
			{
				read_buffer = buffer;
				msgqueue->msgqueue->receive(read_buffer,
					msgqueue->msgqueue->get_max_msg_size(), rcvd, prio);
			}
			else
			{
				read_buffer = msgqueue->read_buffer;
				msgqueue->msgqueue->receive(read_buffer,
					msgqueue->msgqueue->get_max_msg_size(), rcvd, prio);
				std::memcpy(buffer + (count - left), msgqueue->read_buffer, std::min(rcvd, left));
			}
            if (rcvd > left)
            {
               std::memcpy(msgqueue->read_buffer, msgqueue->read_buffer + left, rcvd - left);
               msgqueue->rdy = true;
               msgqueue->curr_size = rcvd - left;
               final_size = left;
            }
            else
            {
               final_size = rcvd;
               msgqueue->rdy = false;
            }
         }
         catch (boost::interprocess::interprocess_exception & e)
         {
			 return 0;
         }
      }
      left -= final_size;
   }

   return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
   // No buffer size, ignoring   
   if(count == 0)
   {
      return 0;
   }

   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);                                                              

   guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
      : data->parent_to_child_msgqueue;

   char * buffer = (char *) buf;
   size_t final_size;

   if (msgqueue->rdy)
   {
      std::memcpy(buffer, msgqueue->read_buffer, std::min(msgqueue->curr_size, count));
      if (msgqueue->curr_size > count)
      {
         std::memcpy(msgqueue->read_buffer, msgqueue->read_buffer + count, msgqueue->curr_size - count);
         msgqueue->rdy = true;
         msgqueue->curr_size = msgqueue->curr_size - count;
         final_size = count;
      }
      else
      {
         final_size = msgqueue->curr_size;
         msgqueue->rdy = false;
      }
   }
   else
   {
      try
      {
         size_t rcvd;
		 uint32_t prio;
         msgqueue->msgqueue->receive(msgqueue->read_buffer, msgqueue->msgqueue->get_max_msg_size(), rcvd, prio);
         std::memcpy(buffer, msgqueue->read_buffer, std::min(rcvd, count));
         if (rcvd > count)
         {
            std::memcpy(msgqueue->read_buffer, msgqueue->read_buffer + count, rcvd - count);
            msgqueue->rdy = true;
            msgqueue->curr_size = rcvd - count;
            final_size = count;
         }
         else
         {
            final_size = rcvd;
            msgqueue->rdy = false;
         }
      }
      catch (boost::interprocess::interprocess_exception & e)
      {
		 return 0;
      }
   }

   return final_size;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
                                                            void * buf, size_t count)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

   // return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
   // Multi packet read, fill as much as possible to the buffer from the readable packets
   if(data->multi_packet_read)
   {
      return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
   }
   // Single packet read, read a single packet from the queue to the buffer
   else
   {
      return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
   }
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
                                                             const void * buf, size_t count)
{
   // No buffer size, ignoring
   if(count == 0)
   {
      return 0;
   }

   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);

   guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->parent_to_child_msgqueue
      : data->child_to_parent_msgqueue;

   //fprintf(stdout, "WC=%d,%s\n", count, data->is_parent ? "PARENT" : "CHILD");

   int left = count;
   while (left > 0)
   {
      char * buffer = (char *)buf;
      size_t s = std::min(count, msgqueue->msgqueue->get_max_msg_size());

      msgqueue->msgqueue->send(buffer + (count - left), s, 1);
	  // ReleaseSemaphore(msgqueue->semaphore, 1, NULL);
      left -= s;
   }
   return count;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);

   data->local_mutex.lock();
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);

   data->local_mutex.unlock();
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);

   // Reset the queues to notify everyone
   guac_socket_shared_memory_socket_reset_handler(socket);

   std::string strSM_PTC = data->region_name + "_PTC";
   std::string strSM_CTP = data->region_name + "_CTP";

   boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
   boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

   delete data;
   socket->data = nullptr;

   return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
           socket->data);

   return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
                                                           int usec_timeout)
{
   static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));

   guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

   guac_socket_shared_memory_msgqueue * msgqueue = data->is_parent ? data->child_to_parent_msgqueue
      : data->parent_to_child_msgqueue;

   // First check if there is already packets in the queue
   if(msgqueue->msgqueue->get_num_msg() > 0)
   {
      return 1;
   }

   // Get the target time which is the current time and the given extra MS
   boost::posix_time::ptime target_time = boost::posix_time::microsec_clock::universal_time() +
                                          boost::posix_time::milliseconds(usec_timeout);
   // Wait for the interprocess lock to receive a notification that there are packets ready
   try
   {
      size_t s;
      unsigned int prio;
	  // if (WaitForSingleObject(msgqueue->semaphore, usec_timeout) == WAIT_TIMEOUT)
      if(!msgqueue->msgqueue->timed_receive(msgqueue->read_buffer, msgqueue->msgqueue->get_max_msg_size(), s, prio, target_time))
      {
         msgqueue->rdy = false;
         guac_error = GUAC_STATUS_TIMEOUT;
         guac_error_message = "Timeout while waiting for data on socket";
         return 0;
      }
      else
      {
         msgqueue->rdy = true;
         msgqueue->curr_size = s;
      }
   }
   catch (...)
   {
      return 0;
   }
   // Make sure that there is actually data on the queue
   return msgqueue->rdy;
   //return 1;
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
                                        int packet_size)
{
   guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

   // Set the needed parameters
   data->region_name = shname;
   data->is_parent = true;
   data->multi_packet_read = multi_read;
   data->streamlined = streamlined;

   data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
   data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();

   data->parent_to_child_msgqueue->rdy = false;
   data->child_to_parent_msgqueue->rdy = false;

   boost::interprocess::message_queue::remove((shname + "_PTC").c_str());
   boost::interprocess::message_queue::remove((shname + "_CTP").c_str());

   size_t ptc_packet_size = packet_size;
   size_t ctp_packet_size = packet_size;

   if (streamlined)
   {
	  ptc_packet_size = 32768;
	  ctp_packet_size = 8192;
   }

   data->parent_to_child_msgqueue->msgqueue = new boost::interprocess::message_queue
      (boost::interprocess::create_only,
         (shname + "_PTC").c_str(),
         queue_size, ptc_packet_size);
   data->child_to_parent_msgqueue->msgqueue = 
      new boost::interprocess::message_queue
      (boost::interprocess::create_only,
         (shname + "_CTP").c_str(),
         queue_size, ctp_packet_size);

   data->parent_to_child_msgqueue->read_buffer =
      new char[ptc_packet_size];
   data->child_to_parent_msgqueue->read_buffer =
      new char[ctp_packet_size];

   data->parent_to_child_msgqueue->write_buffer =
      new char[ptc_packet_size];
   data->child_to_parent_msgqueue->write_buffer =
      new char[ctp_packet_size];

   guac_socket * socket = guac_socket_alloc();
   socket->data = data;

   // Set the handlers for the socket
   socket->read_handler = guac_socket_shared_memory_socket_read_handler;
   socket->write_handler = guac_socket_shared_memory_socket_write_handler;
   socket->select_handler = guac_socket_shared_memory_socket_select_handler;
   socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
   socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
   socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
   socket->free_handler = guac_socket_shared_memory_socket_free_handler;
   socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

   return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
   guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

   // Set the needed parameters
   data->region_name = shname;
   data->is_parent = false;
   data->multi_packet_read = multi_read;
   data->streamlined = streamlined;

   data->parent_to_child_msgqueue = new guac_socket_shared_memory_msgqueue();
   data->child_to_parent_msgqueue = new guac_socket_shared_memory_msgqueue();

   data->parent_to_child_msgqueue->rdy = false;
   data->child_to_parent_msgqueue->rdy = false;

   data->parent_to_child_msgqueue->msgqueue = new boost::interprocess::message_queue
      (boost::interprocess::open_only,
         (shname + "_PTC").c_str());
   data->child_to_parent_msgqueue->msgqueue = new boost::interprocess::message_queue
      (boost::interprocess::open_only,
         (shname + "_CTP").c_str());

   data->parent_to_child_msgqueue->read_buffer =
      new char[data->parent_to_child_msgqueue->msgqueue->get_max_msg_size()];
   data->child_to_parent_msgqueue->read_buffer =
      new char[data->child_to_parent_msgqueue->msgqueue->get_max_msg_size()];

   data->parent_to_child_msgqueue->write_buffer =
      new char[data->parent_to_child_msgqueue->msgqueue->get_max_msg_size()];
   data->child_to_parent_msgqueue->write_buffer =
      new char[data->child_to_parent_msgqueue->msgqueue->get_max_msg_size()];

   guac_socket * socket = guac_socket_alloc();
   socket->data = data;

   // Set the handlers for the socket
   socket->read_handler = guac_socket_shared_memory_socket_read_handler;
   socket->write_handler = guac_socket_shared_memory_socket_write_handler;
   socket->select_handler = guac_socket_shared_memory_socket_select_handler;
   socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
   socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
   socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
   socket->free_handler = guac_socket_shared_memory_socket_free_handler;
   socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

   return socket;
}

#endif
#endif
#ifdef LOCK_FREE_NORMAL_QUEUE
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#include "../../guacservice/include/guacservice/GuacLogger.h"

//#ifdef WIN32
//   #include <boost/interprocess/managed_windows_shared_memory.hpp>
//   typedef boost::interprocess::managed_windows_shared_memory ManagedSharedMemoryType;
//#else
#include <boost/interprocess/managed_shared_memory.hpp>

typedef boost::interprocess::managed_shared_memory ManagedSharedMemoryType;
//#endif

#define SHARED_MEMORY_INITIALIZED_SIZE 8*1024*1024
#define QUEUE_SHARED_MEMORY_NAME "RegionQueue"

typedef struct guac_socket_shared_memory_region_packet
{
	int current_packet_size;
	int current_read_byte;
	boost::interprocess::offset_ptr<char> packet_buffer;
} guac_socket_shared_memory_region_packet;
typedef struct guac_socket_shared_memory_region_queue
{
	boost::interprocess::interprocess_mutex queue_mutex;
	boost::interprocess::interprocess_condition_any queue_condition;
	std::atomic<int> queue_current_size;
	std::atomic<int> queue_read_marker;
	std::atomic<int> queue_write_marker;
	int queue_max_size;
	int queue_packet_size;
	boost::interprocess::offset_ptr<boost::interprocess::interprocess_semaphore> queue_sema;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_region_packet> queue;
} guac_socket_shared_memory_region_queue;
typedef struct guac_socket_shared_memory_region_data
{
	boost::shared_ptr<ManagedSharedMemoryType> managed_shm;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_region_queue> region_queue;
} guac_socket_shared_memory_region_data;
typedef struct guac_socket_shared_memory_data
{
	guac_socket_shared_memory_region_data * parent_to_child_queue;
	guac_socket_shared_memory_region_data * child_to_parent_queue;
	boost::mutex local_mutex;
	std::string region_name;
	bool is_parent;
	bool multi_packet_read;
	bool streamlined;
} guac_socket_shared_memory_data;

bool guac_socket_shared_memory_socket_queue_create(guac_socket_shared_memory_region_data * region_data,
	const std::string & shname, std::size_t queue_size,
	std::size_t packet_size)
{
	try
	{
		// Remove stale shared memory before creation, this rmeoves all the constructions aswell
		boost::interprocess::shared_memory_object::remove(shname.c_str());

		region_data->managed_shm.reset(new ManagedSharedMemoryType(boost::interprocess::create_only, shname.c_str(),
			SHARED_MEMORY_INITIALIZED_SIZE));

		// Allocate the packets and reset them
		region_data->region_queue = region_data->managed_shm->construct<guac_socket_shared_memory_region_queue>(
			QUEUE_SHARED_MEMORY_NAME)();
		region_data->region_queue->queue_current_size = 0;
		region_data->region_queue->queue_max_size = queue_size;
		region_data->region_queue->queue_packet_size = packet_size;
		region_data->region_queue->queue = region_data->managed_shm->construct<guac_socket_shared_memory_region_packet>(
			boost::interprocess::anonymous_instance)[queue_size]();
		region_data->region_queue->queue_read_marker = 0;
		region_data->region_queue->queue_write_marker = 0;
		region_data->region_queue->queue_sema =
			region_data->managed_shm->construct<boost::interprocess::interprocess_semaphore>("SEMAPHORE")(0);

		// Allocate the buffers for each packet
		for (size_t i = 0; i < queue_size; i++)
		{
			region_data->region_queue->queue[i].current_read_byte = 0;
			region_data->region_queue->queue[i].current_packet_size = 0;
			region_data->region_queue->queue[i].packet_buffer = region_data->managed_shm->construct<char>(
				boost::interprocess::anonymous_instance)[packet_size](0);
		}

		// Shrink the region to the exect needed size
		region_data->managed_shm->get_segment_manager()->shrink_to_fit();
	}
	catch (...)
	{
		return false;
	}
	return true;
}

bool guac_socket_shared_memory_socket_queue_join(guac_socket_shared_memory_region_data * region_data,
	const std::string & shname)
{
	try
	{
		// Joins the shared memory of this region, no need to initialize the inner members since they are offset_ptr
		region_data->managed_shm.reset(new ManagedSharedMemoryType(boost::interprocess::open_only, shname.c_str()));
		region_data->region_queue = region_data->managed_shm->find<guac_socket_shared_memory_region_queue>(
			QUEUE_SHARED_MEMORY_NAME).first;
	}
	catch (...)
	{
		return false;
	}
	return true;
}

void guac_socket_shared_memory_socket_queue_clear(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Resetting the queue params, no need to clear the buffers
	shm_queue->queue_current_size = 0;
	shm_queue->queue_read_marker = 0;
	shm_queue->queue_write_marker = 0;
	for (int i = 0; i < shm_queue->queue_max_size; i++)
	{
		shm_queue->queue[i].current_read_byte = 0;
		shm_queue->queue[i].current_packet_size = 0;
	}
}

void guac_socket_shared_memory_socket_queue_pop(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Queue is empty, nothing to pop
	if (shm_queue->queue_current_size == 0)
	{
		return;
	}

	// Reset the packet
	guac_socket_shared_memory_region_packet * packet = &shm_queue->queue[shm_queue->queue_read_marker];
	packet->current_read_byte = 0;
	packet->current_packet_size = 0;

	// Move the read marker since the packet is done being read
	shm_queue->queue_read_marker = (shm_queue->queue_read_marker + 1) % shm_queue->queue_max_size;

	// Decrease the queue size since the packet has been popped
	shm_queue->queue_current_size--;

	// If the queue is empty, reset the markers
	if (shm_queue->queue_current_size == 0)
	{
		shm_queue->queue_read_marker = 0;
		shm_queue->queue_write_marker = 0;
	}
}

guac_socket_shared_memory_region_packet *
guac_socket_shared_memory_socket_queue_start_push(guac_socket_shared_memory_region_queue * shm_queue, bool streamlined)
{
	// Cant push if queue is full
	if (shm_queue->queue_current_size == shm_queue->queue_max_size)
	{
		return nullptr;
	}

	// If there is not enough space on the current written packet and we are in streamline mode (efficency on packet space)
	// Move the write marker forward on the queue
	if (shm_queue->queue[shm_queue->queue_write_marker].current_packet_size == shm_queue->queue_packet_size &&
		streamlined)
	{
		shm_queue->queue_write_marker = (shm_queue->queue_write_marker + 1) % shm_queue->queue_max_size;
	}

	// Return the current written packet
	return &shm_queue->queue[shm_queue->queue_write_marker];
}

void
guac_socket_shared_memory_socket_queue_end_push(guac_socket_shared_memory_region_queue * shm_queue, bool streamlined)
{
	// On the push end, the packet is ready to be read, so we increase the queue size
	shm_queue->queue_current_size++;

	// If we are not streamlined (efficent on packet space), move the write marker forward when we are done writing to the packet
	// This ignores the fact that the packet might not be filled
	if (!streamlined)
	{
		shm_queue->queue_write_marker = (shm_queue->queue_write_marker + 1) % shm_queue->queue_max_size;
	}
}

guac_socket_shared_memory_region_packet *
guac_socket_shared_memory_socket_queue_peek(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Empty queue, cant peek the front
	if (shm_queue->queue_current_size == 0)
	{
		return nullptr;
	}

	// Peek the currently read packet
	return &shm_queue->queue[shm_queue->queue_read_marker];
}

bool guac_socket_shared_memory_socket_queue_empty(guac_socket_shared_memory_region_queue * shm_queue)
{
	return shm_queue->queue_current_size == 0;
}

int guac_socket_shared_memory_socket_queue_free_size(guac_socket_shared_memory_region_queue * shm_queue)
{
	return shm_queue->queue_max_size - shm_queue->queue_current_size;
}

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
	                                                                 : data->parent_to_child_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now
	boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(queue->queue_mutex);

	// First make sure that there are any packets to read
	if(guac_socket_shared_memory_socket_queue_empty(queue))
	{
	   return 0;
	}

	char * buffer = (char *)buf;
	int left = count;

	// We read as much as we can according to the buffer size
	while(left > 0 && !guac_socket_shared_memory_socket_queue_empty(queue))
	{
		//Get the packet
		guac_socket_shared_memory_region_packet * packet = guac_socket_shared_memory_socket_queue_peek(queue);

		// Calculate how much can be read
		int size = std::min(left, packet->current_packet_size);

		// Read the data and update
		std::memmove(buffer, packet->packet_buffer.get() + packet->current_read_byte, size);
		left -= size;
		packet->current_read_byte += size;
		packet->current_packet_size -= size;
		buffer += size;

		// Means the packet is done being read so we can pop it
		if(packet->current_packet_size == 0)
		{
			guac_socket_shared_memory_socket_queue_pop(queue);
		}
	}

	// Return the actual read size
	return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring   
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
	                                                                 : data->parent_to_child_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now 
	boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(queue->queue_mutex);

	// Queue is empty, nothing to read
	if(guac_socket_shared_memory_socket_queue_empty(queue))
	{
	   return 0;
	}

	// Get the packet      
	guac_socket_shared_memory_region_packet * packet = guac_socket_shared_memory_socket_queue_peek(queue);

	char * buffer = (char *)buf;
	int left = count;

	// Calculate how much can be read
	int size = std::min(left, packet->current_packet_size);

	// Read the data and update
	std::memmove(buffer, packet->packet_buffer.get() + packet->current_read_byte, size);
	left -= size;
	packet->current_read_byte += size;
	packet->current_packet_size -= size;

	// Means the packet is done being read so we can pop it
	if(packet->current_packet_size == 0)
	{
	   guac_socket_shared_memory_socket_queue_pop(queue);
	}

	// Return the actual read size   
	return count - left;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
	void * buf, size_t count)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Multi packet read, fill as much as possible to the buffer from the readable packets
	if (data->multi_packet_read)
	{
		return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
	}
	// Single packet read, read a single packet from the queue to the buffer
	else
	{
		return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	}
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
	const void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->parent_to_child_queue->region_queue.get()
	                                                                 : data->child_to_parent_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now
	boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(queue->queue_mutex);

	// In order to write the needed packets we need ceil(count / PACKET_SIZE) packets to be avaliable
	// We will write the max avaliable packets to the region
	int allocated_packets = std::min((int) std::ceil((double) count / (double) queue->queue_packet_size),
	                                 guac_socket_shared_memory_socket_queue_free_size(queue));

	// No place to write packets, notify the condition and finish
	if (allocated_packets == 0)
	{
	   return 0;
	}

	char * buffer = (char *)buf;
	int left = count;

	// Go over the allocated packets and start inputting the data
	for(int i = 0; i < allocated_packets; i++)
	{
		// Get the packet
		guac_socket_shared_memory_region_packet * packet =
	        guac_socket_shared_memory_socket_queue_start_push(queue, data->streamlined);

		// If the writing is not streamlined, we reset the packet here
		// Otherwise we try to max the usage of the packets space to avoid queue being filled faster then needed
		if(!data->streamlined)
		{
			packet->current_read_byte = 0;
			packet->current_packet_size = 0;
		}

		// Calculate the size to copy
		int size = std::min(left, queue->queue_packet_size - packet->current_packet_size);

		// Copy the data and update
		std::memmove(packet->packet_buffer.get() + packet->current_read_byte + packet->current_packet_size, buffer, size);
		left -= size;
		packet->current_packet_size += size;
		buffer += size;

		// Finish the queue push
		guac_socket_shared_memory_socket_queue_end_push(queue, data->streamlined);	
	 }

	 queue->queue_sema->post();
	 // Return all the actual bytes that were written
	 return count - left;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.lock();
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	data->local_mutex.unlock();
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Empty the queues      
	guac_socket_shared_memory_socket_queue_clear(data->child_to_parent_queue->region_queue.get());
	guac_socket_shared_memory_socket_queue_clear(data->parent_to_child_queue->region_queue.get());
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Reset the queues to notify everyone
	guac_socket_shared_memory_socket_reset_handler(socket);

	// Clear the shared memory
	data->child_to_parent_queue->managed_shm.reset();
	data->parent_to_child_queue->managed_shm.reset();

	std::string strSM_PTC = data->region_name + "_PTC";
	std::string strSM_CTP = data->region_name + "_CTP";

	boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
	boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

	delete data;
	socket->data = nullptr;

	return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
	int usec_timeout)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
	                                                                 : data->parent_to_child_queue->region_queue.get();

	// First check if there is already packets in the queue
	if(!guac_socket_shared_memory_socket_queue_empty(queue))
	{
		return 1;
	}

	// Get the target time which is the current time and the given extra MS
	boost::posix_time::ptime target_time = boost::posix_time::microsec_clock::universal_time() +
		boost::posix_time::milliseconds(usec_timeout);

	// Wait for the interprocess lock to receive a notification that there are packets ready
	if (!queue->queue_sema->timed_wait(target_time))
	{
		guac_error = GUAC_STATUS_TIMEOUT;
		guac_error_message = "Timeout while waiting for data on socket";
		return 0;
	}
	// Make sure that there is actually data on the queue
	return !guac_socket_shared_memory_socket_queue_empty(queue);
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
	int packet_size)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = true;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;
	data->parent_to_child_queue = new guac_socket_shared_memory_region_data();
	data->child_to_parent_queue = new guac_socket_shared_memory_region_data();

	// Create shared memory for each queue, to seperate workloads
	if(!guac_socket_shared_memory_socket_queue_create(data->parent_to_child_queue, shname + "_PTC", queue_size,
	                                                  packet_size) || !
	           guac_socket_shared_memory_socket_queue_create(data->child_to_parent_queue, shname + "_CTP", queue_size,
	                                                         packet_size))
	{
	   delete data;
	   return nullptr;
	}

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = false;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;
	data->parent_to_child_queue = new guac_socket_shared_memory_region_data();
	data->child_to_parent_queue = new guac_socket_shared_memory_region_data();

	// Join the shared memory for each queue
	if(!guac_socket_shared_memory_socket_queue_join(data->parent_to_child_queue, shname + "_PTC") || !
	        guac_socket_shared_memory_socket_queue_join(data->child_to_parent_queue, shname + "_CTP"))
	{
	   delete data;
	   return nullptr;
	}

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

#endif
#endif
#ifdef LOCKING_NORMAL_QUEUE
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

#include <boost/interprocess/detail/workaround.hpp>
// For Win32 specific, undef the generic implementation for windows impl
//#ifdef WIN32
//   #ifdef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//      #undef BOOST_INTERPROCESS_FORCE_GENERIC_EMULATION
//   #endif
//#endif
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_condition_any.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/upgradable_lock.hpp>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <guacamole/error.h>
#include <guacamole/socket.h>
//#ifdef WIN32
//   #include <boost/interprocess/managed_windows_shared_memory.hpp>
//   typedef boost::interprocess::managed_windows_shared_memory ManagedSharedMemoryType;
//#else
#include <boost/interprocess/managed_shared_memory.hpp>

typedef boost::interprocess::managed_shared_memory ManagedSharedMemoryType;
//#endif

#define SHARED_MEMORY_INITIALIZED_SIZE 4*1024*1024
#define QUEUE_SHARED_MEMORY_NAME "RegionQueue"

typedef struct guac_socket_shared_memory_region_packet
{
	int current_packet_size;
	int current_read_byte;
	boost::interprocess::offset_ptr<char> packet_buffer;
} guac_socket_shared_memory_region_packet;
typedef struct guac_socket_shared_memory_region_queue
{
	boost::interprocess::interprocess_upgradable_mutex queue_mutex;
	boost::interprocess::interprocess_condition_any queue_condition;
	int queue_current_size;
	int queue_max_size;
	int queue_packet_size;
	int queue_read_marker;
	int queue_write_marker;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_region_packet> queue;
} guac_socket_shared_memory_region_queue;
typedef struct guac_socket_shared_memory_region_data
{
	boost::shared_ptr<ManagedSharedMemoryType> managed_shm;
	boost::interprocess::offset_ptr<guac_socket_shared_memory_region_queue> region_queue;
} guac_socket_shared_memory_region_data;
typedef struct guac_socket_shared_memory_data
{
	guac_socket_shared_memory_region_data * parent_to_child_queue;
	guac_socket_shared_memory_region_data * child_to_parent_queue;
	boost::mutex local_mutex;
	std::string region_name;
	bool is_parent;
	bool multi_packet_read;
	bool streamlined;
} guac_socket_shared_memory_data;

bool guac_socket_shared_memory_socket_queue_create(guac_socket_shared_memory_region_data * region_data,
	const std::string & shname, std::size_t queue_size,
	std::size_t packet_size)
{
	try
	{
		// Remove stale shared memory before creation, this rmeoves all the constructions aswell
		boost::interprocess::shared_memory_object::remove(shname.c_str());

		region_data->managed_shm.reset(new ManagedSharedMemoryType(boost::interprocess::create_only, shname.c_str(),
			SHARED_MEMORY_INITIALIZED_SIZE));

		// Allocate the packets and reset them
		region_data->region_queue = region_data->managed_shm->construct<guac_socket_shared_memory_region_queue>(
			QUEUE_SHARED_MEMORY_NAME)();
		region_data->region_queue->queue_current_size = 0;
		region_data->region_queue->queue_max_size = queue_size;
		region_data->region_queue->queue_packet_size = packet_size;
		region_data->region_queue->queue = region_data->managed_shm->construct<guac_socket_shared_memory_region_packet>(
			boost::interprocess::anonymous_instance)[queue_size]();
		region_data->region_queue->queue_read_marker = 0;
		region_data->region_queue->queue_write_marker = 0;

		// Allocate the buffers for each packet
		for (size_t i = 0; i < queue_size; i++)
		{
			region_data->region_queue->queue[i].current_read_byte = 0;
			region_data->region_queue->queue[i].current_packet_size = 0;
			region_data->region_queue->queue[i].packet_buffer = region_data->managed_shm->construct<char>(
				boost::interprocess::anonymous_instance)[packet_size](0);
		}

		// Shrink the region to the exect needed size
		region_data->managed_shm->get_segment_manager()->shrink_to_fit();
	}
	catch (...)
	{
		return false;
	}
	return true;
}

bool guac_socket_shared_memory_socket_queue_join(guac_socket_shared_memory_region_data * region_data,
	const std::string & shname)
{
	try
	{
		// Joins the shared memory of this region, no need to initialize the inner members since they are offset_ptr
		region_data->managed_shm.reset(new ManagedSharedMemoryType(boost::interprocess::open_only, shname.c_str()));
		region_data->region_queue = region_data->managed_shm->find<guac_socket_shared_memory_region_queue>(
			QUEUE_SHARED_MEMORY_NAME).first;
	}
	catch (...)
	{
		return false;
	}
	return true;
}

void guac_socket_shared_memory_socket_queue_clear(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Resetting the queue params, no need to clear the buffers
	shm_queue->queue_current_size = 0;
	shm_queue->queue_read_marker = 0;
	shm_queue->queue_write_marker = 0;
	for (int i = 0; i < shm_queue->queue_max_size; i++)
	{
		shm_queue->queue[i].current_read_byte = 0;
		shm_queue->queue[i].current_packet_size = 0;
	}
}

void guac_socket_shared_memory_socket_queue_pop(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Queue is empty, nothing to pop
	if (shm_queue->queue_current_size == 0)
	{
		return;
	}

	// Reset the packet
	guac_socket_shared_memory_region_packet * packet = &shm_queue->queue[shm_queue->queue_read_marker];
	packet->current_read_byte = 0;
	packet->current_packet_size = 0;

	// Move the read marker since the packet is done being read
	shm_queue->queue_read_marker = (shm_queue->queue_read_marker + 1) % shm_queue->queue_max_size;

	// Decrease the queue size since the packet has been popped
	shm_queue->queue_current_size--;

	// If the queue is empty, reset the markers
	if (shm_queue->queue_current_size == 0)
	{
		shm_queue->queue_read_marker = 0;
		shm_queue->queue_write_marker = 0;
	}
}

guac_socket_shared_memory_region_packet *
guac_socket_shared_memory_socket_queue_start_push(guac_socket_shared_memory_region_queue * shm_queue, bool streamlined)
{
	// Cant push if queue is full
	if (shm_queue->queue_current_size == shm_queue->queue_max_size)
	{
		return nullptr;
	}

	// If there is not enough space on the current written packet and we are in streamline mode (efficency on packet space)
	// Move the write marker forward on the queue
	if (shm_queue->queue[shm_queue->queue_write_marker].current_packet_size == shm_queue->queue_packet_size &&
		streamlined)
	{
		shm_queue->queue_write_marker = (shm_queue->queue_write_marker + 1) % shm_queue->queue_max_size;
	}

	// Return the current written packet
	return &shm_queue->queue[shm_queue->queue_write_marker];
}

void
guac_socket_shared_memory_socket_queue_end_push(guac_socket_shared_memory_region_queue * shm_queue, bool streamlined)
{
	// On the push end, the packet is ready to be read, so we increase the queue size
	shm_queue->queue_current_size++;

	// If we are not streamlined (efficent on packet space), move the write marker forward when we are done writing to the packet
	// This ignores the fact that the packet might not be filled
	if (!streamlined)
	{
		shm_queue->queue_write_marker = (shm_queue->queue_write_marker + 1) % shm_queue->queue_max_size;
	}
}

guac_socket_shared_memory_region_packet *
guac_socket_shared_memory_socket_queue_peek(guac_socket_shared_memory_region_queue * shm_queue)
{
	// Empty queue, cant peek the front
	if (shm_queue->queue_current_size == 0)
	{
		return nullptr;
	}

	// Peek the currently read packet
	return &shm_queue->queue[shm_queue->queue_read_marker];
}

bool guac_socket_shared_memory_socket_queue_empty(guac_socket_shared_memory_region_queue * shm_queue)
{
	return shm_queue->queue_current_size == 0;
}

int guac_socket_shared_memory_socket_queue_free_size(guac_socket_shared_memory_region_queue * shm_queue)
{
	return shm_queue->queue_max_size - shm_queue->queue_current_size;
}

size_t guac_socket_shared_memory_socket_multi_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
		: data->parent_to_child_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now
	boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(queue->queue_mutex);

	// First make sure that there are any packets to read
	if (guac_socket_shared_memory_socket_queue_empty(queue))
	{
		return 0;
	}

	// We read as much as we can according to the buffer size
	char * buffer = (char *)buf;
	int left = count;
	while (left > 0 && !guac_socket_shared_memory_socket_queue_empty(queue))
	{
		// Get the packet
		guac_socket_shared_memory_region_packet * packet = guac_socket_shared_memory_socket_queue_peek(queue);

		// Calculate how much can be read
		int size = std::min(left, packet->current_packet_size);

		// Read the data and update
		std::memmove(buffer, packet->packet_buffer.get() + packet->current_read_byte, size);
		left -= size;
		packet->current_read_byte += size;
		packet->current_packet_size -= size;
		buffer += size;

		// Means the packet is done being read so we can pop it
		if (packet->current_packet_size == 0)
		{
			guac_socket_shared_memory_socket_queue_pop(queue);
		}
	}

	// Return the actual read size
	return count - left;
}

size_t guac_socket_shared_memory_socket_single_packet_read(guac_socket * socket, void * buf, size_t count)
{
	// No buffer size, ignoring   
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
		: data->parent_to_child_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now 
	boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(queue->queue_mutex);

	// Queue is empty, nothing to read
	if (guac_socket_shared_memory_socket_queue_empty(queue))
	{
		return 0;
	}

	char * buffer = (char *)buf;
	int left = count;

	// Get the packet      
	guac_socket_shared_memory_region_packet * packet = guac_socket_shared_memory_socket_queue_peek(queue);

	// Calculate how much can be read
	int size = std::min(left, packet->current_packet_size);

	// Read the data and update
	std::memmove(buffer, packet->packet_buffer.get() + packet->current_read_byte, size);
	left -= size;
	packet->current_read_byte += size;
	packet->current_packet_size -= size;

	// Means the packet is done being read so we can pop it
	if (packet->current_packet_size == 0)
	{
		guac_socket_shared_memory_socket_queue_pop(queue);
	}

	// Return the actual read size   
	return count - left;
}

static size_t guac_socket_shared_memory_socket_read_handler(guac_socket * socket,
	void * buf, size_t count)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Multi packet read, fill as much as possible to the buffer from the readable packets
	if (data->multi_packet_read)
	{
		return guac_socket_shared_memory_socket_multi_packet_read(socket, buf, count);
	}
	// Single packet read, read a single packet from the queue to the buffer
	else
	{
		return guac_socket_shared_memory_socket_single_packet_read(socket, buf, count);
	}
}

static size_t guac_socket_shared_memory_socket_write_handler(guac_socket * socket,
	const void * buf, size_t count)
{
	// No buffer size, ignoring
	if (count == 0)
	{
		return 0;
	}

	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->parent_to_child_queue->region_queue.get()
		: data->child_to_parent_queue->region_queue.get();

	// Lock the region mutex, reading and writing at the same time is not allowed for now
	boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> lock(queue->queue_mutex);

	// In order to write the needed packets we need ceil(count / PACKET_SIZE) packets to be avaliable
	// We will write the max avaliable packets to the region
	int allocated_packets = std::min((int)std::ceil((double)count / (double)queue->queue_packet_size),
		guac_socket_shared_memory_socket_queue_free_size(queue));

	// No place to write packets, notify the condition and finish
	if (allocated_packets == 0)
	{
		queue->queue_condition.notify_all();
		return 0;
	}

	char * buffer = (char *)buf;
	int left = count;

	// Go over the allocated packets and start inputting the data
	for (int i = 0; i < allocated_packets; i++)
	{
		// Get the packet
		guac_socket_shared_memory_region_packet * packet =
			guac_socket_shared_memory_socket_queue_start_push(queue, data->streamlined);

		// If the writing is not streamlined, we reset the packet here
		// Otherwise we try to max the usage of the packets space to avoid queue being filled faster then needed
		if (!data->streamlined)
		{
			packet->current_read_byte = 0;
			packet->current_packet_size = 0;
		}

		// Calculate the size to copy
		int size = std::min(left, queue->queue_packet_size - packet->current_packet_size);

		// Copy the data and update
		std::memmove(packet->packet_buffer.get() + packet->current_read_byte + packet->current_packet_size, buffer, size);
		left -= size;
		packet->current_packet_size += size;
		buffer += size;

		// Finish the queue push
		guac_socket_shared_memory_socket_queue_end_push(queue, data->streamlined);

		// Notify that there are new packets to be read in case the other side is selecting
		// Only notify for the first packet, to pop the select as earliest as possible
		// No need to notify for others due to the queue lock
		// Note that the lock is sharable so many read one write
		// if (i == 0)
		// {
			
		// }
	}

	queue->queue_condition.notify_all();
	// Return all the actual bytes that were written
	return count - left;
}

static void guac_socket_shared_memory_socket_lock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);
}

static void guac_socket_shared_memory_socket_unlock_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);
}

static void guac_socket_shared_memory_socket_reset_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Empty the queues      
	guac_socket_shared_memory_socket_queue_clear(data->child_to_parent_queue->region_queue.get());
	guac_socket_shared_memory_socket_queue_clear(data->parent_to_child_queue->region_queue.get());

	// Notify conditions to stop blocked threads      
	{
		boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(
			data->child_to_parent_queue->region_queue->queue_mutex);
		data->child_to_parent_queue->region_queue->queue_condition.notify_all();
	}
	{
		boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(
			data->parent_to_child_queue->region_queue->queue_mutex);
		data->parent_to_child_queue->region_queue->queue_condition.notify_all();
	}
}

static int guac_socket_shared_memory_socket_free_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	// Reset the queues to notify everyone
	guac_socket_shared_memory_socket_reset_handler(socket);

	// Clear the shared memory
	data->child_to_parent_queue->managed_shm.reset();
	data->parent_to_child_queue->managed_shm.reset();

	std::string strSM_PTC = data->region_name + "_PTC";
	std::string strSM_CTP = data->region_name + "_CTP";

	boost::interprocess::shared_memory_object::remove(strSM_PTC.c_str());
	boost::interprocess::shared_memory_object::remove(strSM_CTP.c_str());

	delete data;
	socket->data = nullptr;

	return 0;
}

static size_t guac_socket_shared_memory_socket_flush_handler(guac_socket * socket)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(
		socket->data);

	return 0;
}

static int guac_socket_shared_memory_socket_select_handler(guac_socket * socket,
	int usec_timeout)
{
	guac_socket_shared_memory_data * data = static_cast<guac_socket_shared_memory_data *>(socket->data);

	// Get the working queue
	guac_socket_shared_memory_region_queue * queue = data->is_parent ? data->child_to_parent_queue->region_queue.get()
		: data->parent_to_child_queue->region_queue.get();

	// First check if there is already packets in the queue
	if (!guac_socket_shared_memory_socket_queue_empty(queue))
	{
		return 1;
	}

	// If there arent any packets ready, lock with timeout on the region conditional variable
	boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(queue->queue_mutex);

	// Get the target time which is the current time and the given extra MS
	boost::posix_time::ptime target_time = boost::posix_time::second_clock::local_time() +
		boost::posix_time::milliseconds(usec_timeout);

	// Wait for the interprocess lock to receive a notification that there are packets ready
	if (!queue->queue_condition.timed_wait(lock, target_time))
	{
		guac_error = GUAC_STATUS_TIMEOUT;
		guac_error_message = "Timeout while waiting for data on socket";
		return 0;
	}

	// Make sure that there is actually data on the queue
	return !guac_socket_shared_memory_socket_queue_empty(queue);
}

guac_socket *
guac_socket_shared_memory_socket_create(const std::string & shname, bool multi_read, bool streamlined, int queue_size,
	int packet_size)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = true;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;
	data->parent_to_child_queue = new guac_socket_shared_memory_region_data();
	data->child_to_parent_queue = new guac_socket_shared_memory_region_data();

	// Create shared memory for each queue, to seperate workloads
	if (!guac_socket_shared_memory_socket_queue_create(data->parent_to_child_queue, shname + "_PTC", queue_size,packet_size) || !
		guac_socket_shared_memory_socket_queue_create(data->child_to_parent_queue, shname + "_CTP", queue_size,packet_size))
	{
		delete data;
		return nullptr;
	}

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

guac_socket * guac_socket_shared_memory_socket_join(const std::string & shname, bool multi_read, bool streamlined)
{
	guac_socket_shared_memory_data * data = new guac_socket_shared_memory_data();

	// Set the needed parameters
	data->region_name = shname;
	data->is_parent = false;
	data->multi_packet_read = multi_read;
	data->streamlined = streamlined;
	data->parent_to_child_queue = new guac_socket_shared_memory_region_data();
	data->child_to_parent_queue = new guac_socket_shared_memory_region_data();

	// Join the shared memory for each queue
	if (!guac_socket_shared_memory_socket_queue_join(data->parent_to_child_queue, shname + "_PTC") || !
		guac_socket_shared_memory_socket_queue_join(data->child_to_parent_queue, shname + "_CTP"))
	{
		delete data;
		return nullptr;
	}

	guac_socket * socket = guac_socket_alloc();
	socket->data = data;

	// Set the handlers for the socket
	socket->read_handler = guac_socket_shared_memory_socket_read_handler;
	socket->write_handler = guac_socket_shared_memory_socket_write_handler;
	socket->select_handler = guac_socket_shared_memory_socket_select_handler;
	socket->flush_handler = guac_socket_shared_memory_socket_flush_handler;
	socket->lock_handler = guac_socket_shared_memory_socket_lock_handler;
	socket->unlock_handler = guac_socket_shared_memory_socket_unlock_handler;
	socket->free_handler = guac_socket_shared_memory_socket_free_handler;
	socket->reset_handler = guac_socket_shared_memory_socket_reset_handler;

	return socket;
}

#endif
#endif