#ifndef _RW_LOCK_IMPL_HPP_
#define _RW_LOCK_IMPL_HPP_

#include <guacamole/rwlock.h>

namespace rwlock {
	template <WaitGenre WAIT_GENRE>
	RWLock<WAIT_GENRE>::RWLock() :
		ref_counter_(0),
		MIN_INT(std::numeric_limits<int>::min()) {}

	template <>
	void RWLock<SPIN>::LockWrite() {

		int expected = 0;
		while (!ref_counter_.compare_exchange_weak(expected, MIN_INT,
			boost::memory_order_acquire,
			boost::memory_order_relaxed)) {
			expected = 0;
		}
	}
	template <>
	void RWLock<SLEEP>::LockWrite() {
		int expected = 0;
		if (!ref_counter_.compare_exchange_strong(expected, MIN_INT,
			boost::memory_order_acquire,
			boost::memory_order_relaxed)) {
			expected = 0;

			boost::unique_lock<boost::mutex> lk(wait_mutex_);
			wait_cv_.wait(lk, [this, &expected] {
				if (!ref_counter_.compare_exchange_strong(expected, MIN_INT,
					boost::memory_order_acquire,
					boost::memory_order_relaxed)) {
					expected = 0;
					return false;
				}
				return true;
			});
			lk.unlock();
		}
	}

	template <WaitGenre WAIT_GENRE>
	bool RWLock<WAIT_GENRE>::TryLockWrite() {
		int expected = 0;
		return ref_counter_.compare_exchange_strong(expected, MIN_INT,
			boost::memory_order_acquire,
			boost::memory_order_relaxed);
	}

	template <WaitGenre WAIT_GENRE>
	void RWLock<WAIT_GENRE>::UnLockWrite() {
		ref_counter_.store(0, boost::memory_order_release);
		if (WAIT_GENRE == SLEEP)
			wait_cv_.notify_all();
	}

	template <>
	void RWLock<SPIN>::LockRead() {
		while (ref_counter_.fetch_add(1, boost::memory_order_acquire) < 0) {
			ref_counter_.fetch_sub(1, boost::memory_order_release);
		}
	}
	template <>
	void RWLock<SLEEP>::LockRead() {
		if (ref_counter_.fetch_add(1, boost::memory_order_acquire) < 0) {
			ref_counter_.fetch_sub(1, boost::memory_order_release);

			boost::unique_lock<boost::mutex> lk(wait_mutex_);
			wait_cv_.wait(lk, [this] {
				return ref_counter_.fetch_add(1, boost::memory_order_acquire) >= 0;
			});
			lk.unlock();
		}
	}

	template <WaitGenre WAIT_GENRE>
	bool RWLock<WAIT_GENRE>::TryLockRead() {
		return ref_counter_.fetch_add(1, boost::memory_order_acquire) >= 0;
	}

	template <WaitGenre WAIT_GENRE>
	void RWLock<WAIT_GENRE>::UnLockRead() {
		ref_counter_.fetch_sub(1, boost::memory_order_release);
		if (WAIT_GENRE == SLEEP)
			wait_cv_.notify_one();
	}

	template <WaitGenre WAIT_GENRE>
	class ScopeReadLock {
		RWLock<WAIT_GENRE> &lock_;
	public:
		ScopeReadLock(RWLock<WAIT_GENRE> &lock) : lock_(lock) {
			lock_.LockRead();
		}

		~ScopeReadLock() {
			lock_.UnLockRead();
		}
	};

	template <WaitGenre WAIT_GENRE>
	class ScopeWriteLock {
		RWLock<WAIT_GENRE> &lock_;
	public:
		ScopeWriteLock(RWLock<WAIT_GENRE> &lock) : lock_(lock) {
			lock_.LockWrite();
		}

		~ScopeWriteLock() {
			lock_.UnLockWrite();
		}
	};

};

#endif