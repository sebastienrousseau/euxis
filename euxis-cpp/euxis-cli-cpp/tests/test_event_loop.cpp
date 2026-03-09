#include <gtest/gtest.h>
#include "euxis/cli/tui/event_loop.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace euxis::cli::tui {
namespace {

TEST(EventLoopTest, ConstructDestruct) {
    EventLoop loop;
    EXPECT_FALSE(loop.running());
}

TEST(EventLoopTest, QuitBeforeRun) {
    EventLoop loop;
    loop.quit();
    EXPECT_FALSE(loop.running());
}

TEST(EventLoopTest, PostEventWakesLoop) {
    EventLoop loop;
    std::atomic<bool> received{false};

    loop.on(EventType::Custom, [&](const Event& ev) {
        if (ev.custom_id == 42) {
            received = true;
            loop.quit();
        }
    });

    std::thread runner([&]() { loop.run(); });

    // Give loop time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Event ev{EventType::Custom};
    ev.custom_id = 42;
    loop.post(ev);

    runner.join();
    EXPECT_TRUE(received);
}

TEST(EventLoopTest, TimerFires) {
    EventLoop loop;
    std::atomic<int> count{0};

    loop.add_timer(10, [&](const Event&) {
        if (++count >= 3) loop.quit();
    });

    std::thread runner([&]() { loop.run(); });
    runner.join();

    EXPECT_GE(count.load(), 3);
}

TEST(EventLoopTest, OneshotTimerFiresOnce) {
    EventLoop loop;
    std::atomic<int> count{0};

    loop.add_oneshot(10, [&](const Event&) {
        count++;
    });

    // Add a kill timer to ensure we exit
    loop.add_oneshot(100, [&](const Event&) {
        loop.quit();
    });

    std::thread runner([&]() { loop.run(); });
    runner.join();

    EXPECT_EQ(count.load(), 1);
}

TEST(EventLoopTest, RemoveTimer) {
    EventLoop loop;
    std::atomic<int> count{0};

    int id = loop.add_timer(10, [&](const Event&) {
        count++;
    });
    loop.remove_timer(id);

    loop.add_oneshot(50, [&](const Event&) {
        loop.quit();
    });

    std::thread runner([&]() { loop.run(); });
    runner.join();

    EXPECT_EQ(count.load(), 0);
}

TEST(EventLoopTest, MultipleHandlersSameType) {
    EventLoop loop;
    std::atomic<int> a{0}, b{0};

    loop.on(EventType::Custom, [&](const Event&) { a++; });
    loop.on(EventType::Custom, [&](const Event&) { b++; });

    std::thread runner([&]() { loop.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.post({EventType::Custom});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.quit();

    runner.join();
    EXPECT_EQ(a.load(), 1);
    EXPECT_EQ(b.load(), 1);
}

TEST(EventLoopTest, PostMultipleEvents) {
    EventLoop loop;
    std::atomic<int> sum{0};

    loop.on(EventType::Custom, [&](const Event& ev) {
        sum += ev.custom_id;
    });

    std::thread runner([&]() { loop.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Event e1{EventType::Custom};
    e1.custom_id = 10;
    Event e2{EventType::Custom};
    e2.custom_id = 20;
    Event e3{EventType::Custom};
    e3.custom_id = 30;

    loop.post(e1);
    loop.post(e2);
    loop.post(e3);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.quit();
    runner.join();

    EXPECT_EQ(sum.load(), 60);
}

TEST(EventLoopTest, TimerIdIsUnique) {
    EventLoop loop;
    int id1 = loop.add_timer(100, [](const Event&) {});
    int id2 = loop.add_timer(100, [](const Event&) {});
    int id3 = loop.add_oneshot(100, [](const Event&) {});
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST(EventLoopTest, QuitFromHandler) {
    EventLoop loop;

    loop.add_oneshot(1, [&](const Event&) {
        loop.quit();
    });

    // Should not hang
    loop.run();
    EXPECT_FALSE(loop.running());
}

} // namespace
} // namespace euxis::cli::tui
