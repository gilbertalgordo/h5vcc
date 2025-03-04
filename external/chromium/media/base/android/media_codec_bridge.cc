// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_bridge.h"

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
//#include "base/safe_numerics.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "jni/MediaCodecBridge_jni.h"
#include "media/base/bit_reader.h"
#include "media/base/decrypt_config.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace media {

enum { kBufferFlagEndOfStream = 4 };

static const std::string AudioCodecToAndroidMimeType(const AudioCodec& codec) {
  switch (codec) {
    case kCodecMP3:
      return "audio/mpeg";
    case kCodecVorbis:
      return "audio/vorbis";
    case kCodecAAC:
      return "audio/mp4a-latm";
    default:
      return std::string();
  }
}

static const std::string VideoCodecToAndroidMimeType(const VideoCodec& codec) {
  switch (codec) {
    case kCodecH264:
      return "video/avc";
    case kCodecVP8:
      return "video/x-vnd.on2.vp8";
    case kCodecVP9:
      return "video/x-vnd.on2.vp9";
    default:
      return std::string();
  }
}

static const std::string CodecTypeToAndroidMimeType(const std::string& codec) {
  // TODO(xhwang): Shall we handle more detailed strings like "mp4a.40.2"?
  if (codec == "avc1")
    return "video/avc";
  if (codec == "mp4a")
    return "audio/mp4a-latm";
  if (codec == "vp8" || codec == "vp8.0")
    return "video/x-vnd.on2.vp8";
  if (codec == "vp9" || codec  == "vp9.0")
    return "video/x-vnd.on2.vp9";
  if (codec == "vorbis")
    return "audio/vorbis";
  return std::string();
}

// TODO(qinmin): using a map to help all the conversions in this class.
static const std::string AndroidMimeTypeToCodecType(const std::string& mime) {
  if (mime == "video/mp4v-es")
    return "mp4v";
  if (mime == "video/avc")
    return "avc1";
  if (mime == "video/x-vnd.on2.vp8")
    return "vp8";
  if (mime == "video/x-vnd.on2.vp9")
    return "vp9";
  if (mime == "audio/mp4a-latm")
    return "mp4a";
  if (mime == "audio/mpeg")
    return "mp3";
  if (mime == "audio/vorbis")
    return "vorbis";
  return std::string();
}

static ScopedJavaLocalRef<jintArray> ToJavaIntArray(
    JNIEnv* env, const std::vector<jint>& native_array, int size) {
  ScopedJavaLocalRef<jintArray> j_array(env, env->NewIntArray(size));
  env->SetIntArrayRegion(j_array.obj(), 0, size, size == 0 ? NULL : &native_array[0]);
  return j_array;
}

// static
bool MediaCodecBridge::IsAvailable() {
  // MediaCodec is only available on JB and greater.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 16;
}

// static
bool MediaCodecBridge::CanDecode(const std::string& codec, bool is_secure) {
  JNIEnv* env = AttachCurrentThread();
  std::string mime = CodecTypeToAndroidMimeType(codec);
  if (mime.empty())
    return false;
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_media_codec_bridge =
      Java_MediaCodecBridge_create(env, j_mime.obj(), is_secure);
  if (!j_media_codec_bridge.is_null()) {
    Java_MediaCodecBridge_release(env, j_media_codec_bridge.obj());
    return true;
  }
  return false;
}

MediaCodecBridge::MediaCodecBridge(const std::string& mime, bool is_secure) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  DCHECK(!mime.empty());
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  j_media_codec_.Reset(
      Java_MediaCodecBridge_create(env, j_mime.obj(), is_secure));
}

MediaCodecBridge::~MediaCodecBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (j_media_codec_.obj())
    Java_MediaCodecBridge_release(env, j_media_codec_.obj());
}

bool MediaCodecBridge::StartInternal() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_start(env, j_media_codec_.obj()) &&
         GetOutputBuffers();
}

MediaCodecStatus MediaCodecBridge::Reset() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_flush(env, j_media_codec_.obj()));
}

void MediaCodecBridge::Stop() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_stop(env, j_media_codec_.obj());
}

