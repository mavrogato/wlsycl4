
#include <iostream>
#include <tuple>
#include <string_view>

// >>> TODO
#define __cpp_impl_coroutine 1
# include <coroutine>
#undef  __cpp_impl_coroutine
namespace std::inline experimental
{
    using std::coroutine_traits;
    using std::coroutine_handle;
}
// <<< TODO

#include <wayland-client.h>

#include <CL/sycl.hpp>

namespace
{
    template <class T>
    struct filament {
        struct promise_type {
            void unhandled_exception() { throw; }
            auto get_return_object() noexcept { return filament{*this}; }
            auto initial_suspend() noexcept { return std::suspend_always{}; }
            auto final_suspend() noexcept { return std::suspend_always{}; }
            auto yield_value(T value) noexcept {
                this->result = value;
                return std::suspend_always{};
            }
            auto return_void() noexcept { }
            T result;
        };
        filament() = delete;
        filament(filament const&) = delete;
        filament(filament&& src)
            : handle{std::exchange(src.handle, nullptr)}
        {
        }
        filament& operator=(filament const&) = delete;
        filament& operator=(filament&& src) {
            if (this != &src) {
                this->handle = std::exchange(src.handle, nullptr);
            }
            return *this;
        }
        ~filament() noexcept { if (this->handle) this->handle.destroy(); }
        T step() const noexcept {
            this->handle.resume();
            return this->handle.promise().result;
        }

    private:
        explicit filament(promise_type& p) noexcept
            : handle{std::coroutine_handle<promise_type>::from_promise(p)}
        {
        }
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
    return 0;
}
