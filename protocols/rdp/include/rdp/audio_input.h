/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef GUAC_RDP_AUDIO_INPUT_H
#define GUAC_RDP_AUDIO_INPUT_H

#include <guacamole/config.h>
#include <rdp/dvc.h>

#include <freerdp/freerdp.h>
#include <guacamole/stream.h>
#include <guacamole/user.h>

#ifdef HAVE_BOOST
#include <boost/thread/mutex.hpp>
#elif defined HAVE_LIBPTHREAD
#include <pthread.h>
#endif

/**
 * Handler which is invoked when a guac_rdp_audio_buffer's internal packet
 * buffer has reached capacity and must be flushed.
 *
 * @param buffer
 *     The buffer which needs to be flushed as an audio packet.
 *
 * @param length
 *     The number of bytes stored within the buffer. This is guaranteed to be
 *     identical to the packet_size value specified when the audio buffer was
 *     initialized.
 *
 * @param data
 *     The arbitrary data pointer provided when the audio buffer was
 *     initialized.
 */
typedef void guac_rdp_audio_buffer_flush_handler(char* buffer, int length,
        void* data);

/**
 * A description of an arbitrary PCM audio format.
 */
typedef struct guac_rdp_audio_format {

    /**
     * The rate of the audio data in samples per second.
     */
    int rate;

    /**
     * The number of channels included in the audio data. This will be 1 for
     * monaural audio and 2 for stereo.
     */
    int channels;

    /**
     * The size of each sample within the audio data, in bytes.
     */
    int bps;

} guac_rdp_audio_format;

/**
 * A buffer of arbitrary audio data. Received audio data can be written to this
 * buffer, and will automatically be flushed via a given handler once the
 * internal buffer reaches capacity.
 */
typedef struct guac_rdp_audio_buffer {

    /**
     * Lock which is acquired/released to ensure accesses to the audio buffer
     * are atomic.
     */
#ifdef HAVE_BOOST
	boost::mutex lock;
#elif defined HAVE_LIBPTHREAD
    pthread_mutex_t lock;
#endif

    /**
     * The user from which this audio buffer will receive data. If no user has
     * yet opened an associated audio stream, this will be NULL.
     */
    guac_user* user;

    /**
     * The stream from which this audio buffer will receive data. If no user
     * has yet opened an associated audio stream, this will be NULL.
     */
    guac_stream* stream;

    /**
     * The PCM format of the audio stream being received from the user, if any.
     * If no stream is yet associated, the values stored within this format are
     * undefined.
     */
    guac_rdp_audio_format in_format;

    /**
     * The PCM format of the audio stream expected by RDP, if any. If no audio
     * stream has yet been requested by the RDP server, the values stored
     * within this format are undefined.
     */
    guac_rdp_audio_format out_format;

    /**
     * The size that each audio packet must be, in bytes. The packet buffer
     * within this structure will be at least this size.
     */
    int packet_size;

    /**
     * The number of bytes currently stored within the packet buffer.
     */
    int bytes_written;

    /**
     * The total number of bytes having ever been received by the Guacamole
     * server for the current audio stream.
     */
    int total_bytes_received;

    /**
     * The total number of bytes having ever been sent to the RDP server for
     * the current audio stream.
     */
    int total_bytes_sent;

    /**
     * All audio data being prepared for sending to the AUDIO_INPUT channel.
     */
    char* packet;

    /**
     * Handler function which will be invoked when a full audio packet is
     * ready to be flushed to the AUDIO_INPUT channel, if defined. If NULL,
     * audio packets will simply be ignored.
     */
    guac_rdp_audio_buffer_flush_handler* flush_handler;

    /**
     * Arbitrary data assigned by the AUDIO_INPUT plugin implementation.
     */
    void* data;

} guac_rdp_audio_buffer;

/**
 * Allocates a new audio buffer. The new audio buffer will ignore any received
 * data until guac_rdp_audio_buffer_begin() is invoked, and will resume
 * ignoring received data once guac_rdp_audio_buffer_end() is invoked.
 *
 * @return
 *     A newly-allocated audio buffer.
 */
guac_rdp_audio_buffer* guac_rdp_audio_buffer_alloc();

/**
 * Associates the given audio buffer with the underlying audio stream which
 * has been received from the given Guacamole user. Once both the Guacamole
 * audio stream and the RDP audio stream are ready, an appropriate "ack"
 * message will be sent.
 *
 * @param audio_buffer
 *     The audio buffer associated with the audio stream just received.
 *
 * @param user
 *     The Guacamole user that created the audio stream.
 *
 * @param stream
 *     The guac_stream object representing the audio stream.
 *
 * @param rate
 *     The rate of the audio stream being received from the user, if any, in
 *     samples per second.
 *
 * @param channels
 *     The number of channels included in the audio stream being received from
 *     the user, if any.
 *
 * @param bps
 *     The size of each sample within the audio stream being received from the
 *     user, if any, in bytes.
 */
