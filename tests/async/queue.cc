#include <yamail/resource_pool/async/detail/queue.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

typedef queue<request, mocked_io_service, mocked_timer> request_queue;
typedef boost::shared_ptr<request_queue> request_queue_ptr;

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

struct async_request_queue : Test {
    mocked_io_service ios;
    mocked_callback_ptr expired;
    boost::function<void (error_code)> on_async_wait;

    async_request_queue() : expired(make_shared<mocked_callback>()) {}

    request_queue_ptr make_queue(std::size_t capacity) {
        return make_shared<request_queue>(ref(ios), capacity);
    }
};

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()() const { return impl->call(); }

private:
    mocked_callback_ptr impl;
};

TEST_F(async_request_queue, push_then_timeout_request_queue_should_be_empty) {
    request_queue_ptr queue = make_queue(1);

    time_point expire_time;

    InSequence s;

    EXPECT_CALL(queue->timer(), expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    EXPECT_CALL(queue->timer(), async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));

    ASSERT_TRUE(queue->push(request(), callback(expired), seconds(0)));

    EXPECT_CALL(queue->timer(), expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired, call()).WillOnce(Return());

    on_async_wait(error_code());

    EXPECT_TRUE(queue->empty());
}

TEST_F(async_request_queue, push_then_pop_should_return_request) {
    request_queue_ptr queue = make_queue(1);

    InSequence s;

    EXPECT_CALL(queue->timer(), expires_at(_)).WillOnce(Return());
    EXPECT_CALL(queue->timer(), async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    EXPECT_CALL(*expired, call()).Times(0);

    EXPECT_TRUE(queue->push(request(), callback(expired), seconds(1)));

    EXPECT_FALSE(queue->empty());
    request_queue::value_type result;
    EXPECT_TRUE(queue->pop(result));
}

TEST_F(async_request_queue, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_queue(0);

    const bool result = queue->push(request(), callback(expired), seconds(0));
    EXPECT_FALSE(result);
}

TEST_F(async_request_queue, pop_from_empty_should_return_error) {
    request_queue_ptr queue = make_queue(1);

    EXPECT_TRUE(queue->empty());
    request_queue::value_type result;
    EXPECT_FALSE(queue->pop(result));
}

}
