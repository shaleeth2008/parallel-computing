// spsc_ring_buffer.cpp
// Lock-free Single-Producer Single-Consumer ring buffer (C++17).
//
// Build:
//   g++ -std=c++17 -O2 -pthread -Wall -Wextra -o spsc spsc_ring_buffer.cpp
// ThreadSanitizer build (use -O1, TSan is slow at -O2 on some setups):
//   g++ -std=c++17 -O1 -g -pthread -fsanitize=thread -o spsc_tsan spsc_ring_buffer.cpp
//
// Design rules (explain these in your README):
//   * Only the PRODUCER ever writes pushCursor_.
//   * Only the CONSUMER ever writes popCursor_.
//   * Producer publishes a slot with a RELEASE store to pushCursor_;
//     consumer sees it with an ACQUIRE load of pushCursor_. This guarantees
//     the slot's data is visible before the consumer reads it.
//   * Consumer frees a slot with a RELEASE store to popCursor_; producer sees
//     it with an ACQUIRE load, so it never overwrites data still being read.
//   * Each thread reads its OWN cursor with relaxed ordering (nobody else
//     writes it) and keeps a cached copy of the other side's cursor, so the
//     shared cache line is only touched when the buffer looks full/empty.
//   * The four hot variables sit on separate cache lines (alignas(64)) to
//     avoid false sharing.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

constexpr std::size_t kCacheLine = 64;

// Sample payload. Must be trivial: slots are plain array elements and are
// never constructed/destroyed individually.
struct Foo {
    int bar;
    int blat;
};

template <typename T>
class SpscRing {
    static_assert(std::is_trivial_v<T>, "T must be a trivial type");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "size_t atomics must be lock-free");

    static std::size_t validate(std::size_t capacity) {
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("capacity must be a non-zero power of 2");
        }
        return capacity;
    }

    const std::size_t capacity_;
    const std::size_t mask_;
    std::unique_ptr<T[]> ring_;

    // Producer side: pushCursor_ (shared) + cachedPopCursor_ (producer-only)
    alignas(kCacheLine) std::atomic<std::size_t> pushCursor_{0};
    alignas(kCacheLine) std::size_t cachedPopCursor_{0};
    // Consumer side: popCursor_ (shared) + cachedPushCursor_ (consumer-only)
    alignas(kCacheLine) std::atomic<std::size_t> popCursor_{0};
    alignas(kCacheLine) std::size_t cachedPushCursor_{0};

    T* slot(std::size_t cursor) const noexcept { return ring_.get() + (cursor & mask_); }

public:
    explicit SpscRing(std::size_t capacity)
        : capacity_(validate(capacity)),
          mask_(capacity_ - 1),
          ring_(new T[capacity_]()) {}

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    // ---------------- Producer handle ----------------
    // Fill the slot through the handle; the item becomes visible to the
    // consumer when the handle is destroyed (or reassigned).
    class Pusher {
        SpscRing* q_ = nullptr;
        std::size_t cursor_ = 0;

        void commit() noexcept {
            if (q_) {
                q_->pushCursor_.store(cursor_ + 1, std::memory_order_release);
                q_ = nullptr;
            }
        }

    public:
        Pusher() = default;
        Pusher(SpscRing* q, std::size_t cursor) : q_(q), cursor_(cursor) {}
        Pusher(const Pusher&) = delete;
        Pusher& operator=(const Pusher&) = delete;
        Pusher(Pusher&& o) noexcept : q_(std::exchange(o.q_, nullptr)), cursor_(o.cursor_) {}
        Pusher& operator=(Pusher&& o) noexcept {
            if (this != &o) {
                commit();
                q_ = std::exchange(o.q_, nullptr);
                cursor_ = o.cursor_;
            }
            return *this;
        }
        ~Pusher() { commit(); }

        explicit operator bool() const noexcept { return q_ != nullptr; }
        T* operator->() const noexcept { return q_->slot(cursor_); }
        T& operator*() const noexcept { return *q_->slot(cursor_); }
    };

    // Returns an empty handle if the buffer is full.
    Pusher push() noexcept {
        const std::size_t push = pushCursor_.load(std::memory_order_relaxed);
        if (push - cachedPopCursor_ == capacity_) {
            cachedPopCursor_ = popCursor_.load(std::memory_order_acquire);
            if (push - cachedPopCursor_ == capacity_) return Pusher{};
        }
        return Pusher(this, push);
    }

    // ---------------- Consumer handle ----------------
    // Read the slot through the handle; the slot is freed for the producer
    // when the handle is destroyed (or reassigned).
    class Popper {
        SpscRing* q_ = nullptr;
        std::size_t cursor_ = 0;

        void commit() noexcept {
            if (q_) {
                q_->popCursor_.store(cursor_ + 1, std::memory_order_release);
                q_ = nullptr;
            }
        }

    public:
        Popper() = default;
        Popper(SpscRing* q, std::size_t cursor) : q_(q), cursor_(cursor) {}
        Popper(const Popper&) = delete;
        Popper& operator=(const Popper&) = delete;
        Popper(Popper&& o) noexcept : q_(std::exchange(o.q_, nullptr)), cursor_(o.cursor_) {}
        Popper& operator=(Popper&& o) noexcept {
            if (this != &o) {
                commit();
                q_ = std::exchange(o.q_, nullptr);
                cursor_ = o.cursor_;
            }
            return *this;
        }
        ~Popper() { commit(); }

        explicit operator bool() const noexcept { return q_ != nullptr; }
        T* operator->() const noexcept { return q_->slot(cursor_); }
        T& operator*() const noexcept { return *q_->slot(cursor_); }
    };

    // Returns an empty handle if the buffer is empty.
    Popper pop() noexcept {
        const std::size_t pop = popCursor_.load(std::memory_order_relaxed);
        if (cachedPushCursor_ == pop) {
            cachedPushCursor_ = pushCursor_.load(std::memory_order_acquire);
            if (cachedPushCursor_ == pop) return Popper{};
        }
        return Popper(this, pop);
    }
};

