
/*!
 * @file opengl.cpp
 * Lower-level OpenGL interface. No actual rendering is performed here!
 */

#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>

#include "game/graphics/pipelines/glfw_display.h"
#include "opengl.h"

#include "common/dma/dma_copy.h"
#include "common/global_profiler/GlobalProfiler.h"
#include "common/goal_constants.h"
#include "common/log/log.h"
#include "common/util/FileUtil.h"
#include "common/util/FrameLimiter.h"
#include "common/util/Timer.h"
#include "common/util/compress.h"

#include "game/graphics/display.h"
#include "game/graphics/gfx.h"
#include "game/graphics/opengl_renderer/OpenGLRenderer.h"
#include "game/graphics/opengl_renderer/debug_gui.h"
#include "game/graphics/texture/TexturePool.h"
#include "game/runtime.h"
#include "game/sce/libscf.h"
#include "game/system/newpad.h"

#include "third-party/fmt/core.h"
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_impl_glfw.h"
#include "third-party/imgui/imgui_impl_opengl3.h"
#define STBI_WINDOWS_UTF8
#include "third-party/stb_image/stb_image.h"

static bool want_hotkey_screenshot = false;

void GLFWDisplay::SetGlobalGLFWCallbacks() {
  if (g_glfw_state.callbacks_registered) {
    lg::warn("Global GLFW callbacks were already registered!");
  }

  // Get initial state
  g_glfw_state.monitors = glfwGetMonitors(&g_glfw_state.monitor_count);

  // Listen for events
  glfwSetMonitorCallback([](GLFWmonitor* /*monitor*/, int /*event*/) {
    // Reload monitor list
    g_glfw_state.monitors = glfwGetMonitors(&g_glfw_state.monitor_count);
  });

  g_glfw_state.callbacks_registered = true;
}

void GLFWDisplay::ClearGlobalGLFWCallbacks() {
  if (!g_glfw_state.callbacks_registered) {
    return;
  }

  glfwSetMonitorCallback(NULL);

  g_glfw_state.callbacks_registered = false;
}

GLFWDisplay::GLFWDisplay(GLFWwindow* window, bool is_main) : m_window(window) {
  m_main = is_main;

  // Get initial state
  get_position(&m_last_windowed_xpos, &m_last_windowed_ypos);
  get_size(&m_last_windowed_width, &m_last_windowed_height);

  // Listen for window-specific GLFW events
  glfwSetWindowUserPointer(window, reinterpret_cast<void*>(this));

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_key(window, key, scancode, action, mods);
  });

  glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mode) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_mouse_key(window, button, action, mode);
  });

  glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xposition, double yposition) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_cursor_position(window, xposition, yposition);
  });

  glfwSetWindowPosCallback(window, [](GLFWwindow* window, int xpos, int ypos) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_window_pos(window, xpos, ypos);
  });

  glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_window_size(window, width, height);
  });

  glfwSetWindowIconifyCallback(window, [](GLFWwindow* window, int iconified) {
    GLDisplay* display = reinterpret_cast<GLDisplay*>(glfwGetWindowUserPointer(window));
    display->on_iconify(window, iconified);
  });
}

GLFWDisplay::~GLFWDisplay() {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  glfwSetKeyCallback(m_window, NULL);
  glfwSetWindowPosCallback(m_window, NULL);
  glfwSetWindowSizeCallback(m_window, NULL);
  glfwSetWindowIconifyCallback(m_window, NULL);
  glfwSetWindowUserPointer(m_window, nullptr);
  glfwDestroyWindow(m_window);
  if (m_main) {
    // gl_exit();
  }
}

void GLFWDisplay::update_cursor_visibility(GLFWwindow* window, bool is_visible) {
  if (Gfx::get_button_mapping().use_mouse) {
    auto cursor_mode = is_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
    glfwSetInputMode(window, GLFW_CURSOR, cursor_mode);
  }
}

