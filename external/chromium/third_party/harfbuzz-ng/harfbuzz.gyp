# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_harfbuzz%': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'conditions': [
    ['use_system_harfbuzz==0', {
      'targets': [
        {
          'target_name': 'harfbuzz-ng',
          'type': 'static_library',
          'defines': [
            'HAVE_OT',
            'HAVE_ICU',
            'HB_NO_MT',
          ],
          'sources': [
            'src/hb-atomic-private.hh',
            'src/hb-blob.cc',
            'src/hb-blob.h',
            'src/hb-buffer-private.hh',
            'src/hb-buffer.cc',
            'src/hb-buffer.h',
            'src/hb-cache-private.hh',
            'src/hb-common.cc',
            'src/hb-common.h',
            'src/hb-fallback-shape.cc',
            'src/hb-font-private.hh',
            'src/hb-font.cc',
            'src/hb-font.h',
            'src/hb-icu.cc',
            'src/hb-icu.h',
            'src/hb-mutex-private.hh',
            'src/hb-object-private.hh',
            'src/hb-open-file-private.hh',
            'src/hb-open-type-private.hh',
            'src/hb-ot-head-table.hh',
            'src/hb-ot-hhea-table.hh',
            'src/hb-ot-hmtx-table.hh',
            'src/hb-ot-layout-common-private.hh',
            'src/hb-ot-layout-gdef-table.hh',
            'src/hb-ot-layout-gpos-table.hh',
            'src/hb-ot-layout-gsub-table.hh',
            'src/hb-ot-layout-gsubgpos-private.hh',
            'src/hb-ot-layout-private.hh',
            'src/hb-ot-layout.cc',
            'src/hb-ot-layout.h',
            'src/hb-ot-map-private.hh',
            'src/hb-ot-map.cc',
            'src/hb-ot-maxp-table.hh',
            'src/hb-ot-name-table.hh',
            'src/hb-ot-shape-complex-arabic-fallback.hh',
            'src/hb-ot-shape-complex-arabic-table.hh',
            'src/hb-ot-shape-complex-arabic.cc',
            'src/hb-ot-shape-complex-indic-machine.hh',
            'src/hb-ot-shape-complex-indic-private.hh',
            'src/hb-ot-shape-complex-indic-table.hh',
            'src/hb-ot-shape-complex-indic.cc',
            'src/hb-ot-shape-complex-misc.cc',
            'src/hb-ot-shape-complex-private.hh',
            'src/hb-ot-shape-fallback-private.hh',
            'src/hb-ot-shape-fallback.cc',
            'src/hb-ot-shape-normalize-private.hh',
            'src/hb-ot-shape-normalize.cc',
            'src/hb-ot-shape-private.hh',
            'src/hb-ot-shape.cc',
            'src/hb-ot-tag.cc',
            'src/hb-ot-tag.h',
            'src/hb-ot.h',
            'src/hb-private.hh',
            'src/hb-set-private.hh',
            'src/hb-set.cc',
            'src/hb-set.h',
            'src/hb-shape-plan-private.hh',
            'src/hb-shape-plan.cc',
            'src/hb-shape-plan.h',
            'src/hb-shape.cc',
            'src/hb-shape.h',
            'src/hb-shaper-impl-private.hh',
            'src/hb-shaper-list.hh',
            'src/hb-shaper-private.hh',
            'src/hb-shaper.cc',
            'src/hb-tt-font.cc',
            'src/hb-unicode-private.hh',
            'src/hb-unicode.cc',
            'src/hb-unicode.h',
            'src/hb-utf-private.hh',
            'src/hb-version.h',
            'src/hb-warning.cc',
            'src/hb.h',
          ],
          'include_dirs': [
            'src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
          'dependencies': [
            '../../third_party/icu/icu.gyp:icuuc',
          ],
          'conditions': [
            ['clang==1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  '-Wno-unused-value',
                ],
              },
              'cflags': [
                '-Wno-unused-value',
              ]
            }],
          ],
        },
      ],
    }, { # use_system_harfbuzz == 1
      'targets': [
        {
          'target_name': 'harfbuzz-ng',
          'type': 'none',
          'cflags': [
            '<!@(pkg-config --cflags harfbuzz)',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags harfbuzz)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other harfbuzz)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l harfbuzz)',
            ],
          },
        },
      ],
    }],
  ],
}
