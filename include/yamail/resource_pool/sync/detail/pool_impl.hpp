#ifndef YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP

#include <list>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {
namespace detail {

template <class Value, class ConditionVariable>
class pool_impl : boost::noncopyable {
public:
    typedef Value value_type;
    typedef ConditionVariable condition_variable;
    typedef boost::shared_ptr<value_type> pointer;
    typedef boost::chrono::system_clock::duration time_duration;
    typedef boost::chrono::seconds seconds;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef std::pair<boost::system::error_code, list_iterator> get_result;

    pool_impl(std::size_t capacity)
            : _capacity(assert_capacity(capacity)),
              _available_size(0),
              _used_size(0),
              _disabled(false)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    const condition_variable& has_available_cv() const { return _has_available; }

    get_result get(time_duration wait_duration = seconds(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    list _available;
    list _used;
    const std::size_t _capacity;
    condition_variable _has_available;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    bool has_available() const { return !_available.empty(); }
    bool wait_for(unique_lock& lock, time_duration wait_duration);
};

template <class T, class C>
std::size_t pool_impl<T, C>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class T, class C>
std::size_t pool_impl<T, C>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class T, class C>
std::size_t pool_impl<T, C>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class T, class C>
void pool_impl<T, C>::recycle(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_available.notify_one();
}

template <class T, class C>
void pool_impl<T, C>::waste(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    _has_available.notify_one();
}

template <class T, class C>
void pool_impl<T, C>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    _has_available.notify_all();
}

template <class T, class C>
typename pool_impl<T, C>::get_result pool_impl<T, C>::get(time_duration wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        return std::make_pair(make_error_code(error::disabled), list_iterator());
    }
    if (_available_size == 0 && fit_capacity()) {
        const list_iterator res_it = _used.insert(_used.end(), pointer());
        ++_used_size;
        return std::make_pair(boost::system::error_code(), res_it);
    }
    if (!wait_for(lock, wait_duration)) {
        return std::make_pair(make_error_code(error::get_resource_timeout),
            list_iterator());
    }
    if (_disabled) {
        return std::make_pair(make_error_code(error::disabled), list_iterator());
    }
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    return std::make_pair(boost::system::error_code(), res_it);
}

template <class T, class C>
bool pool_impl<T, C>::wait_for(unique_lock& lock, time_duration wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this));
}

template <class T, class C>
std::size_t pool_impl<T, C>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

}
}
}
}

#endif
