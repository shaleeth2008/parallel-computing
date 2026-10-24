#include <iostream>
#include <atomic>
#include <new>
#include <thread>
#include <cassert>
#include <stdexcept>

// Define hardware interference size for cache alignment (typically 64 bytes)
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr size_t hardware_destructive_interference_size = 64;
#endif

// A sample data payload structure (matching "Foo" from the presentation)
struct Foo {
    int bar;
    int blat;
};

// Ensure it is trivial so we can manage lifetime cleanly via placement-new / direct pointers
static_assert(std::is_trivial_v<Foo>, "Foo must be a trivial type");

template <typename T, typename Alloc = std::allocator<T>>
class Fifo5 : private Alloc {
private:
    size_t capacity_;
    T* ring_;

    static_assert(std::atomic<size_t>::is_always_lock_free, "size_t atomic must be lock-free");

    // Fully cache-aligned cursors and local cache copies to eliminate false sharing and bus traffic
    alignas(hardware_destructive_interference_size) std::atomic<size_t> pushCursor_{0};
    alignas(hardware_destructive_interference_size) size_t cachedPopCursor_{0};

    alignas(hardware_destructive_interference_size) std::atomic<size_t> popCursor_{0};
    alignas(hardware_destructive_interference_size) size_t cachedPushCursor_{0};

    // Helper to get pointer to ring buffer slot
    T* element(size_t cursor) const noexcept {
        return ring_ + (cursor & (capacity_ - 1)); // Assumes capacity is a power of 2
    }

    bool full(size_t pushCursor, size_t popCursor) const noexcept {
        return (pushCursor - popCursor) == capacity_;
    }

    bool empty(size_t pushCursor, size_t popCursor) const noexcept {
        return pushCursor == popCursor;
    }

public:
    // Forward declaration of RAII Pusher Handle
    class pusher_t;
    // Forward declaration of RAII Popper Handle
    class popper_t;

    // Constructor: Capacity must be a power of 2
    explicit Fifo5(size_t capacity, const Alloc& alloc = Alloc())
        : Alloc(alloc), capacity_(capacity) {
        // Enforce power of 2 check
        assert((capacity & (capacity - 1)) == 0 && "Capacity must be a power of 2");
        
        // Allocate raw memory for the ring buffer
        ring_ = std::allocator_traits<Alloc>::allocate(*this, capacity_);
    }

    ~Fifo5() {
        // Clean up remaining unconsumed elements if necessary
        size_t push = pushCursor_.load(std::memory_order_relaxed);
        size_t pop = popCursor_.load(std::memory_order_relaxed);
        while (pop != push) {
            element(pop)->~T();
            ++pop;
        }
        std::allocator_traits<Alloc>::deallocate(*this, ring_, capacity_);
    }

    // Non-copyable, non-movable for safety
    Fifo5(const Fifo5&) = delete;
    Fifo5& operator=(const Fifo5&) = delete;

    // ==========================================
    // RAII PUSHER HANDLE (Zero-Copy Push)
    // ==========================================
    class pusher_t {
        Fifo5* fifo_{nullptr};
        size_t cursor_{0};

    public:
        pusher_t() = default;
        explicit pusher_t(Fifo5* fifo, size_t cursor) : fifo_(fifo), cursor_(cursor) {}

        // Non-copyable, but movable
        pusher_t(const pusher_t&) = delete;
        pusher_t& operator=(const pusher_t&) = delete;

        pusher_t(pusher_t&& other) noexcept 
            : fifo_(other.fifo_), cursor_(other.cursor_) {
            other.fifo_ = nullptr;
        }

        pusher_t& operator=(pusher_t&& other) noexcept {
            if (this != &other) {
                if (fifo_) {
                    // Cleanup current if abandoned
                    fifo_->pushCursor_.store(cursor_ + 1, std::memory_order_release);
                }
                fifo_ = other.fifo_;
                cursor_ = other.cursor_;
                other.fifo_ = nullptr;
            }
            return *this;
        }

        // Destructor automatically publishes the release store! (RAII Safety)
        ~pusher_t() {
            if (fifo_) {
                fifo_->pushCursor_.store(cursor_ + 1, std::memory_order_release);
            }
        }

        explicit operator bool() const noexcept { return fifo_ != nullptr; }
        void release() noexcept { fifo_ = nullptr; } // Cancel publishing if exception occurs

        T* get() const noexcept { return fifo_->element(cursor_); }
        T* operator->() const noexcept { return get(); }
        T& operator*() const noexcept { return *get(); }
    };

    // Factory method to acquire a pusher handle
    pusher_t push() {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        if (full(pushCursor, cachedPopCursor_)) {
            cachedPopCursor_ = popCursor_.load(std::memory_order_acquire);
            if (full(pushCursor, cachedPopCursor_)) {
                return pusher_t{}; // Queue is full
            }
        }
        // Construct object in-place using placement new
        T* ptr = element(pushCursor);
        ::new (static_cast<void*>(ptr)) T(); 
        return pusher_t(this, pushCursor);
    }

    // ==========================================
    // RAII POPPER HANDLE (Zero-Copy Pop)
    // ==========================================
    class popper_t {
        Fifo5* fifo_{nullptr};
        size_t cursor_{0};

    public:
        popper_t() = default;
        explicit popper_t(Fifo5* fifo, size_t cursor) : fifo_(fifo), cursor_(cursor) {}

        ~popper_t() {
            if (fifo_) {
                // Destroy object and release slot back to producer
                fifo_->element(cursor_)->~T();
                fifo_->popCursor_.store(cursor_ + 1, std::memory_order_release);
            }
        }

        explicit operator bool() const noexcept { return fifo_ != nullptr; }
        void release() noexcept { fifo_ = nullptr; }

        T* get() const noexcept { return fifo_->element(cursor_); }
        T* operator->() const noexcept { return get(); }
        T& operator*() const noexcept { return *get(); }
    };

    // Factory method to acquire a popper handle
    popper_t pop() {
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        if (empty(cachedPushCursor_, popCursor)) {
            cachedPushCursor_ = pushCursor_.load(std::memory_order_acquire);
            if (empty(cachedPushCursor_, popCursor)) {
                return popper_t{}; // Queue is empty
            }
        }
        return popper_t(this, popCursor);
    }
};

// ==========================================
// EXAMPLE TEST / USAGE
// ==========================================
int main() {
    Fifo5<Foo> fifo(16); // Capacity must be power of 2

    // Producer Thread simulation
    std::thread producer([&]() {
        for (int i = 0; i < 5; ++i) {
            auto pusher = fifo.push();
            while (!pusher) {
                // Yield or retry if full
                std::this_thread::yield();
                pusher = fifo.push();
            }

            // Zero-copy write straight into the queue buffer!
            pusher->bar = i * 10;
            pusher->blat = i * 20;
            std::cout << "[Producer] Pushed: bar = " << pusher->bar << ", blat = " << pusher->blat << std::endl;
        }
    });

    // Consumer Thread simulation
    std::thread consumer([&]() {
        int count = 0;
        while (count < 5) {
            auto popper = fifo.pop();
            if (popper) {
                // Zero-copy read straight out of the queue buffer!
                std::cout << "[Consumer] Popped: bar = " << popper->bar << ", blat = " << popper->blat << std::endl;
                ++count;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    return 0;
}