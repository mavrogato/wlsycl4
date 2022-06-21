
#include <iostream>
#include <tuple>
#include <string_view>

#include <coroutine>
#include <wayland-client.h>

#include <CL/sycl.hpp>

inline namespace aux
{
    template <class T>
    struct task {
        struct promise_type {
            std::coroutine_handle<> continuation;
            T result;
            void unhandled_exception() { throw; }
            auto get_return_object() { return task{*this}; }
            auto initial_suspend() { return std::suspend_always{}; }
            auto final_suspend() noexcept {
                struct awaiter {
                    bool await_ready() noexcept { return false; }
                    void await_resume() noexcept { }
                    auto await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
                        /*
                          awaiter::await_suspend is called when the execution of the 
                          current coroutine (referred to by 'handle') is about to finish.
                          If the current coroutine was resumed by another coroutine via
                          co_await get_task(), a handle to that coroutine has been stored
                          as handle.promise().continuation. In that case, return the handle
                          to resume the previous coroutine.
                          Otherwise, return noop_coroutine(), whose resumption does nothing.
                        */
                        if (auto continuation = handle.promise().continuation) {
                            return continuation;
                        }
                        else {
                            continuation = std::noop_coroutine();
                            return continuation;
                        }
                    }
                };
                return awaiter{};
            }
            void return_value(T value) noexcept { this->result = std::move(value); }
        };

    private:
        explicit task(promise_type& p) noexcept
            : handle{std::coroutine_handle<promise_type>::from_promise(p)}
        {
        }

    public:
        task(task const&) = delete;
        task(task&& rhs) noexcept
            : handle{std::exchange(rhs.handle, nullptr)}
        {
        }
        auto operator co_await() noexcept {
            struct awaiter {
                bool await_ready() noexcept {
                    return false;
                }
                T await_resume() noexcept {
                    return std::move(this->handle.promise().result);
                }
                auto await_suspend() noexcept {
                    this->handle.promise().continuation = handle;
                    return this->handle;
                }
                std::coroutine_handle<promise_type> handle;
            };
            return awaiter{this->handle};
        }
        T operator()() noexcept {
            this->handle.resume();
            return std::move(this->handle.promise().result);
        }
        ~task() noexcept {
            if (this->handle) this->handle.destroy();
        }

    private:
        std::coroutine_handle<promise_type> handle;
    };
}

void with_shell(wl_shell* shell) noexcept {
    std::cout << shell << std::endl;
    wl_shell_destroy(shell);
}

void with_surface(wl_surface* surface) noexcept {
    std::cerr << surface << std::endl;
    wl_surface_destroy(surface);
}

void with_compositor(wl_compositor* compositor) noexcept {
    if (auto surface = wl_compositor_create_surface(compositor)) {
        with_surface(surface);
    }
    wl_compositor_destroy(compositor);
}

void with_registry(wl_display* display, wl_registry* registry) noexcept {
    wl_registry_listener listener {
        .global = [](auto... args) noexcept {
            auto [data, registry, name, interface, version] = std::tuple{args...};
            if (std::string_view(interface) == wl_compositor_interface.name) {
                auto raw = wl_registry_bind(registry, name, &wl_compositor_interface, version);
                with_compositor(reinterpret_cast<wl_compositor*>(raw));
            }
            if (std::string_view(interface) == wl_shell_interface.name) {
                auto raw = wl_registry_bind(registry, name, &wl_shell_interface, version);
                with_shell(reinterpret_cast<wl_shell*>(raw));
            }
        },
        .global_remove = [](auto... args) noexcept {
        },
    };
    if (0 == wl_registry_add_listener(registry, &listener, nullptr)) {
        wl_display_roundtrip(display);
    }
    wl_registry_destroy(registry);
}

void with_display(wl_display* display) noexcept {
    if (auto registry = wl_display_get_registry(display)) {
        with_registry(display, registry);
    }
    wl_display_disconnect(display);
}

int main() {
    if (auto display = wl_display_connect(nullptr)) {
        with_display(display);
    }

    auto v = []() -> aux::task<int> {
        co_return 42;
    }();

    std::cout << v() << std::endl;

    return 0;
}