void MediaCodecBridge::GetOutputFormat(int* width, int* height) {
  JNIEnv* env = AttachCurrentThread();

  *width = Java_MediaCodecBridge_getOutputWidth(env, j_media_codec_.obj());
  *height = Java_MediaCodecBridge_getOutputHeight(env, j_media_codec_.obj());
}

MediaCodecStatus MediaCodecBridge::QueueInputBuffer(
    int index, const uint8* data, int data_size,
    const base::TimeDelta& presentation_time) {
  if (!FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(Java_MediaCodecBridge_queueInputBuffer(
      env, j_media_codec_.obj(),
      index, 0, data_size, presentation_time.InMicroseconds(), 0));
}

MediaCodecStatus MediaCodecBridge::QueueSecureInputBuffer(
    int index, const uint8* data, int data_size, const uint8* key_id,
    int key_id_size, const uint8* iv, int iv_size,
    const SubsampleEntry* subsamples, int subsamples_size,
    const base::TimeDelta& presentation_time) {
  if (!FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_key_id =
      base::android::ToJavaByteArray(env, key_id, key_id_size);
  ScopedJavaLocalRef<jbyteArray> j_iv =
      base::android::ToJavaByteArray(env, iv, iv_size);

  // MediaCodec.CryptoInfo documentations says passing NULL for |clear_array|
  // to indicate that all data is encrypted. But it doesn't specify what
  // |cypher_array| and |subsamples_size| should be in that case. Passing
  // one subsample here just to be on the safe side.
  int new_subsamples_size = subsamples_size == 0 ? 1 : subsamples_size;

  std::vector<jint> native_clear_array(new_subsamples_size);
  std::vector<jint> native_cypher_array(new_subsamples_size);

  if (subsamples_size == 0) {
    DCHECK(!subsamples);
    native_clear_array[0] = 0;
    native_cypher_array[0] = data_size;
  } else {
    DCHECK_GT(subsamples_size, 0);
    DCHECK(subsamples);
    for (int i = 0; i < subsamples_size; ++i) {
      native_clear_array[i] = subsamples[i].clear_bytes;
      native_cypher_array[i] = subsamples[i].cypher_bytes;
    }
  }

  ScopedJavaLocalRef<jintArray> clear_array =
      ToJavaIntArray(env, native_clear_array, new_subsamples_size);
  ScopedJavaLocalRef<jintArray> cypher_array =
      ToJavaIntArray(env, native_cypher_array, new_subsamples_size);

  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_queueSecureInputBuffer(
          env, j_media_codec_.obj(), index, 0, j_iv.obj(), j_key_id.obj(),
          clear_array.obj(), cypher_array.obj(), new_subsamples_size,
          presentation_time.InMicroseconds()));
}

void MediaCodecBridge::QueueEOS(int input_buffer_index) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_queueInputBuffer(
      env, j_media_codec_.obj(),
      input_buffer_index, 0, 0, 0, kBufferFlagEndOfStream);
}

MediaCodecStatus MediaCodecBridge::DequeueInputBuffer(
    const base::TimeDelta& timeout, int* index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result = Java_MediaCodecBridge_dequeueInputBuffer(
      env, j_media_codec_.obj(), timeout.InMicroseconds());
  *index = Java_DequeueInputResult_index(env, result.obj());
  return static_cast<MediaCodecStatus>(
      Java_DequeueInputResult_status(env, result.obj()));
}

MediaCodecStatus MediaCodecBridge::DequeueOutputBuffer(
    const base::TimeDelta& timeout, int* index, size_t* offset, size_t* size,
    base::TimeDelta* presentation_time, bool* end_of_stream) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_dequeueOutputBuffer(env, j_media_codec_.obj(),
                                                timeout.InMicroseconds());
  *index = Java_DequeueOutputResult_index(env, result.obj());;
  *offset =Java_DequeueOutputResult_offset(env, result.obj());
  *size = Java_DequeueOutputResult_numBytes(env, result.obj());
  *presentation_time = base::TimeDelta::FromMicroseconds(
      Java_DequeueOutputResult_presentationTimeMicroseconds(env, result.obj()));
  int flags = Java_DequeueOutputResult_flags(env, result.obj());
  *end_of_stream = flags & kBufferFlagEndOfStream;
  return static_cast<MediaCodecStatus>(
      Java_DequeueOutputResult_status(env, result.obj()));
}

