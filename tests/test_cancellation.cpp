#include <catch2/catch_test_macros.hpp>
#include <fakeit.hpp>
#include <aw/tpl/tpl.hpp>

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;
using namespace aw::tpl;
using namespace fakeit;

namespace {

struct IWorker {
    virtual ~IWorker() = default;
    virtual int step(int x) = 0;
    virtual void on_cancel() = 0;
};

} // namespace

TEST_CASE("CancellationToken::none is never canceled", "[cancellation]") {
    auto token = CancellationToken::none();
    REQUIRE_FALSE(token.is_cancellation_requested());
    REQUIRE_NOTHROW(token.throw_if_cancellation_requested());
    REQUIRE_NOTHROW(token.register_callback([] {}));
}

TEST_CASE("CancellationTokenSource handles manual cancel, idempotency, and late callbacks", "[cancellation]") {
    CancellationTokenSource cts;
    auto token = cts.token();

    std::atomic<int> callback_hits{0};
    token.register_callback([&] { ++callback_hits; });

    cts.cancel();
    cts.cancel(); // Second call must be a no-op

    REQUIRE(token.is_cancellation_requested());
    REQUIRE(callback_hits.load() == 1);
    REQUIRE_THROWS_AS(token.throw_if_cancellation_requested(), OperationCanceledException);

    // Registering callback after cancellation must execute immediately
    token.register_callback([&] { ++callback_hits; });
    REQUIRE(callback_hits.load() == 2);
}

TEST_CASE("async_sleep aborts immediately if token is already canceled or via cancel_after", "[cancellation][sleep]") {
    // 1. Pre-canceled token (hits await_ready() -> throws immediately)
    CancellationTokenSource pre_cts;
    pre_cts.cancel();

    auto pre_canceled_task = [](CancellationToken t) -> Task<void> {
        co_await async_sleep(1000ms, t);
    };
    REQUIRE_THROWS_AS(pre_canceled_task(pre_cts.token()).wait(), OperationCanceledException);

    // 2. Canceled mid-flight via cancel_after
    CancellationTokenSource timed_cts;
    timed_cts.cancel_after(30ms);

    auto mid_flight_task = [](CancellationToken t) -> Task<void> {
        co_await async_sleep(1000ms, t);
    };
    REQUIRE_THROWS_AS(mid_flight_task(timed_cts.token()).wait(), OperationCanceledException);
}

TEST_CASE("Cooperative cancellation with FakeIt mock verification", "[cancellation][mock]") {
    Mock<IWorker> mock;
    When(Method(mock, step)).AlwaysDo([](int x) { return x + 1; });
    Fake(Method(mock, on_cancel));

    CancellationTokenSource cts;

    auto pipeline = [&](CancellationToken token) -> Task<int> {
        int val = mock.get().step(10);
        try {
            co_await async_sleep(500ms, token);
            co_return mock.get().step(val);
        } catch (const OperationCanceledException&) {
            mock.get().on_cancel();
            throw;
        }
    };

    auto task = pipeline(cts.token());
    std::this_thread::sleep_for(25ms);
    cts.cancel();

    REQUIRE_THROWS_AS(task.result(), OperationCanceledException);
    Verify(Method(mock, step).Using(10)).Once();
    Verify(Method(mock, step).Using(11)).Never();
    Verify(Method(mock, on_cancel)).Once();
}
