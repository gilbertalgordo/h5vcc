// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
#define WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGamepads.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/weburl_loader_mock_factory.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_dom_storage_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

class TestShellWebBlobRegistryImpl;

namespace WebKit {
  class WebAudioDevice;
}

typedef struct _HyphenDict HyphenDict;

// An implementation of WebKitPlatformSupport for tests.
class TestWebKitPlatformSupport :
    public webkit_glue::WebKitPlatformSupportImpl {
 public:
  TestWebKitPlatformSupport(bool unit_test_mode,
                            WebKit::Platform* shadow_platform_delegate);
  virtual ~TestWebKitPlatformSupport();

  virtual WebKit::WebMimeRegistry* mimeRegistry() OVERRIDE;
  virtual WebKit::WebClipboard* clipboard() OVERRIDE;
  virtual WebKit::WebFileUtilities* fileUtilities() OVERRIDE;
  virtual WebKit::WebSandboxSupport* sandboxSupport() OVERRIDE;
  virtual WebKit::WebCookieJar* cookieJar() OVERRIDE;
  virtual WebKit::WebBlobRegistry* blobRegistry() OVERRIDE;
  virtual WebKit::WebFileSystem* fileSystem() OVERRIDE;

  virtual bool sandboxEnabled() OVERRIDE;
  virtual WebKit::WebKitPlatformSupport::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags) OVERRIDE;
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir) OVERRIDE;
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier) OVERRIDE;
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length) OVERRIDE;
  virtual bool isLinkVisited(unsigned long long linkHash) OVERRIDE;
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel() OVERRIDE;
  virtual void prefetchHostName(const WebKit::WebString&) OVERRIDE;
  virtual WebKit::WebURLLoader* createURLLoader() OVERRIDE;
  virtual WebKit::WebData loadResource(const char* name) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1,
      const WebKit::WebString& value2) OVERRIDE;
  virtual WebKit::WebString defaultLocale() OVERRIDE;
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota) OVERRIDE;

  virtual WebKit::WebIDBFactory* idbFactory() OVERRIDE;

#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetThemeEngine(WebKit::WebThemeEngine* engine);
  virtual WebKit::WebThemeEngine *themeEngine() OVERRIDE;
#endif

  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes&);
  virtual bool canAccelerate2dCanvas();

  WebURLLoaderMockFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  const FilePath& file_system_root() const {
    return file_system_root_.path();
  }

  // Mock out the WebAudioDevice since the real one
  // talks with the browser process.
  virtual double audioHardwareSampleRate() OVERRIDE;
  virtual size_t audioHardwareBufferSize() OVERRIDE;
  virtual WebKit::WebAudioDevice* createAudioDevice(size_t bufferSize,
      unsigned numberOfChannels, double sampleRate,
      WebKit::WebAudioDevice::RenderCallback*) OVERRIDE;

  virtual void sampleGamepads(WebKit::WebGamepads& data);
  void setGamepadData(const WebKit::WebGamepads& data);

  virtual string16 GetLocalizedString(int message_id) OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE;
  virtual void GetPlugins(bool refresh,
                          std::vector<webkit::WebPluginInfo>* plugins) OVERRIDE;
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
      OVERRIDE;
  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate) OVERRIDE;

  virtual WebKit::WebMediaStreamCenter* createMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client) OVERRIDE;
  virtual WebKit::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client) OVERRIDE;
  virtual bool canHyphenate(const WebKit::WebString& locale) OVERRIDE;
  virtual size_t computeLastHyphenLocation(
      const char16* characters,
      size_t length,
      size_t before_index,
      const WebKit::WebString& locale) OVERRIDE;

  virtual WebKit::WebGestureCurve* createFlingAnimationCurve(
      int device_source,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulative_scroll) OVERRIDE;

 private:
  TestShellWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir appcache_dir_;
  SimpleAppCacheSystem appcache_system_;
  SimpleDatabaseSystem database_system_;
  SimpleDomStorageSystem dom_storage_system_;
  SimpleWebCookieJarImpl cookie_jar_;
  scoped_refptr<TestShellWebBlobRegistryImpl> blob_registry_;
  SimpleFileSystem file_system_;
  base::ScopedTempDir file_system_root_;
  WebURLLoaderMockFactory url_loader_factory_;
  bool unit_test_mode_;
  WebKit::WebGamepads gamepad_data_;
  WebKit::Platform* shadow_platform_delegate_;
  HyphenDict* hyphen_dictionary_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitPlatformSupport);
};

#endif  // WEBKIT_SUPPORT_TEST_WEBKIT_PLATFORM_SUPPORT_H_
