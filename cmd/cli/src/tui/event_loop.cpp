#include "euxis/cli/tui/event_loop.hpp"
#include "euxis/cli/terminal.hpp"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace euxis::cli::tui {

namespace {
// Global signal pipe for SIGWINCH forwarding.
int g_signal_pipe_write = -1;

void sigwinch_handler(int) {
    if (g_signal_pipe_write >= 0) {
        char byte = 'W';
        [[maybe_unused]] auto _ = ::write(g_signal_pipe_write, &byte, 1);
    }
}

void sigexit_handler(int sig) {
    if (g_signal_pipe_write >= 0) {
        char byte = (sig == SIGINT) ? 'I' : 'T';
        [[maybe_unused]] auto _ = ::write(g_signal_pipe_write, &byte, 1);
    }
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
} // namespace

EventLoop::EventLoop() : handlers_(5) {
    // Create self-pipe for cross-thread posting.
    int pipe_fds[2];
    if (::pipe(pipe_fds) == 0) {
        self_pipe_read_ = pipe_fds[0];
        self_pipe_write_ = pipe_fds[1];
        set_nonblocking(self_pipe_read_);
        set_nonblocking(self_pipe_write_);
    }

    // Create signal pipe for SIGWINCH.
    if (::pipe(pipe_fds) == 0) {
        signal_pipe_read_ = pipe_fds[0];
        signal_pipe_write_ = pipe_fds[1];
        set_nonblocking(signal_pipe_read_);
        set_nonblocking(signal_pipe_write_);
    }
}

EventLoop::~EventLoop() {
    quit();
    // Restore default SIGWINCH handler if we installed ours.
    if (g_signal_pipe_write == signal_pipe_write_) {
        std::signal(SIGWINCH, SIG_DFL);
        g_signal_pipe_write = -1;
    }
    if (self_pipe_read_ >= 0) ::close(self_pipe_read_);
    if (self_pipe_write_ >= 0) ::close(self_pipe_write_);
    if (signal_pipe_read_ >= 0) ::close(signal_pipe_read_);
    if (signal_pipe_write_ >= 0) ::close(signal_pipe_write_);
}

void EventLoop::setup_signal_pipe() {
    g_signal_pipe_write = signal_pipe_write_;
    struct sigaction sa{};
    sa.sa_handler = sigwinch_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, nullptr);