void GLFWDisplay::on_key(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
  if (action == GlfwKeyAction::Press) {
    // lg::debug("KEY PRESS:   key: {} scancode: {} mods: {:X}", key, scancode, mods);
    Pad::OnKeyPress(key);
  } else if (action == GlfwKeyAction::Release) {
    // lg::debug("KEY RELEASE: key: {} scancode: {} mods: {:X}", key, scancode, mods);
    Pad::OnKeyRelease(key);
    // Debug keys input mapping TODO add remapping
    switch (key) {
      case GLFW_KEY_LEFT_ALT:
      case GLFW_KEY_RIGHT_ALT:
        if (glfwGetWindowAttrib(window, GLFW_FOCUSED) &&
            !Gfx::g_debug_settings.ignore_imgui_hide_keybind) {
          set_imgui_visible(!is_imgui_visible());
          update_cursor_visibility(window, is_imgui_visible());
        }
        break;
      case GLFW_KEY_F2:
        want_hotkey_screenshot = true;
        break;
    }
  }
}

void GLFWDisplay::on_mouse_key(GLFWwindow* /*window*/, int button, int action, int /*mode*/) {
  int key =
      button + GLFW_KEY_LAST;  // Mouse button index are appended after initial GLFW keys in newpad

  if (button == GLFW_MOUSE_BUTTON_LEFT &&
      is_imgui_visible()) {  // Are there any other mouse buttons we don't want to use?
    Pad::ClearKey(key);
    return;
  }

  if (action == GlfwKeyAction::Press) {
    Pad::OnKeyPress(key);
  } else if (action == GlfwKeyAction::Release) {
    Pad::OnKeyRelease(key);
  }
}

void GLFWDisplay::on_cursor_position(GLFWwindow* /*window*/, double xposition, double yposition) {
  double xoffset = xposition - last_cursor_x_position;
  double yoffset = yposition - last_cursor_y_position;

  last_cursor_x_position = xposition;
  last_cursor_y_position = yposition;
  Pad::MappingInfo mapping_info = Gfx::get_button_mapping();
  if (is_imgui_visible() || !mapping_info.use_mouse) {
    if (is_cursor_position_valid == true) {
      Pad::ClearAnalogAxisValue(mapping_info, GlfwKeyCustomAxis::CURSOR_X_AXIS);
      Pad::ClearAnalogAxisValue(mapping_info, GlfwKeyCustomAxis::CURSOR_Y_AXIS);
      is_cursor_position_valid = false;
    }
    return;
  }

  if (is_cursor_position_valid == false) {
    is_cursor_position_valid = true;
    return;
  }

  Pad::SetAnalogAxisValue(mapping_info, GlfwKeyCustomAxis::CURSOR_X_AXIS, xoffset);
  Pad::SetAnalogAxisValue(mapping_info, GlfwKeyCustomAxis::CURSOR_Y_AXIS, yoffset);
}

void GLFWDisplay::on_window_pos(GLFWwindow* /*window*/, int xpos, int ypos) {
  // only change them on a legit change, not on the initial update
  if (m_fullscreen_mode != GfxDisplayMode::ForceUpdate &&
      m_fullscreen_target_mode == GfxDisplayMode::Windowed) {
    m_last_windowed_xpos = xpos;
    m_last_windowed_ypos = ypos;
  }
}

void GLFWDisplay::on_window_size(GLFWwindow* /*window*/, int width, int height) {
  // only change them on a legit change, not on the initial update
  if (m_fullscreen_mode != GfxDisplayMode::ForceUpdate &&
      m_fullscreen_target_mode == GfxDisplayMode::Windowed) {
    m_last_windowed_width = width;
    m_last_windowed_height = height;
  }
}

void GLFWDisplay::on_iconify(GLFWwindow* /*window*/, int iconified) {
  m_minimized = iconified == GLFW_TRUE;
}

void GLFWDisplay::get_position(int* x, int* y) {
  std::lock_guard<std::mutex> lk(m_lock);
  if (x) {
    *x = m_display_state.window_pos_x;
  }
  if (y) {
    *y = m_display_state.window_pos_y;
  }
}

void GLFWDisplay::get_size(int* width, int* height) {
  std::lock_guard<std::mutex> lk(m_lock);
  if (width) {
    *width = m_display_state.window_size_width;
  }
  if (height) {
    *height = m_display_state.window_size_height;
  }
}

void GLFWDisplay::get_scale(float* xs, float* ys) {
  std::lock_guard<std::mutex> lk(m_lock);
  if (xs) {
    *xs = m_display_state.window_scale_x;
  }
  if (ys) {
    *ys = m_display_state.window_scale_y;
  }
}

void GLFWDisplay::set_size(int width, int height) {
  // glfwSetWindowSize(m_window, width, height);
  m_pending_size.width = width;
  m_pending_size.height = height;
  m_pending_size.pending = true;

  if (windowed()) {
    m_last_windowed_width = width;
    m_last_windowed_height = height;
  }
}