void guac_rdp_audio_buffer_set_stream(guac_rdp_audio_buffer* audio_buffer,
        guac_user* user, guac_stream* stream, int rate, int channels, int bps);

/**
 * Defines the output format that should be used by the audio buffer when
 * flushing packets of audio data received via guac_rdp_audio_buffer_write().
 * As this format determines how the underlying packet buffer will be
 * allocated, this function MUST be called prior to the call to
 * guac_rdp_audio_buffer_begin().
 *
 * @param audio_buffer
 *     The audio buffer to set the output format of.
 *
 * @param rate
 *     The rate of the audio stream expected by RDP, in samples per second.
 *
 * @param channels
 *     The number of channels included in the audio stream expected by RDP.
 *
 * @param bps
 *     The size of each sample within the audio stream expected by RDP, in
 *     bytes.
 */
void guac_rdp_audio_buffer_set_output(guac_rdp_audio_buffer* audio_buffer,
        int rate, int channels, int bps);

/**
 * Begins handling of audio data received via guac_rdp_audio_buffer_write() and
 * allocates the necessary underlying packet buffer. Audio packets having
 * exactly packet_frames frames will be flushed as available using the provided
 * flush_handler. An audio frame is a set of single samples, one sample per
 * channel. The guac_rdp_audio_buffer_set_output() function MUST have
 * been invoked first.
 *
 * @param audio_buffer
 *     The audio buffer to begin.
 *
 * @param packet_frames
 *     The exact number of frames (a set of samples, one for each channel)
 *     which MUST be included in all audio packets provided to the
 *     given flush_handler.
 *
 * @param flush_handler
 *     The function to invoke when an audio packet must be flushed.
 *
 * @param data
 *     Arbitrary data to provide to the flush_handler when an audio packet
 *     needs to be flushed.
 */
void guac_rdp_audio_buffer_begin(guac_rdp_audio_buffer* audio_buffer,
        int packet_frames, guac_rdp_audio_buffer_flush_handler* flush_handler,
        void* data);

/**
 * Writes the given buffer of audio data to the given audio buffer. A new
 * packet will be flushed using the associated flush handler once sufficient
 * bytes have been accumulated.
 *
 * @param audio_buffer
 *     The audio buffer to which the given audio data should be written.
 *
 * @param buffer
 *     The buffer of audio data to write to the given audio buffer.
 *
 * @param length
 *     The number of bytes to write.
 */
void guac_rdp_audio_buffer_write(guac_rdp_audio_buffer* audio_buffer,
        char* buffer, int length);

/**
 * Stops handling of audio data received via guac_rdp_audio_buffer_write() and
 * frees the underlying packet buffer. Further audio data will be ignored until
 * guac_rdp_audio_buffer_begin() is invoked again.
 *
 * @param audio_buffer
 *     The audio buffer to end.
 */
void guac_rdp_audio_buffer_end(guac_rdp_audio_buffer* audio_buffer);

/**
 * Frees the given audio buffer. If guac_rdp_audio_buffer_end() has not yet
 * been called, its associated packet buffer will also be freed.
 *
 * @param audio_buffer
 *     The audio buffer to free.
 */
void guac_rdp_audio_buffer_free(guac_rdp_audio_buffer* audio_buffer);

/**
 * Handler for inbound audio data (audio input).
 */
guac_user_audio_handler guac_rdp_audio_handler;

/**
 * Handler for stream data related to audio input.
 */
guac_user_blob_handler guac_rdp_audio_blob_handler;

/**
 * Handler for end-of-stream related to audio input.
 */
guac_user_end_handler guac_rdp_audio_end_handler;

/**
 * Adds Guacamole's "guacai" plugin to the list of dynamic virtual channel
 * plugins to be loaded by FreeRDP's "drdynvc" plugin. The plugin will only
 * be loaded once guac_rdp_load_drdynvc() is invoked with the guac_rdp_dvc_list
 * passed to this function. The "guacai" plugin ultimately adds support for the
 * "AUDIO_INPUT"  dynamic virtual channel.
 *
 * @param context
 *     The rdpContext associated with the active RDP session.
 *
 * @param list
 *     The guac_rdp_dvc_list to which the "guacai" plugin should be added, such
 *     that it may later be loaded by guac_rdp_load_drdynvc().
 */
void guac_rdp_audio_load_plugin(rdpContext* context, guac_rdp_dvc_list* list);

#endif

