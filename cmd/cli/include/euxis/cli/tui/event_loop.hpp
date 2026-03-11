#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

namespace euxis::cli::tui {

/// Event types that flow through the loop.
enum class EventType { Key, Resize, Timer, Signal, Custom };

/// Unified event structure.
struct Event {
    EventType type{EventType::Key};
    int key{0};             // For Key events: raw keycode
    int width{0};           // For Resize events
    int height{0};          // For Resize events
    int timer_id{0};        // For Timer events
    int signal_num{0};      // For Signal events
    int custom_id{0};       // For Custom events
};

/// Handler callback type.
using Handler = std::function<void(const Event&)>;

/// Timer entry for the internal heap.
struct TimerEntry {
    int id;
    int interval_ms;
    bool repeating;
    int64_t next_fire_ms;
    Handler handler;

    bool operator>(const TimerEntry& o) const { return next_fire_ms > o.next_fire_ms; }
};

/// poll()-based event loop with multiplexed stdin, self-pipe, and signal-pipe.
/// Portable across Linux, macOS, and WSL (uses poll(), not epoll).
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Non-copyable, non-movable.
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /// Register a handler for a specific event type.
    void on(EventType type, Handler handler);

    /// Schedule a repeating timer. Returns timer ID.
    int add_timer(int interval_ms, Handler handler);

    /// Schedule a one-shot timer. Returns timer ID.
    int add_oneshot(int delay_ms, Handler handler);

    /// Remove a timer by ID.
    void remove_timer(int id);

    /// Post a custom event from any thread (thread-safe via self-pipe).
    void post(Event event);

    /// Run the event loop (blocks until quit() is called).
    void run();

    /// Request the loop to stop (thread-safe).
    void quit();

    /// Returns true if the loop is currently running.
    [[nodiscard]] bool running() const { return running_.load(std::memory_order_relaxed); }

private:
    void setup_signal_pipe();
    void process_stdin();
    void process_self_pipe();
    void process_signal_pipe();
    void fire_timers();
    void dispatch(const Event& event);
    int64_t now_ms() const;

    std::atomic<bool> running_{false};
    int self_pipe_read_{-1};
    int self_pipe_write_{-1};
    int signal_pipe_read_{-1};
    int signal_pipe_write_{-1};

    int next_timer_id_{1};
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<>> timers_;
    std::mutex timer_mutex_;

    std::mutex post_mutex_;
    std::queue<Event> posted_events_;

    // Handlers indexed by EventType (up to 5 types).
    std::vector<std::vector<Handler>> handlers_;
};

} // namespace euxis::cli::tui
