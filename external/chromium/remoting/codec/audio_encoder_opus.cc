// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_opus.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/base/audio_bus.h"
#include "media/base/multi_channel_resampler.h"
#include "third_party/opus/opus.h"

namespace remoting {

namespace {

// Output 160 kb/s bitrate.
const int kOutputBitrateBps = 160 * 1024;

// Encoded buffer size.
const int kFrameDefaultBufferSize = 4096;

// Maximum buffer size we'll allocate when encoding before giving up.
const int kMaxBufferSize = 65536;

// Opus doesn't support 44100 sampling rate so we always resample to 48kHz.
const AudioPacket::SamplingRate kOpusSamplingRate =
    AudioPacket::SAMPLING_RATE_48000;

// Opus supports frame sizes of 2.5, 5, 10, 20, 40 and 60 ms. We use 20 ms
// frames to balance latency and efficiency.
const int kFrameSizeMs = 20;

// Number of samples per frame when using default sampling rate.
const int kFrameSamples =
    kOpusSamplingRate * kFrameSizeMs / base::Time::kMillisecondsPerSecond;

const AudioPacket::BytesPerSample kBytesPerSample =
    AudioPacket::BYTES_PER_SAMPLE_2;

bool IsSupportedSampleRate(int rate) {
  return rate == 44100 || rate == 48000;
}

}  // namespace

AudioEncoderOpus::AudioEncoderOpus()
    : sampling_rate_(0),
      channels_(AudioPacket::CHANNELS_STEREO),
      encoder_(NULL),
      frame_size_(0),
      resampling_data_(NULL),
      resampling_data_size_(0),
      resampling_data_pos_(0) {
}

AudioEncoderOpus::~AudioEncoderOpus() {
  DestroyEncoder();
}

void AudioEncoderOpus::InitEncoder() {
  DCHECK(!encoder_);
  int error;
  encoder_ = opus_encoder_create(kOpusSamplingRate, channels_,
                                 OPUS_APPLICATION_AUDIO, &error);
  if (!encoder_) {
    LOG(ERROR) << "Failed to create OPUS encoder. Error code: " << error;
    return;
  }

  opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(kOutputBitrateBps));

  frame_size_ = sampling_rate_ * kFrameSizeMs /
      base::Time::kMillisecondsPerSecond;

  if (sampling_rate_ != kOpusSamplingRate) {
    resample_buffer_.reset(
        new char[kFrameSamples * kBytesPerSample * channels_]);
    resampler_.reset(new media::MultiChannelResampler(
        channels_,
        static_cast<double>(sampling_rate_) / kOpusSamplingRate,
        base::Bind(&AudioEncoderOpus::FetchBytesToResample,
                   base::Unretained(this))));
    resampler_bus_ = media::AudioBus::Create(channels_, kFrameSamples);
  }

  // Drop leftover data because it's for different sampling rate.
  leftover_samples_ = 0;
  leftover_buffer_size_ =
      frame_size_ + media::SincResampler::kMaximumLookAheadSize;
  leftover_buffer_.reset(
      new int16[leftover_buffer_size_ * channels_]);
}

void AudioEncoderOpus::DestroyEncoder() {
  if (encoder_) {
    opus_encoder_destroy(encoder_);
    encoder_ = NULL;
  }

  resampler_.reset();
}

bool AudioEncoderOpus::ResetForPacket(AudioPacket* packet) {
  if (packet->channels() != channels_ ||
      packet->sampling_rate() != sampling_rate_) {
    DestroyEncoder();

    channels_ = packet->channels();
    sampling_rate_ = packet->sampling_rate();

    if (channels_ <= 0 || channels_ > 2 ||
        !IsSupportedSampleRate(sampling_rate_)) {
      LOG(WARNING) << "Unsupported OPUS parameters: "
                   << channels_ << " channels with "
                   << sampling_rate_ << " samples per second.";
      return false;
    }

    InitEncoder();
  }

  return encoder_ != NULL;
}

void AudioEncoderOpus::FetchBytesToResample(int resampler_frame_delay,
                                            media::AudioBus* audio_bus) {
  DCHECK(resampling_data_);
  int samples_left = (resampling_data_size_ - resampling_data_pos_) /
      kBytesPerSample / channels_;
  DCHECK_LE(audio_bus->frames(), samples_left);
  audio_bus->FromInterleaved(
      resampling_data_ + resampling_data_pos_,
      audio_bus->frames(), kBytesPerSample);
  resampling_data_pos_ += audio_bus->frames() * kBytesPerSample * channels_;
  DCHECK_LE(resampling_data_pos_, static_cast<int>(resampling_data_size_));
}