// Build the whole line first, then print it with ONE call so lines from the
// two threads do not get mixed together.
static void log_line(const std::string& s) { std::cout << s; }

// ---------------------------------------------------------------------------
// PART 1: small buffer + logging, so it fills up and wraps around.
// Returns true if the consumer saw every item, in order.
// ---------------------------------------------------------------------------
static bool run_demo() {
    constexpr int kItems = 12;
    SpscRing<Foo> q(4);  // tiny on purpose: forces "full" and "empty" cases
    bool ok = true;

    std::thread producer([&] {
        for (int i = 0; i < kItems; ++i) {
            auto p = q.push();
            while (!p) {
                std::this_thread::yield();
                p = q.push();
            }
            p->bar = i * 10;
            p->blat = i * 20;
            std::ostringstream os;
            os << "[Producer] Pushed: bar = " << p->bar << ", blat = " << p->blat << '\n';
            log_line(os.str());
        }  // handle destroyed here -> item published
    });

    std::thread consumer([&] {
        int count = 0;
        while (count < kItems) {
            auto p = q.pop();
            if (!p) {
                std::this_thread::yield();
                continue;
            }
            if (p->bar != count * 10 || p->blat != count * 20) ok = false;
            std::ostringstream os;
            os << "[Consumer] Popped: bar = " << p->bar << ", blat = " << p->blat << '\n';
            log_line(os.str());
            ++count;
        }  // handle destroyed here -> slot freed
    });

    producer.join();
    consumer.join();
    return ok;
}

// ---------------------------------------------------------------------------
// PART 2: throughput benchmark WITH a correctness check.
// ---------------------------------------------------------------------------
static bool run_benchmark() {
    constexpr long long kTotal = 5'000'000;
    SpscRing<Foo> q(1 << 16);

    long long order_errors = 0;
    unsigned long long checksum = 0;

    // Note: elapsed time includes thread start-up and join.
    const auto start = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (long long i = 0; i < kTotal; ++i) {
            auto p = q.push();
            while (!p) {
                std::this_thread::yield();
                p = q.push();
            }
            p->bar = static_cast<int>(i);
            p->blat = static_cast<int>(i * 2);
        }
    });

    std::thread consumer([&] {
        for (long long count = 0; count < kTotal;) {
            auto p = q.pop();
            if (!p) {
                std::this_thread::yield();
                continue;
            }
            if (p->bar != static_cast<int>(count)) ++order_errors;
            checksum += static_cast<unsigned long long>(p->blat);
            ++count;
        }
    });

    producer.join();
    consumer.join();

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

    // sum of blat = 2 * (0 + 1 + ... + (N-1)) = N * (N-1)
    const unsigned long long expected =
        static_cast<unsigned long long>(kTotal) * static_cast<unsigned long long>(kTotal - 1);
    const bool ok = (order_errors == 0 && checksum == expected);

    std::cout << "Total operations : " << kTotal << " pushes and pops\n"
              << "Elapsed time     : " << elapsed.count() << " s\n"
              << "Throughput       : " << static_cast<double>(kTotal) / elapsed.count()
              << " items/s\n"
              << "Order errors     : " << order_errors << '\n'
              << "Checksum         : " << (checksum == expected ? "OK" : "MISMATCH") << '\n';
    return ok;
}

int main() {
    std::cout << "=== PART 1: logging demo (capacity 4, 12 items) ===\n";
    const bool demo_ok = run_demo();
    std::cout << "Demo result: " << (demo_ok ? "PASS" : "FAIL") << "\n\n";

    std::cout << "=== PART 2: benchmark (5M items, capacity 65536) ===\n";
    const bool bench_ok = run_benchmark();
    std::cout << "Benchmark result: " << (bench_ok ? "PASS" : "FAIL") << '\n';

    return (demo_ok && bench_ok) ? 0 : 1;
}