void GLFWDisplay::update_fullscreen(GfxDisplayMode mode, int screen) {
  GLFWmonitor* monitor = get_monitor(screen);

  switch (mode) {
    case GfxDisplayMode::Windowed: {
      // windowed
      // TODO - display mode doesn't re-position the window
      int x, y, width, height;

      if (m_last_fullscreen_mode == GfxDisplayMode::Windowed) {
        // windowed -> windowed, keep position and size
        width = m_last_windowed_width;
        height = m_last_windowed_height;
        x = m_last_windowed_xpos;
        y = m_last_windowed_ypos;
        lg::debug("Windowed -> Windowed - x:{} | y:{}", x, y);
      } else {
        // fullscreen -> windowed, use last windowed size but on the monitor previously fullscreened
        //
        // glfwGetMonitorWorkarea will return the width/height of the scaled fullscreen window
        // - for example, you full screened a 1280x720 game on a 4K monitor -- you won't get the 4k
        // resolution!
        //
        // Additionally, the coordinates for the top left seem very weird in stacked displays (you
        // get a negative Y coordinate)
        int monitorX, monitorY, monitorWidth, monitorHeight;
        glfwGetMonitorWorkarea(monitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);

        width = m_last_windowed_width;
        height = m_last_windowed_height;
        if (monitorX < 0) {
          x = monitorX - 50;
        } else {
          x = monitorX + 50;
        }
        if (monitorY < 0) {
          y = monitorY - 50;
        } else {
          y = monitorY + 50;
        }
        lg::debug("FS -> Windowed screen: {} - x:{}:{}/{} | y:{}:{}/{}", screen, monitorX, x, width,
                  monitorY, y, height);
      }

      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
      glfwSetWindowFocusCallback(m_window, NULL);
      glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_FALSE);
      glfwSetWindowMonitor(m_window, NULL, x, y, width, height, GLFW_DONT_CARE);
    } break;
    case GfxDisplayMode::Fullscreen: {
      // TODO - when transitioning from fullscreen to windowed, it will use the old primary display
      // which is to say, dragging the window to a different monitor won't update the used display
      // fullscreen
      const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
      glfwSetWindowFocusCallback(m_window, NULL);
      glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_FALSE);
      glfwSetWindowMonitor(m_window, monitor, 0, 0, vmode->width, vmode->height, GLFW_DONT_CARE);
    } break;
    case GfxDisplayMode::Borderless: {
      // TODO - when transitioning from fullscreen to windowed, it will use the old primary display
      // which is to say, dragging the window to a different monitor won't update the used display
      // borderless fullscreen
      int x, y;
      glfwGetMonitorPos(monitor, &x, &y);
      const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
      glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
      // glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_TRUE);
      // glfwSetWindowFocusCallback(m_window, FocusCallback);
#ifdef _WIN32
      glfwSetWindowMonitor(m_window, NULL, x, y, vmode->width, vmode->height + 1, GLFW_DONT_CARE);
#else
      glfwSetWindowMonitor(m_window, NULL, x, y, vmode->width, vmode->height, GLFW_DONT_CARE);
#endif
    } break;
    default: {
      break;
    }
  }
}

int GLFWDisplay::get_screen_vmode_count() {
  std::lock_guard<std::mutex> lk(m_lock);
  return m_display_state.num_vmodes;
}

void GLFWDisplay::get_screen_size(int vmode_idx, s32* w_out, s32* h_out) {
  std::lock_guard<std::mutex> lk(m_lock);
  if (vmode_idx >= 0 && vmode_idx < MAX_VMODES) {
    if (w_out) {
      *w_out = m_display_state.vmodes[vmode_idx].width;
    }
    if (h_out) {
      *h_out = m_display_state.vmodes[vmode_idx].height;
    }
  } else if (fullscreen_mode() == Fullscreen) {
    if (w_out) {
      *w_out = m_display_state.largest_vmode_width;
    }
    if (h_out) {
      *h_out = m_display_state.largest_vmode_height;
    }
  } else {
    if (w_out) {
      *w_out = m_display_state.current_vmode.width;
    }
    if (h_out) {
      *h_out = m_display_state.current_vmode.height;
    }
  }
}

