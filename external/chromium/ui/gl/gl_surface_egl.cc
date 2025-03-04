// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#if defined(__LB_SHELL__)
#include "../angle/include/EGL/egl.h"
#include "../angle/include/EGL/eglext.h"
#else
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#endif
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "ui/gl/gl_bindings.h"

#if defined(USE_X11)
extern "C" {
#include <X11/Xlib.h>
}
#endif

using ui::GetLastEGLErrorString;

namespace gfx {

namespace {

EGLConfig g_config;
EGLDisplay g_display;
EGLNativeDisplayType g_native_display;
EGLConfig g_software_config;
EGLDisplay g_software_display;
EGLNativeDisplayType g_software_native_display;

const char* g_egl_extensions = NULL;
bool g_egl_create_context_robustness_supported = false;

}

GLSurfaceEGL::GLSurfaceEGL() : software_(false) {}

bool GLSurfaceEGL::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

#if defined(USE_X11)
  g_native_display = base::MessagePumpForUI::GetDefaultXDisplay();
#else
  g_native_display = EGL_DEFAULT_DISPLAY;
#endif
  g_display = eglGetDisplay(g_native_display);
  if (!g_display) {
    LOG(ERROR) << "eglGetDisplay failed with error " << GetLastEGLErrorString();
    return false;
  }

  if (!eglInitialize(g_display, NULL, NULL)) {
    LOG(ERROR) << "eglInitialize failed with error " << GetLastEGLErrorString();
    return false;
  }

  // Choose an EGL configuration.
  // On X this is only used for PBuffer surfaces.
  static const EGLint kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
    EGL_NONE
  };

  EGLint num_configs;
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return false;
  }

  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       &g_config,
                       1,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  g_egl_extensions = eglQueryString(g_display, EGL_EXTENSIONS);
  g_egl_create_context_robustness_supported =
      HasEGLExtension("EGL_EXT_create_context_robustness");

  initialized = true;

#if defined(USE_X11) || defined(OS_ANDROID) || defined(__LB_ANDROID__)
  return true;
#else
  g_software_native_display = EGL_SOFTWARE_DISPLAY_ANGLE;
#endif
  g_software_display = eglGetDisplay(g_software_native_display);
  if (!g_software_display) {
    return true;
  }

  if (!eglInitialize(g_software_display, NULL, NULL)) {
    return true;
  }

  if (!eglChooseConfig(g_software_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    g_software_display = NULL;
    return true;
  }

  if (num_configs == 0) {
    g_software_display = NULL;
    return true;
  }

  if (!eglChooseConfig(g_software_display,
                       kConfigAttribs,
                       &g_software_config,
                       1,
                       &num_configs)) {
    g_software_display = NULL;
    return false;
  }

  return true;
}

EGLDisplay GLSurfaceEGL::GetDisplay() {
  return software_ ? g_software_display : g_display;
}

EGLDisplay GLSurfaceEGL::GetHardwareDisplay() {
  return g_display;
}

EGLDisplay GLSurfaceEGL::GetSoftwareDisplay() {
  return g_software_display;
}

EGLNativeDisplayType GLSurfaceEGL::GetNativeDisplay() {
  return g_native_display;
}

const char* GLSurfaceEGL::GetEGLExtensions() {
  return g_egl_extensions;
}

bool GLSurfaceEGL::HasEGLExtension(const char* name) {
  return ExtensionsContain(GetEGLExtensions(), name);
}

bool GLSurfaceEGL::IsCreateContextRobustnessSupported() {
  return g_egl_create_context_robustness_supported;
}

GLSurfaceEGL::~GLSurfaceEGL() {}

NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(bool software,
                                               gfx::AcceleratedWidget window)
    : window_(window),
      surface_(NULL),
      supports_post_sub_buffer_(false),
      config_(NULL) {
  software_ = software;
}

bool NativeViewGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  if (!GetDisplay()) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  static const EGLint egl_window_attributes_sub_buffer[] = {
    EGL_POST_SUB_BUFFER_SUPPORTED_NV, EGL_TRUE,
    EGL_NONE
  };

  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(
      GetDisplay(),
      GetConfig(),
      window_,
      gfx::g_driver_egl.ext.b_EGL_NV_post_sub_buffer ?
          egl_window_attributes_sub_buffer :
          NULL);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  EGLint surfaceVal;
  EGLBoolean retVal = eglQuerySurface(GetDisplay(),
                                      surface_,
                                      EGL_POST_SUB_BUFFER_SUPPORTED_NV,
                                      &surfaceVal);
  supports_post_sub_buffer_ = (surfaceVal && retVal) == EGL_TRUE;

  return true;
}

void NativeViewGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

