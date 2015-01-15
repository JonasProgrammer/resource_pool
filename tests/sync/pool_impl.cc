#include <boost/make_shared.hpp>
#include <gtest/gtest.h>
#include <yamail/resource_pool.hpp>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync::detail;

using boost::make_shared;

struct resource {};

typedef boost::shared_ptr<resource> resource_ptr;
typedef pool_impl<resource_ptr> resource_pool_impl;
typedef resource_pool_impl::get_result get_result;

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

struct sync_resource_pool_impl : Test {};

TEST(sync_resource_pool_impl, get_one_and_recycle_succeed) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.recycle(*res.second);
}

TEST(sync_resource_pool_impl, get_one_and_waste_succeed) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.waste(*res.second);
}

TEST(sync_resource_pool_impl, get_more_than_capacity_should_throw_exception) {
    resource_pool_impl pool_impl(1, make_resource);
    pool_impl.get();
    EXPECT_EQ(pool_impl.get().first, error::get_resource_timeout);
}

}