scoped_ptr<AudioPacket> AudioEncoderOpus::Encode(
    scoped_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_EQ(1, packet->data_size());
  DCHECK_EQ(kBytesPerSample, packet->bytes_per_sample());

  if (!ResetForPacket(packet.get())) {
    LOG(ERROR) << "Encoder initialization failed";
    return scoped_ptr<AudioPacket>();
  }

  int samples_in_packet = packet->data(0).size() / kBytesPerSample / channels_;
  const int16* next_sample =
      reinterpret_cast<const int16*>(packet->data(0).data());

  // Create a new packet of encoded data.
  scoped_ptr<AudioPacket> encoded_packet(new AudioPacket());
  encoded_packet->set_encoding(AudioPacket::ENCODING_OPUS);
  encoded_packet->set_sampling_rate(kOpusSamplingRate);
  encoded_packet->set_channels(channels_);

  int prefetch_samples =
      resampler_.get() ? media::SincResampler::kMaximumLookAheadSize : 0;
  int samples_wanted = frame_size_ + prefetch_samples;

  while (leftover_samples_ + samples_in_packet >= samples_wanted) {
    const int16* pcm_buffer = NULL;

    // Combine the packet with the leftover samples, if any.
    if (leftover_samples_ > 0) {
      pcm_buffer = leftover_buffer_.get();
      int samples_to_copy = samples_wanted - leftover_samples_;
      memcpy(leftover_buffer_.get() + leftover_samples_ * channels_,
             next_sample, samples_to_copy * kBytesPerSample * channels_);
    } else {
      pcm_buffer = next_sample;
    }

    // Resample data if necessary.
    int samples_consumed = 0;
    if (resampler_.get()) {
      resampling_data_ = reinterpret_cast<const char*>(pcm_buffer);
      resampling_data_pos_ = 0;
      resampling_data_size_ = samples_wanted * channels_ * kBytesPerSample;
      resampler_->Resample(resampler_bus_.get(), kFrameSamples);
      resampling_data_ = NULL;
      samples_consumed = resampling_data_pos_ / channels_ / kBytesPerSample;

      resampler_bus_->ToInterleaved(kFrameSamples, kBytesPerSample,
                                    resample_buffer_.get());
      pcm_buffer = reinterpret_cast<int16*>(resample_buffer_.get());
    } else {
      samples_consumed = frame_size_;
    }

    // Initialize output buffer.
    std::string* data = encoded_packet->add_data();
    data->resize(kFrameSamples * kBytesPerSample * channels_);

    // Encode.
    unsigned char* buffer =
        reinterpret_cast<unsigned char*>(string_as_array(data));
    int result = opus_encode(encoder_, pcm_buffer, kFrameSamples,
                             buffer, data->length());
    if (result < 0) {
      LOG(ERROR) << "opus_encode() failed with error code: " << result;
      return scoped_ptr<AudioPacket>();
    }

    DCHECK_LE(result, static_cast<int>(data->length()));
    data->resize(result);

    // Cleanup leftover buffer.
    if (samples_consumed >= leftover_samples_) {
      samples_consumed -= leftover_samples_;
      leftover_samples_ = 0;
      next_sample += samples_consumed * channels_;
      samples_in_packet -= samples_consumed;
    } else {
      leftover_samples_ -= samples_consumed;
      memmove(leftover_buffer_.get(),
              leftover_buffer_.get() + samples_consumed * channels_,
              leftover_samples_ * channels_ * kBytesPerSample);
    }
  }

  // Store the leftover samples.
  if (samples_in_packet > 0) {
    DCHECK_LE(leftover_samples_ + samples_in_packet, leftover_buffer_size_);
    memmove(leftover_buffer_.get() + leftover_samples_ * channels_,
            next_sample, samples_in_packet * kBytesPerSample * channels_);
    leftover_samples_ += samples_in_packet;
  }

  // Return NULL if there's nothing in the packet.
  if (encoded_packet->data_size() == 0)
    return scoped_ptr<AudioPacket>();

  return encoded_packet.Pass();
}

}  // namespace remoting