EGLConfig NativeViewGLSurfaceEGL::GetConfig() {
#if !defined(USE_X11)
  return software_ ? g_software_config : g_config;
#else
  if (!config_) {
    // Get a config compatible with the window
    DCHECK(window_);
    XWindowAttributes win_attribs;
    if (!XGetWindowAttributes(GetNativeDisplay(), window_, &win_attribs)) {
      return NULL;
    }

    // Try matching the window depth with an alpha channel,
    // because we're worried the destination alpha width could
    // constrain blending precision.
    const int kBufferSizeOffset = 1;
    const int kAlphaSizeOffset = 3;
    EGLint config_attribs[] = {
      EGL_BUFFER_SIZE, ~0,
      EGL_ALPHA_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
      EGL_NONE
    };
    config_attribs[kBufferSizeOffset] = win_attribs.depth;

    EGLint num_configs;
    if (!eglChooseConfig(g_display,
                         config_attribs,
                         &config_,
                         1,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return NULL;
    }

    if (num_configs) {
      EGLint config_depth;
      if (!eglGetConfigAttrib(g_display,
                              config_,
                              EGL_BUFFER_SIZE,
                              &config_depth)) {
        LOG(ERROR) << "eglGetConfigAttrib failed with error "
                   << GetLastEGLErrorString();
        return NULL;
      }

      if (config_depth == win_attribs.depth) {
        return config_;
      }
    }

    // Try without an alpha channel.
    config_attribs[kAlphaSizeOffset] = 0;
    if (!eglChooseConfig(g_display,
                         config_attribs,
                         &config_,
                         1,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return NULL;
    }

    if (num_configs == 0) {
      LOG(ERROR) << "No suitable EGL configs found.";
      return NULL;
    }
  }
  return config_;
#endif
}

bool NativeViewGLSurfaceEGL::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceEGL::SwapBuffers() {
  if (!eglSwapBuffers(GetDisplay(), surface_)) {
    DVLOG(1) << "eglSwapBuffers failed with error "
             << GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::Size NativeViewGLSurfaceEGL::GetSize() {
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(GetDisplay(), surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(GetDisplay(), surface_, EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
}

bool NativeViewGLSurfaceEGL::Resize(const gfx::Size& size) {
  if (size == GetSize())
    return true;

  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current)
    current_context->ReleaseCurrent(this);

  Destroy();

  if (!Initialize()) {
    LOG(ERROR) << "Failed to resize pbuffer.";
    return false;
  }

  if (was_current)
    return current_context->MakeCurrent(this);
  return true;
}

EGLSurface NativeViewGLSurfaceEGL::GetHandle() {
  return surface_;
}

std::string NativeViewGLSurfaceEGL::GetExtensions() {
  std::string extensions = GLSurface::GetExtensions();
  if (supports_post_sub_buffer_) {
    extensions += extensions.empty() ? "" : " ";
    extensions += "GL_CHROMIUM_post_sub_buffer";
  }
  return extensions;
}

bool NativeViewGLSurfaceEGL::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(supports_post_sub_buffer_);
  if (!eglPostSubBufferNV(GetDisplay(), surface_, x, y, width, height)) {
    DVLOG(1) << "eglPostSubBufferNV failed with error "
             << GetLastEGLErrorString();
    return false;
  }
  return true;
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  Destroy();
}

void NativeViewGLSurfaceEGL::SetHandle(EGLSurface surface) {
  surface_ = surface;
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(bool software, const gfx::Size& size)
    : size_(size),
      surface_(NULL) {
  software_ = software;
}

bool PbufferGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  if (!GetDisplay()) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  if (size_.GetArea() == 0) {
    LOG(ERROR) << "Error: surface has zero area "
               << size_.width() << " x " << size_.height();
    return false;
  }

  const EGLint pbuffer_attribs[] = {
    EGL_WIDTH, size_.width(),
    EGL_HEIGHT, size_.height(),
    EGL_NONE
  };

  surface_ = eglCreatePbufferSurface(GetDisplay(),
                                     GetConfig(),
                                     pbuffer_attribs);
  if (!surface_) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

EGLConfig PbufferGLSurfaceEGL::GetConfig() {
  return software_ ? g_software_config : g_config;
}

bool PbufferGLSurfaceEGL::IsOffscreen() {
  return true;
}

bool PbufferGLSurfaceEGL::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a PbufferGLSurfaceEGL.";
  return false;
}

gfx::Size PbufferGLSurfaceEGL::GetSize() {
  return size_;
}

bool PbufferGLSurfaceEGL::Resize(const gfx::Size& size) {
  if (size == size_)
    return true;

  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current)
    current_context->ReleaseCurrent(this);

  Destroy();

  size_ = size;

  if (!Initialize()) {
    LOG(ERROR) << "Failed to resize pbuffer.";
    return false;
  }

  if (was_current)
    return current_context->MakeCurrent(this);

  return true;
}

EGLSurface PbufferGLSurfaceEGL::GetHandle() {
  return surface_;
}

void* PbufferGLSurfaceEGL::GetShareHandle() {
#if defined(OS_ANDROID) || defined(__LB_ANDROID__)
  NOTREACHED();
  return NULL;
#else
  if (!gfx::g_driver_egl.ext.b_EGL_ANGLE_query_surface_pointer)
    return NULL;

  if (!gfx::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle)
    return NULL;

  void* handle;
  if (!eglQuerySurfacePointerANGLE(g_display,
                                   GetHandle(),
                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                   &handle)) {
    return NULL;
  }

  return handle;
#endif
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  Destroy();
}

}  // namespace gfx
