
#include <iostream>
#include <stdexcept>
#include <memory>
#include <tuple>

#include <cstring>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GL/gl.h>

#include "weston-desktop-shell-client.h"

template <typename T, typename D>
inline auto ptr(T* p, D d) {
  return std::unique_ptr<T, D>(p, d);
}

template <typename T>
inline auto ptr(T* p) {
  return ptr(p, [](T* p) { wl_proxy_destroy(reinterpret_cast<wl_proxy*>(p)); });
}

template <typename T> constexpr wl_interface const* const iface = nullptr;
template <> constexpr wl_interface const* const iface<wl_compositor>        = &wl_compositor_interface;
template <> constexpr wl_interface const* const iface<wl_shell>             = &wl_shell_interface;
template <> constexpr wl_interface const* const iface<wl_seat>              = &wl_seat_interface;
template <> constexpr wl_interface const* const iface<wl_output>            = &wl_output_interface;
template <> constexpr wl_interface const* const iface<weston_desktop_shell> = &weston_desktop_shell_interface;

template <typename T>
inline auto global_bind(wl_display* display, uint32_t version) {
  static_assert(nullptr != iface<T>, "iface not defined");

  using userdata_type = std::tuple<T*, uint32_t>;
  auto registry = ptr(wl_display_get_registry(display));
  wl_registry_listener listener = {
    .global = [](void* data,
		 wl_registry* registry,
		 uint32_t name,
		 char const* interface,
		 uint32_t version)
    {
      if (0 == std::strcmp(interface, iface<T>->name)) {
	auto& userdata = *(reinterpret_cast<userdata_type*>(data));

	auto& ptr = std::get<0>(userdata);
	ptr = reinterpret_cast<T*>(wl_registry_bind(registry,
						    name,
						    iface<T>,
						    std::min(version,
							     std::get<1>(userdata))));
      }
    },
    .global_remove = [](void* data,
			wl_registry* registry,
			uint32_t name)
    {
    },
  };

  userdata_type userdata(nullptr, version);

  wl_registry_add_listener(registry.get(), &listener, &userdata);
  wl_display_roundtrip(display);

  return ptr(std::get<0>(userdata));
}

int main() {
  int result = -1;
  std::cerr << "****************************************" << std::endl;
  try {
    auto display = ptr(wl_display_connect(nullptr), wl_display_disconnect);

    auto compositor    = global_bind<wl_compositor>       (display.get(), 4);
    //    auto shell         = global_bind<wl_shell>            (display.get(), 1);
    auto seat          = global_bind<wl_seat>             (display.get(), 1);
    auto output        = global_bind<wl_output>           (display.get(), 1);
    auto desktop_shell = global_bind<weston_desktop_shell>(display.get(), 1);

    auto surface       = ptr(wl_compositor_create_surface(compositor.get()));
    //    auto shell_surface = ptr(wl_shell_get_shell_surface(shell.get(), surface.get()));

    wl_seat_listener seat_listener = {
      .capabilities = [](void*, wl_seat*, uint32_t capability) {
	std::cerr << "seat capability: " << capability << std::endl;
      },
      .name = [](void*, wl_seat*, char const* name) {
	std::cerr << "seat name: " << name << std::endl;
      },
    };
    wl_seat_add_listener(seat.get(), &seat_listener, nullptr);
    wl_display_roundtrip(display.get());

    wl_output_listener output_listener = {
      .geometry = [](void* data, wl_output* output,
		     int x, int y, int physical_width, int physical_height,
		     int subpixel, char const* make, char const* model, int transform)
      {
	std::cerr << "geometry: ";
	std::cerr << x << ':';
	std::cerr << y << ':';
	std::cerr << physical_width << ':';
	std::cerr << physical_height << ':';
	std::cerr << subpixel << ':';
	std::cerr << make << ':';
	std::cerr << model << ':';
	std::cerr << transform << std::endl;
      },
      .mode = [](void* data, wl_output* output,
		 uint32_t flags, int width, int height, int refresh)
      {
	std::cerr << "mode: ";
	std::cerr << flags << ':';
	std::cerr << width << ':';
	std::cerr << height << ':';
	std::cerr << refresh << ':';
      },
      .done = [](void* data, wl_output* output) {
	std::cerr << "done." << std::endl;
      },
      .scale = [](void* data, wl_output* output, int factor) {
	std::cerr << "scale: ";
	std::cerr << factor << std::endl;
      },
    };
    std::cerr << output.get() << std::endl;
    wl_output_add_listener(output.get(), &output_listener, nullptr);
    wl_display_roundtrip(display.get());

    auto egl_display = ptr(eglGetDisplay(display.get()), eglTerminate);

    eglInitialize(egl_display.get(), nullptr, nullptr);

    eglBindAPI(EGL_OPENGL_API);
    EGLint attributes[] = {
      EGL_RED_SIZE,   8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE,  8,
      EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint num_config = 0;
    eglChooseConfig(egl_display.get(), attributes, &config, 1, &num_config);
    auto egl_context = ptr(eglCreateContext(egl_display.get(), config, EGL_NO_CONTEXT, nullptr),
			   [&egl_display](auto p) { eglDestroyContext(egl_display.get(), p); });
    auto egl_window = ptr(wl_egl_window_create(surface.get(), 1920, 1080), wl_egl_window_destroy);
    auto egl_surface = ptr(eglCreateWindowSurface(egl_display.get(),
						  config, egl_window.get(), nullptr),
			   [&egl_display](auto p) { eglDestroySurface(egl_display.get(), p); });
    eglMakeCurrent(egl_display.get(), egl_surface.get(), egl_surface.get(), egl_context.get());

    #if 0
    wl_shell_surface_listener shell_surface_listener = {
      .ping = [](void* data, wl_shell_surface* shell_surface, uint32_t serial) {
	wl_shell_surface_pong(shell_surface, serial);
      },
      .configure = [](void* data, wl_shell_surface* shell_surface,
		      uint32_t edges, int32_t width, int32_t height) {
	wl_egl_window_resize(reinterpret_cast<wl_egl_window*>(data),
			     width, height, 0, 0);
	// glViewport(0, 0, width, height);
      },
      .popup_done = [](void* data, wl_shell_surface* surface) {
      },
    };
    wl_shell_surface_add_listener(shell_surface.get(), &shell_surface_listener, egl_window.get());
    wl_shell_surface_set_toplevel(shell_surface.get());
    #endif

    weston_desktop_shell_set_grab_surface(desktop_shell.get(), surface.get());
    weston_desktop_shell_set_background(desktop_shell.get(), output.get(), surface.get());
    weston_desktop_shell_desktop_ready(desktop_shell.get());

    std::system("weston-terminal &");

    do {
      glClearColor(0.0, 0.0, 1.0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
      eglSwapBuffers(egl_display.get(), egl_surface.get());
    } while (-1 != wl_display_dispatch(display.get()));

    result = 0;
  }
  catch (std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  std::cerr << "****************************************" << std::endl;
  return result;
}