void MediaCodecBridge::FillBufferWithOutputBuffer(
    int index, size_t offset, size_t size, uint8* buffer_to_fill) {
  DCHECK_GE(index, 0);
  int numBytes = size;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> buf =
      Java_MediaCodecBridge_getOutputBuffer(env, media_codec(), index);
  uint8* source_buffer =
      static_cast<uint8*>(env->GetDirectBufferAddress(buf.obj()));
  memcpy(buffer_to_fill, source_buffer + offset, size);
}

void MediaCodecBridge::ReleaseOutputBuffer(int index, bool render) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_MediaCodecBridge_releaseOutputBuffer(
      env, j_media_codec_.obj(), index, render);
}

bool MediaCodecBridge::GetOutputBuffers() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getOutputBuffers(env, j_media_codec_.obj());
}

bool MediaCodecBridge::FillInputBuffer(int index, const uint8* data, int size) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_buffer(
      Java_MediaCodecBridge_getInputBuffer(env, j_media_codec_.obj(), index));
  jlong capacity = env->GetDirectBufferCapacity(j_buffer.obj());
  if (size > capacity) {
    LOG(ERROR) << "Input buffer size " << size
               << " exceeds MediaCodec input buffer capacity: " << capacity;
    return false;
  }

  uint8* direct_buffer =
      static_cast<uint8*>(env->GetDirectBufferAddress(j_buffer.obj()));
  memcpy(direct_buffer, data, size);
  return true;
}

AudioCodecBridge::AudioCodecBridge(const std::string& mime)
    // Audio codec doesn't care about security level.
    : MediaCodecBridge(mime, false) {
}

bool AudioCodecBridge::Start(
    const AudioCodec& codec, int sample_rate, int channel_count,
    const uint8* extra_data, size_t extra_data_size, bool play_audio,
    jobject media_crypto) {
  JNIEnv* env = AttachCurrentThread();

  if (!media_codec())
    return false;

  std::string codec_string = AudioCodecToAndroidMimeType(codec);
  if (codec_string.empty())
    return false;

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, codec_string);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createAudioFormat(
          env, j_mime.obj(), sample_rate, channel_count));
  DCHECK(!j_format.is_null());

  if (!ConfigureMediaFormat(j_format.obj(), codec, extra_data, extra_data_size))
    return false;

  if (!Java_MediaCodecBridge_configureAudio(
      env, media_codec(), j_format.obj(), media_crypto, 0, play_audio)) {
    return false;
  }

  return StartInternal();
}

