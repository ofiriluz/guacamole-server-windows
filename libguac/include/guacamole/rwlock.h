#ifndef _RW_LOCK_HPP_
#define _RW_LOCK_HPP_

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic/atomic.hpp>
#include <atomic>
#include <limits>

namespace rwlock {

	enum WaitGenre {
		SPIN,
		SLEEP
	};

	template <WaitGenre WAIT_GENRE>
	class RWLock {

		boost::mutex wait_mutex_;
		boost::condition_variable wait_cv_;
		boost::atomic_int ref_counter_;
		const int MIN_INT;

	public:

		RWLock();

		void LockWrite();

		bool TryLockWrite();

		void UnLockWrite();

		void LockRead();

		bool TryLockRead();

		void UnLockRead();
	};
};

#endif