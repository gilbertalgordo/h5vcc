// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_ACCELERATED_SURFACE_WIN_H_
#define UI_SURFACE_ACCELERATED_SURFACE_WIN_H_

#include <d3d9.h>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/surface/surface_export.h"

class PresentThread;

namespace gfx {
class Rect;
}

class SURFACE_EXPORT AcceleratedPresenter
    : public base::RefCountedThreadSafe<AcceleratedPresenter> {
 public:
  typedef base::Callback<void(bool,
                              base::TimeTicks,
                              base::TimeDelta)> CompletionTask;

  explicit AcceleratedPresenter(gfx::PluginWindowHandle window);

  // Returns a thread safe reference to the presenter for the given window or
  // null is no such presenter exists. The thread safe refptr ensures the
  // presenter will not be destroyed. This can be called on any thread.
  static scoped_refptr<AcceleratedPresenter> GetForWindow(
      gfx::PluginWindowHandle window);

  // Schedule a frame to be presented. The completion callback will be invoked
  // when it is safe to write to the surface on another thread. The lock for
  // this surface will be held while the completion callback runs. This can be
  // called on any thread.
  void AsyncPresentAndAcknowledge(
      const gfx::Size& size,
      int64 surface_handle,
      const CompletionTask& completion_task);

  // Schedule the presenter to free all its resources. This can be called on any
  // thread.
  void Suspend();

  // Indicates that the presenter has become invisible.
  void WasHidden();

  // Schedule the presenter to release its reference to the shared surface.
  void ReleaseSurface();

  // The public member functions are called on the main thread.
  void Present(HDC dc);
  void AsyncCopyTo(const gfx::Rect& src_subrect,
                   const gfx::Size& dst_size,
                   void* buf,
                   const base::Callback<void(bool)>& callback);
  void Invalidate();

#if defined(USE_AURA)
  // TODO(scottmg): This is a temporary hack until we have a two-worlds ash/aura
  // separation.
  void SetNewTargetWindow(gfx::PluginWindowHandle window);
#endif

 private:
  friend class base::RefCountedThreadSafe<AcceleratedPresenter>;

  ~AcceleratedPresenter();

  // These member functions are called on the PresentThread with which the
  // presenter has affinity.
  void DoPresentAndAcknowledge(
      const gfx::Size& size,
      int64 surface_handle,
      const CompletionTask& completion_task);
  void DoSuspend();
  void DoPresent(const base::Closure& composite_task);
  void DoReleaseSurface();
  void DoCopyToAndAcknowledge(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      void* buf,
      scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
      const base::Callback<void(bool)>& callback);
  bool DoCopyTo(const gfx::Rect& src_subrect,
                const gfx::Size& dst_size,
                void* buf);

  void PresentWithGDI(HDC dc);
  gfx::Size GetWindowSize();

  // This function tries to guess whether Direct3D will be able to reliably
  // present to the window. When the window is resizing, presenting with
  // Direct3D causes other regions of the window rendered with GDI to
  // flicker transparent / non-transparent.
  bool CheckDirect3DWillWork();

  // The thread with which this presenter has affinity.
  PresentThread* const present_thread_;

  // The window that is presented to.
  gfx::PluginWindowHandle window_;

  // The lock is taken while any thread is calling the object, except those that
  // simply post from the main thread to the present thread via the immutable
  // present_thread_ member.
  base::Lock lock_;

  // UI thread can wait on this event to ensure a present is finished.
  base::WaitableEvent event_;

  // The current size of the swap chain. This is only accessed on the thread
  // with which the surface has affinity. The swap chain size is rounded up and
  // is not necessarily the same as the window size.
  gfx::Size quantized_size_;

  // The size of the window on the last present. This is used to trigger the
  // compositor instead of presenting the last frame in the case where the
  // window has been resized.
  gfx::Size present_size_;

  // This is a shared texture that is being presented from.
  base::win::ScopedComPtr<IDirect3DTexture9> source_texture_;

  // The swap chain is presented to the child window. Copy semantics
  // are used so it is possible to represent it to quickly validate the window.
  base::win::ScopedComPtr<IDirect3DSwapChain9> swap_chain_;

  // Whether the window is hidden or has not been presented to since it was
  // last hidden.
  bool hidden_;

  // These are used to detect when the window is resizing. For some reason,
  // presenting with D3D while the window resizes causes those parts not
  // drawn with D3D (e.g. with GDI) to flicker visible / invisible.
  // http://crbug.com/120904
  gfx::Size last_window_size_;
  base::Time last_window_resize_time_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedPresenter);
};

class SURFACE_EXPORT AcceleratedSurface {
 public:
  AcceleratedSurface(gfx::PluginWindowHandle window);
  ~AcceleratedSurface();

  // Synchronously present a frame with no acknowledgement.
  void Present(HDC dc);

  // Copies the surface data to |buf|. The copied region is specified with
  // |src_subrect| and the image data is transformed so that it fits in
  // |dst_size|.
  // Caller must ensure that |buf| is allocated with the size no less than
  // |4 * dst_size.width() * dst_size.height()| bytes.
  void AsyncCopyTo(const gfx::Rect& src_subrect,
                   const gfx::Size& dst_size,
                   void* buf,
                   const base::Callback<void(bool)>& callback);

  // Temporarily release resources until a new surface is asynchronously
  // presented. Present will not be able to represent the last surface after
  // calling this and will return false.
  void Suspend();

  // Indicates that the surface has become invisible.
  void WasHidden();

 private:
  const scoped_refptr<AcceleratedPresenter> presenter_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurface);
};

#endif  // UI_SURFACE_ACCELERATED_SURFACE_WIN_H_