bool AudioCodecBridge::ConfigureMediaFormat(
    jobject j_format, const AudioCodec& codec, const uint8* extra_data,
    size_t extra_data_size) {
  if (extra_data_size == 0)
    return true;

  JNIEnv* env = AttachCurrentThread();
  switch (codec) {
    case kCodecVorbis:
    {
      if (extra_data[0] != 2) {
        LOG(ERROR) << "Invalid number of vorbis headers before the codec "
                   << "header: " << extra_data[0];
        return false;
      }

      size_t header_length[2];
      // |total_length| keeps track of the total number of bytes before the last
      // header.
      size_t total_length = 1;
      const uint8* current_pos = extra_data;
      // Calculate the length of the first 2 headers.
      for (int i = 0; i < 2; ++i) {
        header_length[i] = 0;
        while (total_length < extra_data_size) {
          size_t size = *(++current_pos);
          total_length += 1 + size;
          if (total_length > 0x80000000) {
            LOG(ERROR) << "Vorbis header size too large";
            return false;
          }
          header_length[i] += size;
          if (size < 0xFF)
            break;
        }
        if (total_length >= extra_data_size) {
          LOG(ERROR) << "Invalid vorbis header size in the extra data";
          return false;
        }
      }
      current_pos++;
      // The first header is identification header.
      ScopedJavaLocalRef<jbyteArray> first_header =
          base::android::ToJavaByteArray(env, current_pos, header_length[0]);
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 0, first_header.obj());
      // The last header is codec header.
      ScopedJavaLocalRef<jbyteArray> last_header =
          base::android::ToJavaByteArray(
              env, extra_data + total_length, extra_data_size - total_length);
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 1, last_header.obj());
      break;
    }
    case kCodecAAC:
    {
      media::BitReader reader(extra_data, extra_data_size);

      // The following code is copied from aac.cc
      // TODO(qinmin): refactor the code in aac.cc to make it more reusable.
      uint8 profile = 0;
      uint8 frequency_index = 0;
      uint8 channel_config = 0;
      uint32 frequency;
      if (!reader.ReadBits(5, &profile) ||
          !reader.ReadBits(4, &frequency_index)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }
      if (0xf == frequency_index && !reader.ReadBits(24, &frequency)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }
      if (!reader.ReadBits(4, &channel_config)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }

      if (profile < 1 || profile > 4 || frequency_index == 0xf ||
          channel_config > 7) {
        LOG(ERROR) << "Invalid AAC header";
        return false;
      }
      const size_t kCsdLength = 2;
      uint8 csd[kCsdLength];
      csd[0] = profile << 3 | frequency_index >> 1;
      csd[1] = (frequency_index & 0x01) << 7 | channel_config << 3;
      ScopedJavaLocalRef<jbyteArray> byte_array =
          base::android::ToJavaByteArray(env, csd, kCsdLength);
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 0, byte_array.obj());

      // TODO(qinmin): pass an extra variable to this function to determine
      // whether we need to call this.
      Java_MediaCodecBridge_setFrameHasADTSHeader(env, j_format);
      break;
    }
    default:
      LOG(ERROR) << "Invalid header encountered for codec: "
                 << AudioCodecToAndroidMimeType(codec);
      return false;
  }
  return true;
}

void AudioCodecBridge::PlayOutputBuffer(int index, size_t size) {
  DCHECK_LE(0, index);
  int numBytes = size;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> buf =
      Java_MediaCodecBridge_getOutputBuffer(env, media_codec(), index);
  uint8* buffer = static_cast<uint8*>(env->GetDirectBufferAddress(buf.obj()));

  ScopedJavaLocalRef<jbyteArray> byte_array =
      base::android::ToJavaByteArray(env, buffer, numBytes);
  Java_MediaCodecBridge_playOutputBuffer(
      env, media_codec(), byte_array.obj());
}

void AudioCodecBridge::SetVolume(double volume) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setVolume(env, media_codec(), volume);
}

VideoCodecBridge::VideoCodecBridge(const std::string& mime, bool is_secure)
    : MediaCodecBridge(mime, is_secure) {
}

bool VideoCodecBridge::Start(
    const VideoCodec& codec, const gfx::Size& size, jobject surface,
    jobject media_crypto) {
  JNIEnv* env = AttachCurrentThread();

  if (!media_codec())
    return false;

  std::string codec_string = VideoCodecToAndroidMimeType(codec);
  if (codec_string.empty())
    return false;

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, codec_string);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createVideoFormat(
          env, j_mime.obj(), size.width(), size.height()));
  DCHECK(!j_format.is_null());
  if (!Java_MediaCodecBridge_configureVideo(
      env, media_codec(), j_format.obj(), surface, media_crypto, 0)) {
    return false;
  }

  return StartInternal();
}

AudioCodecBridge* AudioCodecBridge::Create(const AudioCodec& codec) {
  const std::string mime = AudioCodecToAndroidMimeType(codec);
  return mime.empty() ? NULL : new AudioCodecBridge(mime);
}

VideoCodecBridge* VideoCodecBridge::Create(const VideoCodec& codec,
                                           bool is_secure) {
  const std::string mime = VideoCodecToAndroidMimeType(codec);
  return mime.empty() ? NULL : new VideoCodecBridge(mime, is_secure);
}

bool MediaCodecBridge::RegisterMediaCodecBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace media