int GLFWDisplay::get_screen_rate(int vmode_idx) {
  std::lock_guard<std::mutex> lk(m_lock);
  if (vmode_idx >= 0 && vmode_idx < MAX_VMODES) {
    return m_display_state.vmodes[vmode_idx].refresh_rate;
  } else if (fullscreen_mode() == GfxDisplayMode::Fullscreen) {
    return m_display_state.largest_vmode_refresh_rate;
  } else {
    return m_display_state.current_vmode.refresh_rate;
  }
}

GLFWmonitor* GLFWDisplay::get_monitor(int index) {
  if (index < 0 || index >= g_glfw_state.monitor_count) {
    // out of bounds, default to primary monitor
    index = 0;
  }

  return g_glfw_state.monitors[index];
}

int GLFWDisplay::get_monitor_count() {
  return g_glfw_state.monitor_count;
}

std::tuple<double, double> GLFWDisplay::get_mouse_pos() {
  return {last_cursor_x_position, last_cursor_y_position};
}

bool GLFWDisplay::minimized() {
  return m_minimized;
}

void GLFWDisplay::set_lock(bool lock) {
  glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, lock ? GLFW_TRUE : GLFW_FALSE);
}

bool GLFWDisplay::fullscreen_pending() {
  GLFWmonitor* monitor;
  {
    auto _ = scoped_prof("get_monitor");
    monitor = get_monitor(fullscreen_screen());
  }

  const GLFWvidmode* vmode;
  {
    auto _ = scoped_prof("get-video-mode");
    vmode = glfwGetVideoMode(monitor);
  }

  return GfxDisplay::fullscreen_pending() ||
         (vmode->width != m_last_video_mode.width || vmode->height != m_last_video_mode.height ||
          vmode->refreshRate != m_last_video_mode.refreshRate);
}

void GLFWDisplay::fullscreen_flush() {
  GfxDisplay::fullscreen_flush();

  GLFWmonitor* monitor = get_monitor(fullscreen_screen());
  auto vmode = glfwGetVideoMode(monitor);

  m_last_video_mode = *vmode;
}

void GLFWDisplay::VMode::set(const GLFWvidmode* vmode) {
  width = vmode->width;
  height = vmode->height;
  refresh_rate = vmode->refreshRate;
}

void GLFWDisplay::update_glfw() {
  auto p = scoped_prof("update_glfw");

  glfwPollEvents();
  //TODO relocate
  if (glfwGetCurrentContext()) {
    glfwMakeContextCurrent(this->m_window);
  }
  auto& mapping_info = Gfx::get_button_mapping();
  Pad::update_gamepads(mapping_info);

  glfwGetFramebufferSize(this->m_window, &m_display_state_copy.window_size_width,
                         &m_display_state_copy.window_size_height);

  glfwGetWindowContentScale(this->m_window, &m_display_state_copy.window_scale_x,
                            &m_display_state_copy.window_scale_y);

  glfwGetWindowPos(this->m_window, &m_display_state_copy.window_pos_x,
                   &m_display_state_copy.window_pos_y);

  GLFWmonitor* monitor = this->get_monitor(fullscreen_screen());
  auto current_vmode = glfwGetVideoMode(monitor);
  if (current_vmode) {
    m_display_state_copy.current_vmode.set(current_vmode);
  }

  int count = 0;
  auto vmodes = glfwGetVideoModes(monitor, &count);

  if (count > MAX_VMODES) {
    fmt::print("got too many vmodes: {}\n", count);
    count = MAX_VMODES;
  }

  m_display_state_copy.num_vmodes = count;

  m_display_state_copy.largest_vmode_width = 1;
  m_display_state_copy.largest_vmode_refresh_rate = 1;
  for (int i = 0; i < count; i++) {
    if (vmodes[i].width > m_display_state_copy.largest_vmode_width) {
      m_display_state_copy.largest_vmode_height = vmodes[i].height;
      m_display_state_copy.largest_vmode_width = vmodes[i].width;
    }

    if (vmodes[i].refreshRate > m_display_state_copy.largest_vmode_refresh_rate) {
      m_display_state_copy.largest_vmode_refresh_rate = vmodes[i].refreshRate;
    }
    m_display_state_copy.vmodes[i].set(&vmodes[i]);
  }

  if (m_pending_size.pending) {
    glfwSetWindowSize(m_window, m_pending_size.width, m_pending_size.height);
    m_pending_size.pending = false;
  }

  std::lock_guard<std::mutex> lk(m_lock);
  m_display_state = m_display_state_copy;
}
