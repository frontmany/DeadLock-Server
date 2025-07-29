#pragma once
#include <deque>
#include <condition_variable>
#include <mutex>

namespace net {

	template <typename T>
	class SafeDeque {
	public:
		SafeDeque() = default;
		SafeDeque(const SafeDeque<T>&) = delete;
		virtual ~SafeDeque() { clear(); }

	public: 
		const T& front() {
			std::scoped_lock lock(m_mtx);
			return m_deque.front();
		}

		const T& back() {
			std::scoped_lock lock(m_mtx);
			return m_deque.back();
		}

		const size_t size() {
			std::scoped_lock lock(m_mtx);
			return m_deque.size();
		}

		void push_front(const T& item) {
			std::scoped_lock lock(m_mtx);
			m_deque.emplace_front(std::move(item));

			std::unique_lock<std::mutex> uniqueLock(m_mtx_blocking);
			m_cv_blocking.notify_one();
		}

		void push_back(const T& item) {
			std::scoped_lock lock(m_mtx);
			m_deque.emplace_back(std::move(item));

			std::unique_lock<std::mutex> uniqueLock(m_mtx_blocking);
			m_cv_blocking.notify_one();
		}

		size_t count(){
			std::scoped_lock lock(m_mtx);
			return m_deque.count();
		}

		bool empty() {
			std::scoped_lock lock(m_mtx);
			return m_deque.empty();
		}

		void clear() {
			std::scoped_lock lock(m_mtx);
			m_deque.clear();
		}

		T pop_front() {
			std::scoped_lock lock(m_mtx);
			auto t = std::move(m_deque.front());
			m_deque.pop_front();
			return t;
		}

		T pop_back() {
			std::scoped_lock lock(m_mtx);
			auto t = std::move(m_deque.back());
			m_deque.pop_back();
			return t;
		}

	protected:
		std::mutex m_mtx;
		std::deque<T> m_deque;

		std::condition_variable m_cv_blocking;
		std::mutex				m_mtx_blocking;
	};

}