    sa.sa_handler = sigexit_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

void EventLoop::on(EventType type, Handler handler) {
    auto idx = static_cast<size_t>(type);
    if (idx < handlers_.size()) {
        handlers_[idx].push_back(std::move(handler));
    }
}

int EventLoop::add_timer(int interval_ms, Handler handler) {
    std::lock_guard lock(timer_mutex_);
    int id = next_timer_id_++;
    timers_.push({id, interval_ms, true, now_ms() + interval_ms, std::move(handler)});
    return id;
}

int EventLoop::add_oneshot(int delay_ms, Handler handler) {
    std::lock_guard lock(timer_mutex_);
    int id = next_timer_id_++;
    timers_.push({id, delay_ms, false, now_ms() + delay_ms, std::move(handler)});
    return id;
}

void EventLoop::remove_timer(int id) {
    std::lock_guard lock(timer_mutex_);
    // Rebuild the queue without the removed timer.
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<>> filtered;
    while (!timers_.empty()) {
        auto t = timers_.top();
        timers_.pop();
        if (t.id != id) filtered.push(std::move(t));
    }
    timers_ = std::move(filtered);
}

void EventLoop::post(Event event) {
    {
        std::lock_guard lock(post_mutex_);
        posted_events_.push(std::move(event));
    }
    // Wake up poll() via self-pipe.
    if (self_pipe_write_ >= 0) {
        char byte = 'P';
        [[maybe_unused]] auto _ = ::write(self_pipe_write_, &byte, 1);
    }
}

void EventLoop::run() {
    running_.store(true, std::memory_order_relaxed);
    setup_signal_pipe();

    while (running_.load(std::memory_order_relaxed)) {
        // Compute poll timeout from nearest timer.
        int timeout_ms = 100; // Default 100ms for responsive quit
        {
            std::lock_guard lock(timer_mutex_);
            if (!timers_.empty()) {
                int64_t until = timers_.top().next_fire_ms - now_ms();
                timeout_ms = std::max(1, static_cast<int>(std::min(static_cast<int64_t>(timeout_ms), until)));
            }
        }

        struct pollfd fds[3];
        int nfds = 0;

        // stdin
        fds[nfds++] = {STDIN_FILENO, POLLIN, 0};
        // self-pipe
        if (self_pipe_read_ >= 0)
            fds[nfds++] = {self_pipe_read_, POLLIN, 0};
        // signal-pipe
        if (signal_pipe_read_ >= 0)
            fds[nfds++] = {signal_pipe_read_, POLLIN, 0};

        int ret = ::poll(fds, nfds, timeout_ms);
        if (ret < 0) continue; // EINTR or error, just retry

        // Process stdin
        if (fds[0].revents & POLLIN) process_stdin();

        // Process self-pipe (posted events)
        int sp_idx = 1;
        if (self_pipe_read_ >= 0 && sp_idx < nfds && fds[sp_idx].revents & POLLIN) {
            process_self_pipe();
        }

        // Process signal-pipe
        int sig_idx = (self_pipe_read_ >= 0) ? 2 : 1;
        if (signal_pipe_read_ >= 0 && sig_idx < nfds && fds[sig_idx].revents & POLLIN) {
            process_signal_pipe();
        }

        // Fire ready timers.
        fire_timers();
    }
}

void EventLoop::quit() {
    running_.store(false, std::memory_order_relaxed);
}

void EventLoop::process_stdin() {
    unsigned char c;
    if (::read(STDIN_FILENO, &c, 1) == 1) {
        Event ev{EventType::Key, static_cast<int>(c)};

        // Handle Ctrl+key (0x01 to 0x1A, excluding 0x0D for Enter)
        if (c >= 1 && c <= 26 && c != 13) {
            // Keep the raw control code
        }
        // Handle ESC sequences
        else if (c == '\x1b') {
            unsigned char seq[5];
            // Try to read more bytes (non-blocking, as stdin may already be non-blocking
            // in raw mode with VMIN=0)
            if (::read(STDIN_FILENO, &seq[0], 1) == 1) {
                if (seq[0] == '[') {
                    if (::read(STDIN_FILENO, &seq[1], 1) == 1) {
                        // Standard arrow keys: ESC [ A/B/C/D
                        if (seq[1] >= 'A' && seq[1] <= 'D') {
                            ev.key = 1001 + (seq[1] - 'A');
                        }
                        // Extended sequences: ESC [ <num> ~  or  ESC [ 1 ; <mod> <dir>
                        else if (seq[1] >= '0' && seq[1] <= '9') {
                            if (::read(STDIN_FILENO, &seq[2], 1) == 1) {
                                if (seq[2] == '~') {
                                    // ESC [ <n> ~
                                    switch (seq[1]) {
                                        case '1': ev.key = 1010; break; // Home
                                        case '4': ev.key = 1011; break; // End
                                        case '5': ev.key = 1012; break; // PgUp
                                        case '6': ev.key = 1013; break; // PgDn
                                        case '3': ev.key = 1014; break; // Delete
                                    }
                                } else if (seq[1] == '1' && seq[2] == ';') {
                                    // ESC [ 1 ; <mod> <dir> (Ctrl/Shift+arrow)
                                    unsigned char mod_byte, dir_byte;
                                    if (::read(STDIN_FILENO, &mod_byte, 1) == 1 &&
                                        ::read(STDIN_FILENO, &dir_byte, 1) == 1) {
                                        int mod = mod_byte - '0';
                                        int base = 0;
                                        if (dir_byte >= 'A' && dir_byte <= 'D') base = 1001 + (dir_byte - 'A');
                                        if (mod == 5 && base > 0) ev.key = base + 100; // Ctrl+arrow: 1101-1104
                                        else if (mod == 2 && base > 0) ev.key = base + 200; // Shift+arrow: 1201-1204
                                    }
                                }
                                // F-keys: ESC [ 1 5 ~ = F5, etc.
                                else if (seq[2] >= '0' && seq[2] <= '9') {
                                    unsigned char tilde;
                                    if (::read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                                        int code = (seq[1] - '0') * 10 + (seq[2] - '0');
                                        // Map CSI codes to sequential F-key constants
                                        switch (code) {
                                            case 15: ev.key = 1020; break; // F5
                                            case 17: ev.key = 1021; break; // F6
                                            case 18: ev.key = 1022; break; // F7
                                            case 19: ev.key = 1023; break; // F8
                                            case 20: ev.key = 1024; break; // F9
                                            case 21: ev.key = 1025; break; // F10
                                            case 23: ev.key = 1026; break; // F11
                                            case 24: ev.key = 1027; break; // F12
                                            default: break;
                                        }
                                    }
                                }
                            }
                        }
                        // Home: ESC [ H
                        else if (seq[1] == 'H') ev.key = 1010;
                        // End: ESC [ F
                        else if (seq[1] == 'F') ev.key = 1011;
                    }
                } else {
                    // Alt+key: ESC <char>
                    ev.key = 2000 + seq[0]; // Alt+key range: 2000+
                }
            }
            // Bare ESC (no following bytes)
            // ev.key stays as 27 (0x1b)
        }

        dispatch(ev);
    }
}

void EventLoop::process_self_pipe() {
    char buf[64];
    while (::read(self_pipe_read_, buf, sizeof(buf)) > 0) {}

    std::queue<Event> local;
    {
        std::lock_guard lock(post_mutex_);
        std::swap(local, posted_events_);
    }
    while (!local.empty()) {
        dispatch(local.front());
        local.pop();
    }
}

void EventLoop::process_signal_pipe() {
    char buf[64];
    ssize_t n = ::read(signal_pipe_read_, buf, sizeof(buf));
    if (n <= 0) return;

    for (ssize_t i = 0; i < n; ++i) {
        if (buf[i] == 'W') {
            // Emit resize event with current terminal size.
            int w = 80, h = 24;
            terminal::get_terminal_size(w, h);
            dispatch({EventType::Resize, 0, w, h});
        } else if (buf[i] == 'I' || buf[i] == 'T') {
            // Emit signal event
            dispatch({EventType::Signal, 0, 0, 0, 0, (buf[i] == 'I' ? SIGINT : SIGTERM)});
        }
    }
}

void EventLoop::fire_timers() {
    std::lock_guard lock(timer_mutex_);
    auto now = now_ms();

    while (!timers_.empty() && timers_.top().next_fire_ms <= now) {
        auto entry = timers_.top();
        timers_.pop();

        Event ev{EventType::Timer};
        ev.timer_id = entry.id;
        entry.handler(ev);

        if (entry.repeating) {
            entry.next_fire_ms = now + entry.interval_ms;
            timers_.push(std::move(entry));
        }
    }
}

void EventLoop::dispatch(const Event& event) {
    auto idx = static_cast<size_t>(event.type);
    if (idx < handlers_.size()) {
        for (auto& handler : handlers_[idx]) {
            handler(event);
        }
    }
}

int64_t EventLoop::now_ms() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace euxis::cli::tui
