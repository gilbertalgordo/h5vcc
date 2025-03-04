# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../../..',
      },{
        'webkit_src_dir': '../../third_party/WebKit',
      }],
    ],
  },
  'target_defaults': {
    'conditions': [
      ['OS != "lb_shell" or target_arch == "linux"', {
        # Disable narrowing-conversion-in-initialization-list warnings
        # in that we do not want to fix it in data file "webcursor_gtk_data.h".
        'cflags+': ['-Wno-narrowing'],
        'cflags_cc+': ['-Wno-narrowing'],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'grit_grd_file': 'resources/webkit_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'webkit_chromium_resources',
          'variables': {
            'grit_grd_file': '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'webkit_unscaled_resources',
          'variables': {
            'grit_grd_file': 'resources/webkit_unscaled_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [ '<(grit_out_dir)' ],
      },
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'grit_grd_file': 'webkit_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'glue',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_EXTENSIONS_IMPLEMENTATION',
        'WEBKIT_GLUE_IMPLEMENTATION',
        'WEBKIT_PLUGINS_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/webkit/compositor_bindings/compositor_bindings.gyp:webkit_compositor_support',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
        'user_agent',
        'webkit_base',
        'webkit_media',
        'webkit_resources',
        'webkit_storage',
        'webkit_strings',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        # for Platform.h, used to define the USE() macro and build switches for chromium
        '../../third_party/webkit/Source/JavaScriptCore/wtf',
      ],
      'sources': [
        # This list contains all .h, .cc, and .mm files in glue except for
        # those in the test subdirectory and those with unittest in in their
        # names.
        '../plugins/npapi/gtk_plugin_container.cc',
        '../plugins/npapi/gtk_plugin_container.h',
        '../plugins/npapi/gtk_plugin_container_manager.cc',
        '../plugins/npapi/gtk_plugin_container_manager.h',
        '../plugins/npapi/plugin_constants_win.cc',
        '../plugins/npapi/plugin_constants_win.h',
        '../plugins/npapi/plugin_host.cc',
        '../plugins/npapi/plugin_host.h',
        '../plugins/npapi/plugin_instance.cc',
        '../plugins/npapi/plugin_instance.h',
        '../plugins/npapi/plugin_instance_mac.mm',
        '../plugins/npapi/plugin_lib.cc',
        '../plugins/npapi/plugin_lib.h',
        '../plugins/npapi/plugin_lib_mac.mm',
        '../plugins/npapi/plugin_lib_posix.cc',
        '../plugins/npapi/plugin_lib_win.cc',
        '../plugins/npapi/plugin_list.cc',
        '../plugins/npapi/plugin_list.h',
        '../plugins/npapi/plugin_list_mac.mm',
        '../plugins/npapi/plugin_list_posix.cc',
        '../plugins/npapi/plugin_list_win.cc',
        '../plugins/npapi/plugin_stream.cc',
        '../plugins/npapi/plugin_stream.h',
        '../plugins/npapi/plugin_stream_posix.cc',
        '../plugins/npapi/plugin_stream_url.cc',
        '../plugins/npapi/plugin_stream_url.h',
        '../plugins/npapi/plugin_stream_win.cc',
        '../plugins/npapi/plugin_string_stream.cc',
        '../plugins/npapi/plugin_string_stream.h',
        '../plugins/npapi/plugin_utils.cc',
        '../plugins/npapi/plugin_utils.h',
        '../plugins/npapi/plugin_web_event_converter_mac.h',
        '../plugins/npapi/plugin_web_event_converter_mac.mm',
        '../plugins/npapi/webplugin.cc',
        '../plugins/npapi/webplugin.h',
        '../plugins/npapi/webplugin_accelerated_surface_mac.h',
        '../plugins/npapi/webplugin_delegate.h',
        '../plugins/npapi/webplugin_delegate_impl.cc',
        '../plugins/npapi/webplugin_delegate_impl.h',
        '../plugins/npapi/webplugin_delegate_impl_android.cc',
        '../plugins/npapi/webplugin_delegate_impl_aura.cc',
        '../plugins/npapi/webplugin_delegate_impl_gtk.cc',
        '../plugins/npapi/webplugin_delegate_impl_mac.mm',
        '../plugins/npapi/webplugin_delegate_impl_win.cc',
        '../plugins/npapi/webplugin_ime_win.cc',
        '../plugins/npapi/webplugin_ime_win.h',
        '../plugins/npapi/webplugin_impl.cc',
        '../plugins/npapi/webplugin_impl.h',
        '../plugins/plugin_constants.cc',
        '../plugins/plugin_constants.h',
        '../plugins/plugin_switches.cc',
        '../plugins/plugin_switches.h',
        '../plugins/ppapi/audio_helper.cc',
        '../plugins/ppapi/audio_helper.h',
        '../plugins/ppapi/common.h',
        '../plugins/ppapi/content_decryptor_delegate.cc',
        '../plugins/ppapi/content_decryptor_delegate.h',
        '../plugins/ppapi/event_conversion.cc',
        '../plugins/ppapi/event_conversion.h',
        '../plugins/ppapi/file_callbacks.cc',
        '../plugins/ppapi/file_callbacks.h',
        '../plugins/ppapi/fullscreen_container.h',
        '../plugins/ppapi/gfx_conversion.h',
        '../plugins/ppapi/host_array_buffer_var.cc',
        '../plugins/ppapi/host_array_buffer_var.h',
        '../plugins/ppapi/host_globals.cc',
        '../plugins/ppapi/host_globals.h',
        '../plugins/ppapi/host_var_tracker.cc',
        '../plugins/ppapi/host_var_tracker.h',
        '../plugins/ppapi/message_channel.cc',
        '../plugins/ppapi/message_channel.h',
        '../plugins/ppapi/npapi_glue.cc',
        '../plugins/ppapi/npapi_glue.h',
        '../plugins/ppapi/npobject_var.cc',
        '../plugins/ppapi/npobject_var.h',
        '../plugins/ppapi/plugin_delegate.h',
        '../plugins/ppapi/plugin_module.cc',
        '../plugins/ppapi/plugin_module.h',
        '../plugins/ppapi/plugin_object.cc',
        '../plugins/ppapi/plugin_object.h',
        '../plugins/ppapi/ppapi_interface_factory.cc',
        '../plugins/ppapi/ppapi_interface_factory.h',
        '../plugins/ppapi/ppapi_plugin_instance.cc',
        '../plugins/ppapi/ppapi_plugin_instance.h',
        '../plugins/ppapi/ppapi_webplugin_impl.cc',
        '../plugins/ppapi/ppapi_webplugin_impl.h',
        '../plugins/ppapi/ppb_audio_impl.cc',
        '../plugins/ppapi/ppb_audio_impl.h',
        '../plugins/ppapi/ppb_broker_impl.cc',
        '../plugins/ppapi/ppb_broker_impl.h',
        '../plugins/ppapi/ppb_buffer_impl.cc',
        '../plugins/ppapi/ppb_buffer_impl.h',
        '../plugins/ppapi/ppb_directory_reader_impl.cc',
        '../plugins/ppapi/ppb_directory_reader_impl.h',
        '../plugins/ppapi/ppb_file_io_impl.cc',
        '../plugins/ppapi/ppb_file_io_impl.h',
        '../plugins/ppapi/ppb_file_ref_impl.cc',
        '../plugins/ppapi/ppb_file_ref_impl.h',
        '../plugins/ppapi/ppb_file_system_impl.cc',
        '../plugins/ppapi/ppb_file_system_impl.h',
        '../plugins/ppapi/ppb_flash_impl.cc',
        '../plugins/ppapi/ppb_flash_impl.h',
        '../plugins/ppapi/ppb_flash_message_loop_impl.cc',
        '../plugins/ppapi/ppb_flash_message_loop_impl.h',
        '../plugins/ppapi/ppb_gpu_blacklist_private_impl.cc',
        '../plugins/ppapi/ppb_gpu_blacklist_private_impl.h',
        '../plugins/ppapi/ppb_graphics_2d_impl.cc',
        '../plugins/ppapi/ppb_graphics_2d_impl.h',
        '../plugins/ppapi/ppb_graphics_3d_impl.cc',
        '../plugins/ppapi/ppb_graphics_3d_impl.h',
        '../plugins/ppapi/ppb_host_resolver_private_impl.cc',
        '../plugins/ppapi/ppb_host_resolver_private_impl.h',
        '../plugins/ppapi/ppb_image_data_impl.cc',
        '../plugins/ppapi/ppb_image_data_impl.h',
        '../plugins/ppapi/ppb_network_monitor_private_impl.cc',
        '../plugins/ppapi/ppb_network_monitor_private_impl.h',
        '../plugins/ppapi/ppb_proxy_impl.cc',
        '../plugins/ppapi/ppb_proxy_impl.h',
        '../plugins/ppapi/ppb_scrollbar_impl.cc',
        '../plugins/ppapi/ppb_scrollbar_impl.h',
        '../plugins/ppapi/ppb_tcp_server_socket_private_impl.cc',
        '../plugins/ppapi/ppb_tcp_server_socket_private_impl.h',
        '../plugins/ppapi/ppb_tcp_socket_private_impl.cc',
        '../plugins/ppapi/ppb_tcp_socket_private_impl.h',
        '../plugins/ppapi/ppb_udp_socket_private_impl.cc',
        '../plugins/ppapi/ppb_udp_socket_private_impl.h',
        '../plugins/ppapi/ppb_uma_private_impl.cc',
        '../plugins/ppapi/ppb_uma_private_impl.h',
        '../plugins/ppapi/ppb_url_loader_impl.cc',
        '../plugins/ppapi/ppb_url_loader_impl.h',
        '../plugins/ppapi/ppb_var_deprecated_impl.cc',
        '../plugins/ppapi/ppb_var_deprecated_impl.h',
        '../plugins/ppapi/ppb_video_decoder_impl.cc',
        '../plugins/ppapi/ppb_video_decoder_impl.h',
        '../plugins/ppapi/ppb_widget_impl.cc',
        '../plugins/ppapi/ppb_widget_impl.h',
        '../plugins/ppapi/ppb_x509_certificate_private_impl.cc',
        '../plugins/ppapi/ppb_x509_certificate_private_impl.h',
        '../plugins/ppapi/quota_file_io.cc',
        '../plugins/ppapi/quota_file_io.h',
        '../plugins/ppapi/resource_creation_impl.cc',
        '../plugins/ppapi/resource_creation_impl.h',
        '../plugins/ppapi/resource_helper.cc',
        '../plugins/ppapi/resource_helper.h',
        '../plugins/ppapi/string.cc',
        '../plugins/ppapi/string.h',
        '../plugins/ppapi/url_response_info_util.cc',
        '../plugins/ppapi/url_response_info_util.h',
        '../plugins/ppapi/url_request_info_util.cc',
        '../plugins/ppapi/url_request_info_util.h',
        '../plugins/ppapi/usb_key_code_conversion.h',
        '../plugins/ppapi/usb_key_code_conversion.cc',
        '../plugins/ppapi/usb_key_code_conversion_linux.cc',
        '../plugins/ppapi/usb_key_code_conversion_mac.cc',
        '../plugins/ppapi/usb_key_code_conversion_win.cc',
        '../plugins/sad_plugin.cc',
        '../plugins/sad_plugin.h',
        '../plugins/webkit_plugins_export.h',
        '../plugins/webplugininfo.cc',
        '../plugins/webplugininfo.h',
        '../plugins/webview_plugin.cc',
        '../plugins/webview_plugin.h',
        'alt_error_page_resource_fetcher.cc',
        'alt_error_page_resource_fetcher.h',
        'cpp_binding_example.cc',
        'cpp_binding_example.h',
        'cpp_bound_class.cc',
        'cpp_bound_class.h',
        'cpp_variant.cc',
        'cpp_variant.h',
        'dom_operations.cc',
        'dom_operations.h',
        'fling_animator_impl_android.cc',
        'fling_animator_impl_android.h',
        'ftp_directory_listing_response_delegate.cc',
        'ftp_directory_listing_response_delegate.h',
        'glue_serialize.cc',
        'glue_serialize.h',
        'image_decoder.cc',
        'image_decoder.h',
        'image_resource_fetcher.cc',
        'image_resource_fetcher.h',
        'multi_resolution_image_resource_fetcher.cc',
        'multi_resolution_image_resource_fetcher.h',
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'network_list_observer.h',
        'npruntime_util.cc',
        'npruntime_util.h',
        'resource_fetcher.cc',
        'resource_fetcher.h',
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'resource_request_body.cc',
        'resource_request_body.h',
        'resource_type.cc',
        'resource_type.h',
        'scoped_clipboard_writer_glue.cc',
        'scoped_clipboard_writer_glue.h',
        'simple_webmimeregistry_impl.cc',
        'simple_webmimeregistry_impl.h',
        'touch_fling_platform_gesture_curve.cc',
        'touch_fling_platform_gesture_curve.h',
        'webclipboard_impl.cc',
        'webclipboard_impl.h',
        'webcookie.cc',
        'webcookie.h',
        'webcursor.cc',
        'webcursor.h',
        'webcursor_android.cc',
        'webcursor_aura.cc',
        'webcursor_aurawin.cc',
        'webcursor_aurax11.cc',
        'webcursor_gtk.cc',
        'webcursor_gtk_data.h',
        'webcursor_mac.mm',
        'webcursor_win.cc',
        'webdropdata.cc',
        'webdropdata_win.cc',
        'webdropdata.h',
        'webfileutilities_impl.cc',
        'webfileutilities_impl.h',
        'webkit_constants.h',
        'webkit_glue.cc',
        'webkit_glue.h',
        'webkit_glue_export.h',
        'webkitplatformsupport_impl.cc',
        'webkitplatformsupport_impl.h',
        'webmenuitem.cc',
        'webmenuitem.h',
        'webmenurunner_mac.h',
        'webmenurunner_mac.mm',
        'webpreferences.cc',
        'webpreferences.h',
        'websocketstreamhandle_bridge.h',
        'websocketstreamhandle_delegate.h',
        'websocketstreamhandle_impl.cc',
        'websocketstreamhandle_impl.h',
        'webthemeengine_impl_android.cc',
        'webthemeengine_impl_android.h',
        'webthemeengine_impl_default.cc',
        'webthemeengine_impl_default.h',
        'webthemeengine_impl_mac.cc',
        'webthemeengine_impl_mac.h',
        'webthemeengine_impl_win.cc',
        'webthemeengine_impl_win.h',
        'webthread_impl.h',
        'webthread_impl.cc',
        'weburlloader_impl.cc',
        'weburlloader_impl.h',
        'weburlrequest_extradata_impl.cc',
        'weburlrequest_extradata_impl.h',
        'weburlresponse_extradata_impl.cc',
        'weburlresponse_extradata_impl.h',
        'web_intent_data.cc',
        'web_intent_data.h',
        'web_intent_reply_data.cc',
        'web_intent_reply_data.h',
        'web_intent_service_data.cc',
        'web_intent_service_data.h',
        'web_io_operators.cc',
        'web_io_operators.h',
        'window_open_disposition.h',
        'window_open_disposition.cc',
        'worker_task_runner.cc',
        'worker_task_runner.h',
      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'conditions': [
        ['use_default_render_theme==0', {
          'sources/': [
            ['exclude', 'webthemeengine_impl_default.cc'],
            ['exclude', 'webthemeengine_impl_default.h'],
          ],
        }, {  # else: use_default_render_theme==1
          'sources/': [
            ['exclude', 'webthemeengine_impl_win.cc'],
            ['exclude', 'webthemeengine_impl_win.h'],
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
          'sources!': [
            'plugins/plugin_stubs.cc',
          ],
          'conditions': [
            ['linux_use_tcmalloc == 1', {
              'dependencies': [
                # This is needed by ../extensions/v8/heap_profiler_extension.cc
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['use_aura==1', {
          'sources/': [
            ['exclude', '^\\.\\./plugins/npapi/webplugin_delegate_impl_mac.mm'],
          ],
          'sources!': [
            'webcursor_mac.mm',
            'webcursor_win.cc',
          ],
        }],
        ['OS=="lb_shell"', {
          'include_dirs': [
            '<(DEPTH)/v8/include',
          ],
          'sources/' : [
            ['exclude', 'plugin'],
            ['exclude', 'clipboard'],
            ['exclude', 'ftp'],
            ['exclude', 'cpp_bound_class'],
            ['exclude', 'cpp_binding'],
            ['exclude', 'cpp_variant'],
            ['exclude', 'npruntime_util'],
            ['exclude', 'audio_decoder'],
            ['exclude', 'webmediaplayer_impl\\.(cc|h)$'],
            ['exclude', 'gl_bindings'],
            ['exclude', '../extensions/v8/'],
            ['exclude', 'webcursor'],
            ['exclude', 'webdrop'],
            ['exclude', 'webthemeengine_impl_'],
          ],
          'sources' : [
            'webthemeengine_impl_default.cc',
          ],
          'dependencies': [
            '<(lbshell_root)/build/projects/posix_emulation.gyp:posix_emulation',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
            '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
            '<(DEPTH)/media/media.gyp:media',
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_c',
            '<(DEPTH)/ppapi/ppapi_internal.gyp:ppapi_shared',
            '<(DEPTH)/printing/printing.gyp:printing',
            '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
            '<(DEPTH)/ui/gl/gl.gyp:gl',
            '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
            '<(DEPTH)/ui/native_theme/native_theme.gyp:native_theme',
          ]
        }],
        ['use_aura==1 and use_x11==1', {
          'link_settings': {
            'libraries': [ '-lXcursor', ],
          },
        }],
        ['use_aura==1 and OS=="win"', {
          'sources/': [
            ['exclude', '^\\.\\./plugins/npapi/webplugin_delegate_impl_aura'],
          ],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
          'sources!': [
            'webthemeengine_impl_mac.cc',
          ],
        }, {  # else: OS=="mac"
          'sources/': [['exclude', 'plugin_(lib|list)_posix\\.cc$']],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['enable_gpu!=1', {
          'sources!': [
            '../plugins/ppapi/ppb_graphics_3d_impl.cc',
            '../plugins/ppapi/ppb_graphics_3d_impl.h',
            '../plugins/ppapi/ppb_open_gl_es_impl.cc',
          ],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
          'sources!': [
            'webthemeengine_impl_win.cc',
          ],
        }, {  # else: OS=="win"
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources!': [
            'plugins/plugin_stubs.cc',
          ],
          'msvs_disabled_warnings': [ 4800 ],
          'conditions': [
            ['inside_chromium_build==1 and component=="shared_library"', {
              'dependencies': [
                '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
               ],
               'export_dependent_settings': [
                 '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
                 '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
               ],
            }],
          ],
        }],
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_third_party_translations==1', {
      'targets': [
        {
          'target_name': 'inspector_strings',
          'type': 'none',
          'variables': {
            'grit_out_dir': '<(PRODUCT_DIR)/resources/inspector/l10n',
          },
          'actions': [
            {
              'action_name': 'inspector_strings',
              'variables': {
                'grit_grd_file': 'inspector_strings.grd',
              },
              'includes': [ '../../build/grit_action.gypi' ],
            },
          ],
          'includes': [ '../../build/grit_target.gypi' ],
        },
      ],
    }],
  ],
}
