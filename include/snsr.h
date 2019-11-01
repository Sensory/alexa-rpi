/*
 * Copyright (c) Sensory, Inc. 2000-2019.  All Rights Reserved.
 * http://sensory.com/
 *
 * You may not use these files except in compliance with the license.
 * A copy of the license is located the "LICENSE.txt" file accompanying
 * this source. This file is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *---------------------------------------------------------------------------
 * TrulyHandsfree SDK
 *---------------------------------------------------------------------------
 */

/**
 * @addtogroup c C API
 * TrulyHandsfree C SDK.
 *
 * This library uses a [dataflow][] approach to evaluate speech
 * recognition tasks.
 * It relies on [inversion of control][ioc]:
 * User-defined hooks are invoked to report results and control task flow.
 * Task models define flow pipelines and behavior.
 * The API contains two primary data types: @ref session and @ref stream.
 * Sessions encapsulate the entire state of a task instance, and all
 * session input and output data are are vectored through streams.
 *
 * [dataflow]: https://en.wikipedia.org/wiki/Flow-based_programming
 * [ioc]: https://en.wikipedia.org/wiki/Inversion_of_control
 *
 * Session instance behavior is customized via settings, of which there
 * are three cariants:
 * - **Configuration** settings are serializable read-write values.
 *   These are used for parameter values and model objects.
 *   Task models contain reasonable default values - modify these only to
 *   tweak results.
 * - **Runtime** settings are instance-specific and not serializable. These
 *   include callback handles, iterators, input streams and values
 *   such as the name of the phrase being enrolled.
 * - **Result** settings are read-only values with very limited scope, such
 *   as the recognition results.
 *
 * Stream instances are created on various underlying data streams, such as
 * @c FILE @c * handles, memory segments, and live audio sources. Only the
 * constructors are type-specific, all other functions are independent of the
 * underlying stream type. Operations that take stream handles as arguments
 * operate on any of the types.
 *
 * Task evaluation typically follows this recipe:
 * - Create a new session instance.
 * - Load a task model into the instance.
 * - Set the input source stream.
 * - Register one or more event handlers.
 * - Enter the main loop. The library will process the input streams and
 *   invoke event handlers at appropriate times. The main loop continues
 *   until a terminating condition is reached, such as an event returning
 *   an error code.
 * - Release the sesion instance.
 * @{
 * @file snsr.h
 * TrulyHandsfree SDK C API
 */

#ifndef SNSR_H
#define SNSR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdarg.h>
#ifndef _WIN32
# include <unistd.h>
#endif

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
# include <stdint.h>
#endif


/*---------------------------------------------------------------------------*/
/**
 * @addtogroup libsettings Library Settings
 * @{
 * These settings return information about the library.
 * Available in all contexts, even before a task model has been loaded.
 */

/**
 * Type of acceleration used.
 * Returns text describing the type of vector acceleration the library uses.
 * @see snsrGetString(), #SNSR_LIBRARY_INFO.
 */
#define SNSR_ACCELERATION "accel-info"

/**
 * License expiration warning message. Use with snsrGetString().
 * This value is `NULL` for library license keys that do not expire,
 * or if the expiration date is more than 60 days into the future.
 * For license keys expiring in 60 days or fewer, the returned string
 * will be of the form `"License will expire in 37 days."`.
 */
#define SNSR_LICENSE_WARNING "license-exp-warn"

/**
 * License expiration date.
 * Returns the license expiration date in seconds since the epoch.
 * For production keys, which do not expire, the expiration date is @c 0.
 * Example:
 * @code
 * double e;
 * time_t expdate;
 * snsrGetDouble(s, SNSR_LICENSE_EXPDATE, &e);
 * expdate = (time_t)e;
 * @endcode
 * @see snsrGetDouble().
 */
#define SNSR_LICENSE_EXPDATE "license-exp-date"

/**
 * License expiration message.
 * The returned string is of the form `"License expires on <date>"`, or
 * `NULL` for license keys that do not expire.
 * Example:
 * @code
 * const char *expires;
 * snsrGetString(s, SNSR_LICENSE_EXPIRES, &expires);
 * if (expires) fprintf(stderr, "%s\n", expires);
 * @endcode
 * @see snsrGetString().
 */
#define SNSR_LICENSE_EXPIRES "license-exp-message"

/**
 * SDK library information.
 * Convenient summary of #SNSR_NAME, #SNSR_VERSION,
 * #SNSR_VERSION_DSP, #SNSR_ACCELERATION,
 * #SNSR_LICENSE_EXPIRES and #SNSR_LICENSE_WARNING.
 * @note The session handle must be @c NULL.
 *
 * Example:
 * @code
 * const char *libInfo;
 * snsrGetString(NULL, SNSR_LIBRARY_INFO, &libInfo);
 * fprintf(stderr, "%s\n", libInfo);
 * snsrRelease(libInfo);
 * @endcode
 */
#define SNSR_LIBRARY_INFO "library-info"

/**
 * Recognition pipeline behavior.
 * This boolean value controls whether snsrRun() flushes the recognition
 * pipeline when one (or more) of the input streams report an end-of-file
 * condition.
 *
 * The default value is @c 1, which enables automatic flushing on #SNSR_RC_EOF.
 * This is appropriate for most applications.
 * Set #SNSR_AUTO_FLUSH to @c 0 when audio is presented to snsrRun() in small
 * segments.
 *
 * @see snsrRun().
 */
#define SNSR_AUTO_FLUSH "auto-flush"

/**
 * Prepares a SnsrSession for generating custom initialization code.
 * Set this value to @c NULL to enable snsrLoad() instrumentation. This is used
 * by snsrSave() with #SNSR_FM_SUBSET_INIT to generate custom library
 * initialization code (in C), which references only modules encountered
 * during snsrLoad().
 *
 * Linking against only a subset of available modules reduces executable size.
 *
 * This example will create a @a custom-init.c file. Add this to the build, and
 * recompile with -DSNSR_USE_SUBSET to enable. The linked executable(s) will
 * contain just the modules required to run @a model-a.snsr and @a model-b.snsr.
 *
 * Example:
 * @code
 * SnsrSession s;
 * snsrNew(&s);
 * snsrSetString(s, SNSR_PREPARE_SUBSET_INIT, NULL);
 * snsrLoad(s, snsrStreamFromFileName("model-a.snsr", "r"));
 * snsrLoad(s, snsrStreamFromFileName("model-b.snsr", "r"));
 * snsrSave(s, SNSR_FM_SUBSET_INIT,
 *          snsrStreamFromFileName("custom-init.c", "w"));
 * snsrRelease(s);
 * @endcode
 *
 * @see snsrSave(), snsrSetString(), snsrLoad(), #SNSR_FM_SUBSET_INIT.
 */
#define SNSR_PREPARE_SUBSET_INIT "model:ids"


/** @} libsettings */
/*---------------------------------------------------------------------------
 * Phrase spotter
 */
/* Task type */

/**
 * @addtogroup task Task Settings
 * These settings are task-specific, and valid only after a task has
 * been loaded into as session.
 * @{
 */

/**
 * Task type key.
 * @see snsrGetString(), snsrRequire().
 */
#define SNSR_TASK_TYPE "task-type"

/**
 * Task name key.
 * @see snsrGetString()
 */
#define SNSR_TASK_NAME "task-name"

/**
 * Task version key.
 * These strings follow [semantic versioning](http://semver.org/) rules.
 * @see snsrGetString(), snsrRequire().
 */
#define SNSR_TASK_VERSION "task-version"

/**
 * Exported identifier in ROM C code.
 * When text segment C code is created with snsrSave() and #SNSR_FM_SOURCE, this
 * setting specifies the name of the exported C data structure.
 *
 * The value must start with an ASCII alphabetic character or _, and contain
 * only alphanumerics and _.
 */
#define SNSR_TAG_IDENTIFIER "tag-identifier"

/**
 * C code source model name.
 * When ROM-able C code is created with snsrSave() and #SNSR_FM_SOURCE, this
 * setting specifies the name of the model file used to create the code.
 *
 * This value is included in the comment header of the generated code, for
 * information only.
 */
#define SNSR_MODEL_NAME "model-name"


/**
 * A list of task names and versions.
 * When used with snsrRequire(), the value argument must be
 * a semicolon-separated list of #SNSR_TASK_NAME and
 * #SNSR_TASK_VERSION values. This list must have at least one element.
 *
 * A task will match the requirement if one of the #SNSR_TASK_NAME fields
 * match, and the corresponding #SNSR_TASK_VERSION is satisfied.
 *
 * If no #SNSR_TASK_NAME matches, snsrRequire() returns
 * #SNSR_RC_REQUIRE_MISMATCH. If a #SNSR_TASK_NAME matches, but the
 * associated #SNSR_TASK_VERSION is not satisfied, snsrRequire() returns
 * #SNSR_RC_VERSION_MISMATCH.
 *
 * Example:
 * @if (JAVA || ANDROID)
 * @code
 * session.require(Snsr.TASK_TYPE_AND_VERSION_LIST,
 *                 Snsr.PHRASESPOT + " ~0.5.0 || 1.0.0;" +
 *                 Snsr.LVCSR + " 1.0.0");
 * @endcode
 * @else
 * @code
 * snsrRequire(session, SNSR_TASK_TYPE_AND_VERSION_LIST,
 *             SNSR_PHRASESPOT " ~0.5.0 || 1.0.0;"
 *             SNSR_LVCSR " 1.0.0");
 * @endcode
 * @endif
 *
 * @see snsrRequire().
 */
#define SNSR_TASK_TYPE_AND_VERSION_LIST "task-type-and-version-list"

/**
 * @addtogroup task-spot Phrase Spotter
 * See @ref task-phrasespot.
 * @{
 */

/**
 * Phrase spotter task type.
 * @see snsrRequire().
 */
#define SNSR_PHRASESPOT "phrasespot"

/**
 * LVCSR recognizer task type.
 * @tnl
 * @see snsrRequire().
 */
#define SNSR_LVCSR "lvcsr"

/**
 * Phrase spot delay in ms.
 * Delays the phrase spot result while searching for a better match.
 * @see snsrSetInt().
 */
#define SNSR_SPOT_DELAY "delay"

/**
 * Phrase spot listening window in seconds.
 * Listening window the phrase spotter is tuned for.
 * `0` for continuous/unbounded listening.
 * @note Information only, does not affect SDK phrase spotter behaviour.
 * Does, however, affect data generated for embedded use.
 * @see snsrGetInt().
 */
#define SNSR_LISTEN_WINDOW "listen-window"

/**
 * Embedded device target name.
 * @see snsrSetString().
 */
#define SNSR_EMBEDDED_TARGET "dsp-target"

/**
 * Embedded device search header.
 * @see snsrGetStream().
 */
#define SNSR_EMBEDDED_HEADER_STREAM "dsp-header-stream"

/**
 * Embedded device search model.
 * @see snsrGetStream().
 */
#define SNSR_EMBEDDED_SEARCH_STREAM "dsp-search-stream"

/**
 * Embedded device acoustic model.
 * @see snsrGetStream().
 */
#define SNSR_EMBEDDED_ACMODEL_STREAM "dsp-acmodel-stream"

/**
 * Spotter operating point.
 * Selects the trade-off between false accept and false reject errors for
 * fixed-phrase spotters. The operating point was known as the search index
 * in earlier versions of the TrulyHandsfree SDK.
 * - Range is @c 1 to @c 21 inclusive.
 * - Lower-numbered points have a lower false accept rate at the expense
 *   of higher false reject fraction.
 * - The false accept rate is expressed as the expected number of false accepts
 *   (where the recognizer mistakenly spots the trigger phrase) per time unit.
 * - The false reject rate is the percentage of times the actual trigger phrase
 *   is spoken, but not recognized.
 * - The default operating point is selected by Sensory during trigger
 *   development for a good balance between the these two error types.
 * - Note that not all operating points are necessarily valid. Use
 *   #SNSR_OPERATING_POINT_LIST to iterate over the available points.
 * @see snsrGetInt(), snsrSetInt(), #SNSR_OPERATING_POINT_LIST.
 */
#define SNSR_OPERATING_POINT "operating-point"

/**
 * Low false-reject spotter operating point.
 * Selects the low false-reject fall-back operating point used by
 * the @ref tpl-spot-dynop template. This low false-reject operating
 * point is selected (for a limited time) if a spot was barely rejected.
 * @see snsrGetInt(), snsrSetInt(), #SNSR_DURATION_MS, #SNSR_OPERATING_POINT,
 * #SNSR_THRESHOLD.
 */
#define SNSR_LOW_FR_OPERATING_POINT "low-fr-operating-point"

/**
 * Dynamic operating point selection threshold.
 * Selects the threshold used by @ref tpl-spot-dynop to decide whether
 * to select the @const{LOW_FR_OPERATING_POINT}.
 * @see snsrGetDouble(), snsrSetDouble(), #SNSR_OPERATING_POINT,
 * #SNSR_LOW_FR_OPERATING_POINT, #SNSR_DURATION_MS.
 */
#define SNSR_THRESHOLD "threshold"

/**
 * Low false-reject listening window.
 * Selects the time window (in ms) that @ref tpl-spot-dynop will use
 * the @const{LOW_FR_OPERATING_POINT}.
 *
 * @see snsrGetDouble(), snsrSetDouble(), #SNSR_OPERATING_POINT,
 * #SNSR_LOW_FR_OPERATING_POINT, #SNSR_THRESHOLD.
 */
#define SNSR_DURATION_MS "duration-ms"


/**
 * Iterate over available spotter operating points.
 * @see snsrForEach(), #SNSR_RES_AVAILABLE_POINT.
 */
#define SNSR_OPERATING_POINT_LIST "operating-point-iterator"

/**
 * Available operating point.
 * This read-only value is valid during #SNSR_OPERATING_POINT_LIST
 * iteration callbacks only.
 * @see snsrGetInt(), #SNSR_OPERATING_POINT_LIST, #SNSR_OPERATING_POINT.
 */
#define SNSR_RES_AVAILABLE_POINT "available-point"

/**
 * Sample rate in Hz.
 * Read-only.
 * @see snsrGetInt().
 */
#define SNSR_SAMPLE_RATE "samples-per-second"

/**
 * Input audio buffer size.
 * The number of audio samples kept in a circular audio history buffer,
 * accessible through #SNSR_AUDIO_STREAM.
 *
 * This buffer can be used to retrieve segmented audio using alignments
 * (#SNSR_RES_BEGIN_SAMPLE, #SNSR_RES_END_SAMPLE) obtained in the
 * #SNSR_RESULT_EVENT callback.
 *
 * Set to `0` to disable audio buffering.
 * @see snsrGetInt(), snsrSetInt().
 */
#define SNSR_AUDIO_STREAM_SIZE "audio-stream-size"

/**
 * Audio buffer start sample index.
 * The index of the first (oldest) audio sample contained in the
 * #SNSR_AUDIO_STREAM.
 * @see snsrGetDouble(), snsrSetDouble().
 */
#define SNSR_AUDIO_STREAM_FIRST "audio-stream-first"

/**
 * Audio buffer end sample index.
 * The index of the last (most recent) audio sample contained in the
 * #SNSR_AUDIO_STREAM.
 * @see snsrGetDouble(), snsrSetDouble().
 */
#define SNSR_AUDIO_STREAM_LAST "audio-stream-last"

/**
 * Audio stream requested start index.
 * Start the next #SNSR_AUDIO_STREAM at this sample index value.
 * Defaults to #SNSR_AUDIO_STREAM_FIRST.
 * @see snsrGetDouble(), snsrSetDouble().
 */
#define SNSR_AUDIO_STREAM_FROM "audio-stream-from"

/**
 * Audio stream requested end index.
 * End the next #SNSR_AUDIO_STREAM at this sample index value.
 * Defaults to #SNSR_AUDIO_STREAM_LAST.
 * @see snsrGetDouble(), snsrSetDouble().
 */
#define SNSR_AUDIO_STREAM_TO "audio-stream-to"

/**
 * Source audio stream.
 * This audio stream must be headerless, 16-bit PCM encoded and sampled
 * at 16 kHz.
 * @see snsrSetStream().
 */
#define SNSR_SOURCE_AUDIO_PCM "->audio-pcm"

/**
 * Sequential task has started listening on second slot.
 * Raised in a sequential task when audio focust has shifted from the
 * first slot to the second. This typically happens when the spotter
 * in the first slot has detected the phrase.
 *
 * This event is ignored for tasks that do not feature sequential behavior.
 *
 * @see snsrSetHandler(), #SNSR_LISTEN_END_EVENT
 */
#define SNSR_LISTEN_BEGIN_EVENT "^listen-begin"

/**
 * Sequential task has stopped listening on second slot.
 * Raised in a sequential task when audio focus has shifted from the second slot
 * back to the first.
 *
 * This event is ignored for tasks that do not feature sequential behavior.
 *
 * @see snsrSetHandler(), #SNSR_LISTEN_BEGIN_EVENT, #SNSR_LISTEN_WINDOW
 */
#define SNSR_LISTEN_END_EVENT "^listen-end"


/**
 * Include silence in recognizer results.
 * When set to @c 1, word-pause @c &lt;wp&gt;, sentence-begin @c &lt;s&gt;, and
 * sentence-end @c &lt;s/&gt; markup is included in LVCSR recognizer results.
 *
 * @tnl
 * @see snsrSetInt(), #SNSR_RESULT_EVENT, #SNSR_PARTIAL_RESULT_EVENT
 */
#define SNSR_SHOW_SILENCE "show-silence"

/**
 * Out-of-vocabulary rejection sensitivity.
 * @if TNL
 * This setting controls out-of-vocabulary rejection for @ref build.
 * @else
 * This setting controls out-of-vocabulary rejection in custom
 * LVCSR recognizers.
 * @endif
 *
 * Custom LVCSR recognizers report @c &lt;no-match/&gt; for words or phrases
 * that are not in the grammar. With a #SNSR_OOV_REJECT value of @c 0 the
 * recognizer will never report &lt;no-match/&gt;, it will return the closest
 * match instead. With #SNSR_OOV_REJECT at @c 1.0, almost all input will return
 * &lt;no-match/&gt;.
 *
 * The optimal value for #SNSR_OOV_REJECT depends on the vocabulary used. A
 * reasonable value to start testing with is @c 0.5.
 *
 * Do not change #SNSR_OOV_REJECT for regular LVCSR models (those
 * without custom vocabulary.)
 *
 * @tnl
 * @see snsrSetDouble()
 */
#define SNSR_OOV_REJECT "search.frame-nota"

/**
 * Partial result update interval.
 * The current preliminary result is emitted every @p partial-result-interval
 * miliseconds. Set to @c 0 to disable partial result reporting.

 * @tnl
 * @see snsrSetDouble(), #SNSR_PARTIAL_RESULT_EVENT
 */
#define SNSR_PARTIAL_RESULT_INTERVAL "partial-result-interval"

/**
 * Partial result available event.
 *
 * Raised when a preliminary recognition result is available.
 * @tnl
 */
#define SNSR_PARTIAL_RESULT_EVENT "^result-partial"

/**
 * Recognition grammar stream.
 * Creates a recognizer from a grammar specification read from a stream.
 * The new model will recognize only those phrases that the
 * grammar generates.
 *
 * The model will be ready to recognize once
 * @if C
 * snsrSetStream(session, grammarStream)
 * @endif
 * @if (ANDROID || JAVA)
 * SnsrSession.set(Snsr.GRAMMAR, SnsrStream)
 * @endif
 * returns.
 * For larger grammars the build process can take a significant amount of time.
 *
 * @if TNL
 * See @ref grammar-syntax for detail on how grammars are structured.
 * @endif
 *
 * @tnl
 * @note Requires a model that supports building.
 * @see snsrSetStream(), #SNSR_PHRASES_STREAM.
 */
#define SNSR_GRAMMAR_STREAM "grammar-stream"

/**
 * Recognition phrase list stream.
 * Creates a recognizer from a list of phrases read from a stream.
 * Individual phrases are separated by newlines or semicolons.
 * Comments start with @c # and run until the end of the phrase.
 * Only the exact phrases in the list will be part of the recognition language.
 *
 * This untility setting converts the list of phrases into this
 * grammar specfication:
 * @code
 * g = <s> ( phrase0 | phrase1 | ... | phraseN) </s>;
 * @endcode
 * @tnl
 * @note Requires a model that supports building.
 * @see snsrSetStream(), #SNSR_GRAMMAR_STREAM.
 */
#define SNSR_PHRASES_STREAM "phrases-stream"

/**
 * Result available event.
 * Raised when a recognition result is available.
 * For enrolled phrase spotters, this event is only raised if the speaker
 * verification score is above threshold.
 */
#define SNSR_RESULT_EVENT "^result"

/**
 * Samples available event.
 * Raised whenever audio samples have been read from the input stream and
 * are about to be processed.
 * @see snsrSetHandler().
 */
#define SNSR_SAMPLES_EVENT "^sample-count"

/**
 * Number of samples read from the input stream.
 * @see snsrGetDouble(), #SNSR_SAMPLES_EVENT
 */
#define SNSR_RES_SAMPLES "sample-count"


/* Valid in an SNSR_RESULT_EVENT callback only   */
/**
 * Timestamp of the audio start point.
 * The offset in ms from the beginning of the audio stream where:
 * - the recognition unit started (in #SNSR_RESULT_EVENT), or
 * - the Voice Activity Detector first detected speech
 *   (in #SNSR_BEGIN_EVENT, #SNSR_END_EVENT, #SNSR_LIMIT_EVENT).
 * @see snsrGetDouble().
 */
#define SNSR_RES_BEGIN_MS "begin-ms"

/**
 * Sample index of the audio start point.
 * The offset in samples from the beginning of the audio stream where:
 * - the recognition unit started, (in #SNSR_RESULT_EVENT) or
 * - the Voice Activity Detector first detected speech.
 *   (in #SNSR_BEGIN_EVENT, #SNSR_END_EVENT, #SNSR_LIMIT_EVENT).
 * @see snsrGetDouble().
 */
#define SNSR_RES_BEGIN_SAMPLE "begin-sample"

/**
 * Timestamp of the audio endpoint.
 * The offset in ms from the beginning of the audio stream:
 * - where the recognition unit ended (in #SNSR_RESULT_EVENT), or
 * - the Voice Activity Detector last detected speech.
 *   (in #SNSR_END_EVENT, #SNSR_LIMIT_EVENT).
 * @see snsrGetDouble().
 */
#define SNSR_RES_END_MS "end-ms"

/**
 * Sample index of the audio endpoint.
 * The offset in samples from the beginning of the audio stream:
 * - where the recognition unit ended (in #SNSR_RESULT_EVENT), or
 * - the Voice Activity Detector last detected speech.
 *   (in #SNSR_END_EVENT, #SNSR_LIMIT_EVENT).
 * @see snsrGetDouble().
 */
#define SNSR_RES_END_SAMPLE "end-sample"


/**
 * Fixed-phrase confidence score.
 * The probability of the spotted phrase being a true accept.
 * This is a model-dependent optional feature not universally supported. It
 * is not supported by enrolled models (use #SNSR_RES_SV_SCORE instead.)
 *
 * @note This value is defined at the #SNSR_PHRASE_LIST level only.
 *
 * The reported range is @c 0 to @c 1, or @c < @c 0
 * if not supported by the spotter model.
 * @see snsrGetDouble()
 */
#define SNSR_RES_CONF_SCORE "confidence-score"

/**
 * Speaker verification score.
 * The confidence that the spotted phrase was spoken by the enrolled speaker, in
 * the range @c 0 to @c 1. For non-enrolled spotters the confidence
 * is always @c 1.0.
 * @see snsrGetDouble(), #SNSR_SV_THRESHOLD
 */
#define SNSR_RES_SV_SCORE "sv-score"

/**
 * Speaker verification threshold.
 * Phrase spot results with a #SNSR_RES_SV_SCORE less than this threshold are
 * not reported (the #SNSR_RESULT_EVENT handler will not be invoked.)
 * @see snsrGetDouble(), snsrSetDouble(), #SNSR_RES_SV_SCORE
 */
#define SNSR_SV_THRESHOLD "sv-threshold"

/**
 * Continuous Adaptation cache file name.
 * When set, enrolled user data will be saved to, and loaded from
 * this file. If not set, enrolled user data are discarded when
 * the spotter session is released.
 *
 * This setting is only available in fixed-phrase spotters that
 * support continuous adaptation.
 *
 * @note Continuous Adaptation of fixed phrase spotters
 * is in the alpa development stage.
 * Please @ref contact "contact Sensory" for details.
 *
 * @see snsrSetString(), snsrGetString().
 */
#define SNSR_CACHE_FILE "cache-file"

/**
 * Signal energy.
 * The energy (in dB relative to @c 1) in the spotted phrase.
 *
 * @see snsrGetDouble(), #SNSR_RES_NOISE_ENERGY, #SNSR_RES_SNR
 */
#define SNSR_RES_SIGNAL_ENERGY "signal-energy"

/**
 * Noise energy.
 * The energy (in dB relative to @c 1) in the background audio preceding
 * the spotted phrase.
 *
 * @see snsrGetDouble(), #SNSR_RES_SIGNAL_ENERGY, #SNSR_RES_SNR
 */
#define SNSR_RES_NOISE_ENERGY "noise-energy"

/**
 * Signal to noise ratio.
 * The ratio of the signal energy to the noise energy, in dB.
 *
 * @see snsrGetDouble(), #SNSR_RES_NOISE_ENERGY, #SNSR_RES_SIGNAL_ENERGY
 */
#define SNSR_RES_SNR "snr"


/**
 * **RESERVED**. Do not use.
 * @see snsrSetDouble().
 */
#define SNSR_SCORE_OFFSET "score-offset"

/**
 * Recognition text result.
 * The phrase, word, or phoneme result of a phrase spot.
 * @see snsrGetString()
 */
#define SNSR_RES_TEXT "text"


/**
 * Recognition ID result.
 * Unique phrase result ID.
 * For most single-phrase spotters this will be @c 1.
 * @see snsrGetInt(), #SNSR_RESULT_EVENT, #SNSR_VOCAB_LIST.
 */
#define SNSR_RES_ID "id"

/**
 * Iterate over phrase results.
 * For phrase spotters there is a single result phrase.
 * @see snsrForEach().
 */
#define SNSR_PHRASE_LIST "phrase-iterator"

/**
 * Iterate over the words in a result.
 * User-defined enrolled results are modeled as a single word.
 * @see snsrForEach().
 */
#define SNSR_WORD_LIST "word-iterator"

/**
 * Iterate over the phonemes in a result.
 * @see snsrForEach().
 */
#define SNSR_PHONE_LIST "phone-iterator"

/**
 * Iterate over the phoneme parts in a result.
 * @see snsrForEach().
 */
#define SNSR_MODEL_LIST "model-iterator"

/**
 * Iterate over the vocabulary available in a spotter.
 * @see snsrForEach(), #SNSR_RES_ID, #SNSR_RES_TEXT.
 */
#define SNSR_VOCAB_LIST "vocab-iterator"

/** @} task-spot */

/*---------------------------------------------------------------------------
 * Phrase spotter enrollment constants
 */
/* Task type */

/**
 * @addtogroup task-enroll Enrollment
 * See @ref task-enroll
 * @{
 */
/**
 * Phrase spotter enrollment task type.
 * @see snsrRequire()
 */
#define SNSR_ENROLL "enroll"

/**
 * Enrollment accuracy.
 * Trades accuracy of the enrolled model for enrollment speed.
 * The default accuracy is @c 1.0, for the best accuracy at the slowest
 * enrollment speed. Valid range is @c 0.0 to @c 1.0.
 * @see snsrSetDouble().
 */
#define SNSR_ACCURACY "accuracy"

/**
 * Interactive enrollment mode.
 * This changes the enrollment task behavior: When set to @c 0, enrollment
 * for the current phrase will continue until the end of the stream.
 * @see snsrSetInt().
 */
#define SNSR_INTERACTIVE_MODE "interactive"

/**
 * Enrolling user tag.
 * Sets the tag for the current enrollment. This should be a unique
 * alphanumeric phrase, without spaces. It is the phrase returned
 * as a recognition result.
 *
 * @note If enrolling more than one phrase for any of the users, the
 * tag @b must contain one '/' that separates a user-specific part
 * from the phrase part. For example: @c "user1/phrase1", @c "user2/phrase1",
 * @c "user2/phrase2"
 *
 * @see snsrGetString(), snsrSetString(), #SNSR_USER_LIST
 */
#define SNSR_USER "user"

/**
 * Delete an enrolled user.
 * Deletes the named user, then:
 * - invokes @const{ENROLLED_EVENT},
 * - invokes @const{ADAPTED_EVENT} iff any enrolled users remain,
 * - invokes @const{DONE_EVENT}.
 *
 * @see snsrSetString(), #SNSR_ENROLLED_EVENT, #SNSR_ADAPTED_EVENT,
 * #SNSR_DONE_EVENT.
 */
#define SNSR_DELETE_USER "delete-user"

/**
 * Force re-adaptation of all enrollments.
 * If all users in an enrollment task have been adapted, the adaptation
 * step is skipped. This is the case when one or more adapted enrollment
 * contexts are loaded, and no new users are added.
 *
 * Setting #SNSR_RE_ADAPT to @c 1 changes this behavior to always do the
 * adaptation step.
 * @see snsrSetInt(), #SNSR_ADAPTED_EVENT
 */
#define SNSR_RE_ADAPT "re-adapt"

/**
 * Enrollment progress.
 * A value between @c 0 and @c 100 that is an estimate of the enrollment
 * task completion progress.
 * @see snsrGetDouble().
 */
#define SNSR_RES_PERCENT_DONE "percent-done"

/**
 * Reason for enrollment failure.
 * Provides a shorthand indication of why the enrollment was rejected.
 *
 * List of possible values for #SNSR_RES_REASON
 *
 * Reason | Guidance
 * -------|---------
 * energy-min | Please speak louder.
 * energy-stddev | This recording does not sound like speech.
 * silence-begin | Please wait for the prompt before speaking.
 * silence-end | The trailing silence too short.
 * snr | The recording is too noisy. Please move to a quieter environment.
 * rec-variance | The difference between the recordings is too large. Please repeat the exact same phrase.
 * poor-rec-limit | The recording may not contain speech. Please speak a consistent trigger.
 * clipping | The recording is clipped, please reduce the volume.
 * vowel-duration | Please speak more slowly, or choose a different phrase with more vowel sounds.
 * repetition | This phrase has too many repeated sounds. Please choose another.
 * silence-in-phrase | Please don't pause - even briefly - in the middle of the recording.
 * spot | Please say the exact enrollment phrase, speaking clearly and naturally.
 * phrase-quality | This phrase is not suitable, please choose another or speak a little more slowly.
 * audio-quality | The enrollment shows signs of problems with the audio hardware.
 * audio-duration | The enrollment recording is too short.
 * audio-volume | No audio detected. Please speak louder.
 * audio-failure | @a varies.
 *
 * @see snsrGetString(), #SNSR_RES_GUIDANCE.
 */
#define SNSR_RES_REASON "reason"

/**
 * End-user guidance to correct enrollment failure.
 * Provides a human-readable string (in English) with a suggestion on
 * how to correct an enrollment failure.
 * @see snsrGetString()
 */
#define SNSR_RES_GUIDANCE "reason-guidance"

/**
 * Enrollment success.
 * @p 1 if the enrollment passed, @p 0 if it was rejected.
 * @see snsrGetInt()
 */
#define SNSR_RES_REASON_PASS "reason-pass"

/**
 * Enrollment check failure value.
 * The value of a enrollment check parameter. This is compared
 * to #SNSR_RES_REASON_THRESHOLD to determine #SNSR_RES_REASON_PASS.
 * @see snsrGetDouble()
 */
#define SNSR_RES_REASON_VALUE "reason-value"

/**
 * Enrollment check threshold value.
 * @see snsrGetDouble(), #SNSR_RES_REASON_VALUE
 */
#define SNSR_RES_REASON_THRESHOLD "reason-threshold"

/**
 * Enrollment count.
 * The number of enrollments accumulated for the enrolled user.
 * @see snsrGetInt(), #SNSR_USER_LIST, #SNSR_ENROLLMENT_TARGET
 */
#define SNSR_RES_ENROLLMENT_COUNT "enrollment-count"

/**
 * Enrollment ID.
 * A unique ID for the current user's current enrollment.
 * @see snsrGetInt(), #SNSR_ENROLLMENT_LIST.
 */
#define SNSR_RES_ENROLLMENT_ID "enrollment-id"

/**
 * Enrollment target.
 * The recommended number of enrollments for each user. Using either more or
 * fewer enrollments will reduce overall spotter performance.
 * @see snsrGetInt(), #SNSR_USER_LIST, #SNSR_RES_ENROLLMENT_COUNT
 */
#define SNSR_ENROLLMENT_TARGET "req-enroll"

/**
 * Number of enrollments with trailing context.
 * The recommended number of enrollments where the phrase is followed by
 * additional speech. For example: "Hey Sensory will it rain tomorrow?"
 * @see snsrGetInt()
 */
#define SNSR_ENROLLMENT_CONTEXT "ctx-enroll"

/**
 * The index of the sub-task to enroll.
 * For enrollment tasks that contain multiple sub-tasks (for example,
 * a user-defined trigger and an enrolled fixed trigger), this integer
 * value selects which of the sub-tasks the enrollments should be applied to.
 *
 * See the documentation delivered with the task file for the sub-task mapping.
 *
 * @note For most enrollment tasks the only supported task index is @c 0.
 */
#define SNSR_ENROLLMENT_TASK_INDEX "enrollment-task-index"

/**
 * Current enrollment includes trailing context.
 * Set to `1` if the enrollment recording should include trailing context,
 * for example: "Hey Sensory will it rain tomorrow?"
 * @see snsrGetInt(), #SNSR_RESUME_EVENT
 */
#define SNSR_ADD_CONTEXT "add-context"

/**
 * Include enrollment audio in enrollment context.
 * Set to @c 1 to include the enrollment audio in enrollment contexts.
 * @see snsrSetInt(), #SNSR_FM_RUNTIME
 */
#define SNSR_SAVE_ENROLLMENT_AUDIO "save-enroll-audio"

/**
 * Enrolled model stream.
 * The result after enrollment and adaptation. This is a model that will
 * recognize the enrolled phrases. Save to permanent storage with
 * snsrStreamCopy().
 * Retreiving the model stream will fail with @rc{SETTING_NOT_AVAILABLE}
 * if there are no enrolled users.
 * @see snsrGetStream(), snsrStreamCopy()
 */
#define SNSR_MODEL_STREAM "model-stream"

/**
 * Segmented input audio.
 * - For enrollment tasks with #SNSR_SAVE_ENROLLMENT_AUDIO is set to @c 1 (on)
 *   this is the enrollment recording. If #SNSR_SAVE_ENROLLMENT_AUDIO
 *   is @c 0 (off), audio will only be available in the #SNSR_FAIL_EVENT.
 * - For phrase spotter tasks, the samples from #SNSR_AUDIO_STREAM_FROM to
 *   #SNSR_AUDIO_STREAM_TO selected from the last
 *   #SNSR_AUDIO_STREAM_SIZE samples processed by the recognizer.
 *   The default #SNSR_AUDIO_STREAM_SIZE is @c 0, which disables audio
 *   buffering and will cause #SNSR_AUDIO_STREAM stream retrieval to fail.
 *   Be sure to set #SNSR_AUDIO_STREAM_SIZE to the expected number of
 *   samples before calling snsrRun().
 *
 * @see snsrGetStream().
 */
#define SNSR_AUDIO_STREAM "audio-stream"

/**
 * Adaptation complete event.
 * Handler called when adaptation has completed and
 * the #SNSR_MODEL_STREAM model is available.
 * This handler typically returns #SNSR_RC_STOP to end snsrRun() processing.
 * @see snsrSetHandler()
 */
#define SNSR_DONE_EVENT "^done"

/**
 * Enrollment complete event.
 * Handler called when enrollment is complete, just before
 * model adaptation starts. Use this handler for the earliest possible
 * access to the enrollment context.
 *
 * @note Enrollment is the process of taking individual enrollments
 * (audio files or live audio clips), validating them, and adding them to the
 * model. These enrollments are used during the adaptation stage to estimate
 * spotter model parameters.
 *
 * @see snsrSetHandler(), #SNSR_ADAPTED_EVENT.
 */
#define SNSR_ENROLLED_EVENT "^enrolled"

/**
 * Adaptation complete event.
 * Handler called when the adaptation of an enrollment is complete,
 * Use this handler for access to the adapted enrollment context.
 *
 * @note Adaptation is the estimation of spotter model parameters from
 * a number of enrollments for one or more users.
 *
 * @see snsrSetHandler(), #SNSR_ENROLLED_EVENT.
 */
#define SNSR_ADAPTED_EVENT "^adapted"

/**
 * Enrollment failed event.
 * Handler called if an enrollment fails quailty checks.
 * @see snsrSetHandler()
 */
#define SNSR_FAIL_EVENT "^fail"

/**
 * Enrollment succeeded event.
 * Handler called when an enrollment passes quality checks.
 *
 * Return #SNSR_RC_SKIP from this event handler to discard an enrollment.
 *
 * @see snsrSetHandler()
 */
#define SNSR_PASS_EVENT "^pass"

/**
 * Next user event.
 * Handler called during interactive enrollment to prompt the end-user
 * for a new enrollment phrase.
 * This handler should set #SNSR_USER to the tag associated with the new
 * user. Leave #SNSR_USER unchanged if there are no more users or phrases
 * to enroll.
 * @see snsrSetHandler()
 */
#define SNSR_NEXT_EVENT "^next"

/**
 * Input stream pause event.
 * Handler called when a time-consuming processing step is about to start.
 * Use this handler to pause the input stream when doing interactive enrollment.
 * @see snsrSetHandler()
 */
#define SNSR_PAUSE_EVENT "^pause"

/**
 * Adaptation progress event.
 * Handler called to report adaptation progress.
 * @see snsrSetHandler(), #SNSR_RES_PERCENT_DONE
 */
#define SNSR_PROG_EVENT "^progress"

/**
 * Input stream resume event.
 * Handler called when a time-consuming processing step has completed.
 * Use this handler to restart an input stream that was stopped during
 * a #SNSR_PAUSE_EVENT handler.
 * @see snsrSetHandler()
 */
#define SNSR_RESUME_EVENT "^resume"

/**
 * Iterate over all enrollments for the current user.
 * This can be used to retrieve enrollment audio, when
 * #SNSR_SAVE_ENROLLMENT_AUDIO is enabled.
 * @see snsrForEach(), #SNSR_USER, #SNSR_USER_LIST, #SNSR_SAVE_ENROLLMENT_AUDIO
 */
#define SNSR_ENROLLMENT_LIST "enrollment-iterator"

/**
 * Iterate over all reasons for enrollment failure.
 * @see snsrForEach()
 */
#define SNSR_REASON_LIST "reason-iterator"

/**
 * Iterate over all enrolled users.
 * Sets #SNSR_USER to each of the enrolled users before invoking the
 * callback.
 * @see snsrForEach(), #SNSR_USER
 */
#define SNSR_USER_LIST "user-iterator"

/** @} task-enroll */

/*---------------------------------------------------------------------------
 * Voice Activity Detector constants
 */

/**
 * @addtogroup task-vad Voice Activity Detection
 * See @ref task-vad
 * @{
 */
/**
 * VAD task type.
 * @see snsrRequire()
 */
#define SNSR_VAD "vad"

/**
 * Output audio stream.
 * Headerless audio output, 16-bit PCM encoded and sampled at 16 kHz.
 * @see snsrSetStream().
 */
#define SNSR_SINK_AUDIO_PCM "<-audio-pcm"

/**
 * Start point back-off in ms.
 * Audio margin added before the start point found by a Voice Activity Detector.
 *
 * @see snsrSetInt(), #SNSR_RES_BEGIN_MS, #SNSR_RES_BEGIN_SAMPLE.
 */
#define SNSR_BACKOFF "backoff"

/**
 * Endpoint hold-over.
 * Audio margin added after the endpoint found by a Voice Activity Detector.
 * This is the amount of trailing silence to include in the segmentation.
 *
 * @see snsrSetInt(), #SNSR_RES_END_MS, #SNSR_RES_END_SAMPLE.
 */
#define SNSR_HOLD_OVER "hold-over"

/**
 * Include leading silence in VAD output.
 * Set to @c 1 to include all audio up to the endpoint in the
 * #SNSR_SINK_AUDIO_PCM output stream. Set to @c 0 to return to the
 * default behavior, which discards leading silence.
 *
 * @see snsrSetInt(), #SNSR_SINK_AUDIO_PCM, #SNSR_PASS_THROUGH.
 */
#define SNSR_INCLUDE_LEADING_SILENCE "include-leading-silence"

/**
 * VAD leading silence time-out, in ms.
 * The Voice Activity Detector will invoke the #SNSR_SILENCE_EVENT handler
 * if no speech is detected during the first #SNSR_LEADING_SILENCE ms of
 * processed audio.
 *
 * @see snsrSetInt(), #SNSR_TRAILING_SILENCE, #SNSR_SILENCE_EVENT.
 */
#define SNSR_LEADING_SILENCE "leading-silence"

/**
 * VAD maximum record duration, in ms.
 * The Voice Activity Detector will invoke the #SNSR_LIMIT_EVENT handler
 * if the detected speech segment exceeds this value.
 *
 * @see snsrSetInt(), #SNSR_LIMIT_EVENT.
 */
#define SNSR_MAX_RECORDING "max-recording"

/**
 * VAD audio pass-through behaviour.
 * If set to @c 0, no audio from #SNSR_SOURCE_AUDIO_PCM will be passed
 * through to #SNSR_SINK_AUDIO_PCM. The begin- and endpoint handlers will
 * still be invoked. The default value (@c 1) passes speech-detected samples
 * to #SNSR_SINK_AUDIO_PCM.
 *
 * @see snsrSetInt(), #SNSR_INCLUDE_LEADING_SILENCE.
 */
#define SNSR_PASS_THROUGH "pass-through"

/**
 * VAD trailing silence time-out, in ms.
 * The Voice Activity Detector will invoke the #SNSR_DONE_EVENT handler
 * once #SNSR_TRAILING_SILENCE of ms has followed the last bit of speech.
 *
 * @see snsrSetInt(), #SNSR_LEADING_SILENCE, #SNSR_END_EVENT, #SNSR_HOLD_OVER.
 */
#define SNSR_TRAILING_SILENCE "trailing-silence"

/**
 * VAD initial ignore duration, in ms.
 * Ignore the first #SNSR_SKIP_TO_MS ms of the #SNSR_SOURCE_AUDIO_PCM input
 * stream. Use this runtime setting to skip over a trigger phrase included
 * in the source audio. The default is to process all audio.
 *
 * @see snsrSetDouble(), #SNSR_SKIP_TO_SAMPLE.
 */
#define SNSR_SKIP_TO_MS "skip-to-ms"

/**
 * VAD initial ignore duration, in samples.
 * Ignore the first #SNSR_SKIP_TO_SAMPLE samples of the #SNSR_SOURCE_AUDIO_PCM
 * input stream. Use this runtime setting to skip over a trigger phrase included
 * in the source audio. The default is to process all audio.
 *
 * @see snsrSetDouble(), #SNSR_SKIP_TO_MS.
 */
#define SNSR_SKIP_TO_SAMPLE "skip-to-sample"

/**
 * Begin point detected VAD event.
 * Raised when speech has been detected. Use #SNSR_RES_BEGIN_MS or
 * #SNSR_RES_BEGIN_SAMPLE to retrieve the start point relative to the beginning
 * of #SNSR_SOURCE_AUDIO_PCM.
 *
 * @see snsrSetHandler(), #SNSR_BACKOFF.
 */
#define SNSR_BEGIN_EVENT "^begin"

/**
 * Endpoint detected VAD event.
 * Raised #SNSR_TRAILING_SILENCE ms after end-of-speech has been detected.
 * Use #SNSR_RES_END_MS or #SNSR_RES_END_SAMPLE to retrieve the endpoint
 * relative to the beginning of #SNSR_SOURCE_AUDIO_PCM.
 *
 * @see snsrSetHandler(), #SNSR_HOLD_OVER, #SNSR_TRAILING_SILENCE.
 */
#define SNSR_END_EVENT "^end"

/**
 * Maxium recording reached VAD event.
 * Raised when #SNSR_MAX_RECORDING ms of speech has been processed by the
 * Voice Activity Detector before a trailing-silence endpoint is found.
 * Use #SNSR_RES_END_MS or #SNSR_RES_END_SAMPLE to retrieve the endpoint
 * relative to the beginning of #SNSR_SOURCE_AUDIO_PCM.
 *
 * @see snsrSetHandler(), #SNSR_HOLD_OVER, #SNSR_MAX_RECORDING.
 */
#define SNSR_LIMIT_EVENT "^limit"

/**
 * No speech detected event.
 * Raised if no speech is detected within #SNSR_LEADING_SILENCE ms from the
 * start of #SNSR_SOURCE_AUDIO_PCM (adjusted by #SNSR_SKIP_TO_MS or
 * #SNSR_SKIP_TO_SAMPLE).
 *
 * @see snsrSetHandler(), #SNSR_LEADING_SILENCE.
 */
#define SNSR_SILENCE_EVENT "^silence"

/** @} task-vad    */


/*---------------------------------------------------------------------------
 * Phrase Spotter Template
 */

/**
 * @addtogroup task-template Phrase Spotter Templates
 * See @ref task-template
 * @{
 */

/**
 * Template slot selector.
 * Use with @ref tpl-spot-select to select the active slot.
 *
 * @if C
 * Example:
 * @code
 * snsrSetString(session, SNSR_SLOT, SNSR_SLOT_1);
 * @endcode
 * @endif
 * @if JAVA || ANDROID
 * Example:
 * @code
 * session.setString(SNSR_SLOT, SNSR_SLOT_1);
 * @endcode
 * @endif
 *
 * @see snsrSetString(), #SNSR_SLOT_0, #SNSR_SLOT_1.
 */
#define SNSR_SLOT "slot"

/**
 * Template slot 0.
 * The first phrase spotter slot in a template task.
 * * @see snsrSetStream().
 */
#define SNSR_SLOT_0 "0."

/**
 * Template slot 1.
 * The second phrase spotter slot in a template task.
 * @see snsrSetStream().
 */
#define SNSR_SLOT_1 "1."

/**
 * Debug log filename.
 * The name of the log file @ref tpl-spot-debug writes to. This value
 * is required, and no default is defined in the template. The directory
 * the log file is in must exist, and must be writable.
 *
 * These optional and mutually exclusive character sequences are
 * substituted with the time stamp when the log file is first opened:
 *   - @c %@ - year-month-day_hour-minute-second.milliseconds (UTC)
 *   - @c %# - milliseconds since the epoch.
 *
 * Example:
 * @code
 * snsrSetString(session, SNSR_DEBUG_LOG_FILE, "debug-%#.log");
 * @endcode
 *
 * @see snsrSetString().
 */
#define SNSR_DEBUG_LOG_FILE "debug-log-file"

/**
 * Debug log includes a copy of the model.
 * This boolean value controls whether the #SNSR_DEBUG_LOG_FILE includes
 * a copy of the task model (the .snsr file).
 *
 * The default value is @c 1. Set #SNSR_INCLUDE_MODEL to @c 0 for
 * smaller (but less complete) debug log files.
 *
 * @see snsrGetInt(), snsrSetInt().
 */
#define SNSR_INCLUDE_MODEL "include-model"

/** @} task-template */

/*---------------------------------------------------------------------------
 * Phrase Spotter with Voice Activity Detector constants
 */

/**
 * @addtogroup task-phrasespot-vad Phrase Spotter with Voice Activity Detection
 * See @ref task-phrasespot-vad
 * @{
 */
/**
 * Phrase Spotter VAD task type.
 * @see snsrRequire()
 */
#define SNSR_PHRASESPOT_VAD "phrasespot-vad"

/** @} task-phrasespot-vad */
/** @} task */
/*---------------------------------------------------------------------------*/

#include <stddef.h>

/**
 * @addtogroup build-info Version Macros
 * Compile-time API version information.
 * @{
 */
/** SDK variant name. */
#define SNSR_NAME          "TrulyHandsfree"
/** True if SDK type is TrulyHandsfree */
#define SNSR_IS_TRULYHANDSFREE 1
/** True if SDK type is TrulyNatural */
#define SNSR_IS_TRULYNATURAL   0
/** API version. Follows [semantic versioning](http://semver.org/) rules. */
#define SNSR_VERSION       "6.12.0"
/** API major version number. Incremented for API changes that are
 *  **not** backwards-compatible. */
#define SNSR_VERSION_MAJOR  6
/** API minor version number. Incremented for changes and improvements that
 *  **are** backwards-compatible. */
#define SNSR_VERSION_MINOR  12
/** API patch version number. Incremented for bug fixes. */
#define SNSR_VERSION_PATCH  0
/** Pre-release version tag. This is empty for official releases. */
#define SNSR_VERSION_PRE   ""
/** Build information. This is empty for official releases. */
#define SNSR_VERSION_BUILD ""
/** Unique release version counter. */
#define SNSR_VERSION_ID     52
/** DSP embedded library version. */
#define SNSR_VERSION_DSP   "6.1.4"
/** @} */

/* The largest supported setting key length. */
#define SNSR_SETTING_SZ     64
/**
 * @addtogroup session Session
 * Primary TrulyHandsfree SDK API.
 * @{
 */

/**
 * Opaque session handle. This is the primary handle for SDK operations.
 * Session handles contain the entire state for a task.
 *
 * Functions operating on the same ::SnsrSession instance handle
 * must be protected from re-entrant calls with application-level locks.
 *
 * Synchronization and locking is not needed if each ::SnsrSession instance
 * is accessed from a single thread only.
 */
typedef struct SnsrSession_ *SnsrSession;

/**
 * Session serialization format.
 *  @see snsrSave().
 */
typedef enum {
  /** All configuration settings. */
  SNSR_FM_CONFIG,
  /** Custom initialization code for a model subset.
   * Limited initialization reduces overall executable code size
   * by eliding unused modules. To enable, add the generated custom code to
   * the build, and recompile with -DSNSR_USE_SUBSET.
   */
  SNSR_FM_SUBSET_INIT,
  /** Runtime settings only, e.g. enrollments. */
  SNSR_FM_RUNTIME,
  /** All configuration settings, as C source code.
   * Code is TrulyHandsfree SDK version specific. If you upgrade the
   * TrulyHandsfree SDK, also update any generated C source files.
   */
  SNSR_FM_SOURCE
} SnsrDataFormat;

/**
 * SDK return code. #SNSR_RC_OK indicates success, anything else is a failure.
 */
typedef enum {
  /** Operation completed successfully. */
  SNSR_RC_OK,
  /** Cannot read or write past the end of the stream. */
  SNSR_RC_EOF,
  /** Operation failed. */
  SNSR_RC_ERROR,
  /** Stream is not open. */
  SNSR_RC_NOT_OPEN,
  /** Resource not found. */
  SNSR_RC_NOT_FOUND,
  /** Out of heap memory. */
  SNSR_RC_NO_MEMORY,
  /** Incorrect stream mode (read or write). */
  SNSR_RC_WRONG_MODE,
  /** Interrupted. */
  SNSR_RC_INTERRUPTED,
  /** Invalid argument. */
  SNSR_RC_INVALID_ARG,
  /** Invalid mode. */
  SNSR_RC_INVALID_MODE,
  /** Cannot re-open this stream. */
  SNSR_RC_CANNOT_REOPEN,
  /** The stream handle is invalid. */
  SNSR_RC_INVALID_HANDLE,
  /** Function is not implemented for this stream type. */
  SNSR_RC_NOT_IMPLEMENTED,
  /** Delimiter character not encountered. */
  SNSR_RC_DELIM_NOT_FOUND,
  /** Stream format is not supported. */
  SNSR_RC_FORMAT_NOT_SUPPORTED,
  /** Buffer overrun, stream not read from in time */
  SNSR_RC_BUFFER_OVERRUN,
  /** Buffer underrun, stream not written to in time. */
  SNSR_RC_BUFFER_UNDERRUN,
  /** Invalid return code (SNSR_RC_RESERVED_A is not used). */
  SNSR_RC_RESERVED_A,
  /** Argument value is not in the valid range. */
  SNSR_RC_ARG_OUT_OF_RANGE,
  /** A channel with this name already exists in the bin. */
  SNSR_RC_CHANNEL_EXISTS,
  /** This channel is not backed by a circular buffer. */
  SNSR_RC_CHANNEL_NOT_BUFFERED,
  /** Channel not found. */
  SNSR_RC_CHANNEL_NOT_FOUND,
  /** Source and destination channel types are not compatible. */
  SNSR_RC_CHANNELS_NOT_COMPATIBLE,
  /** Element settings are inconsistent. */
  SNSR_RC_CONFIGURATION_INCONSISTENT,
  /** Element configuration is missing. */
  SNSR_RC_CONFIGURATION_MISSING,
  /** Connection not found. */
  SNSR_RC_CONNECTION_NOT_FOUND,
  /** Destination channel not found. */
  SNSR_RC_DST_CHANNEL_NOT_FOUND,
  /** Destination element is not in this bin. */
  SNSR_RC_DST_ELEMENT_NOT_IN_BIN,
  /** The destination port is already connected. */
  SNSR_RC_DST_PORT_IN_USE,
  /** Element ID is not known. The model either requires an updated SDK library,
   * or a build with the default snsrNew() initialization.
   * See #SNSR_PREPARE_SUBSET_INIT, #SNSR_FM_SUBSET_INIT.
   */
  SNSR_RC_ELEMENT_ID_NOT_KNOWN,
  /** Element initialization failed. */
  SNSR_RC_ELEMENT_INIT_FAILED,
  /** Element initialization is incomplete. */
  SNSR_RC_ELEMENT_INIT_INCOMPLETE,
  /** Element is not a bin, function is not available. */
  SNSR_RC_ELEMENT_IS_NOT_A_BIN,
  /** Element is not in this bin. */
  SNSR_RC_ELEMENT_NOT_IN_BIN,
  /** Element is not the root bin, function is not available. */
  SNSR_RC_ELEMENT_NOT_ROOT_BIN,
  /** Element registration failed. */
  SNSR_RC_ELEMENT_REGISTRATION_FAILED,
  /** Setting is not of the correct type. */
  SNSR_RC_INCORRECT_SETTING_TYPE,
  /** License validation failed. */
  SNSR_RC_LICENSE_NOT_VALID,
  /** Push iteration limit exceeded. */
  SNSR_RC_ITERATION_LIMIT,
  /** Element API processing check failed. */
  SNSR_RC_ELEMENT_API_VIOLATION,
  /** Name is not unique for this bin. */
  SNSR_RC_NAME_NOT_UNIQUE,
  /** Buffer does not have enough free space. */
  SNSR_RC_NOT_ENOUGH_SPACE,
  /** Element has not been initialized. */
  SNSR_RC_NOT_INITIALIZED,
  /** Push failed. */
  SNSR_RC_PUSH_FAILED,
  /** Repeated by callback. */
  SNSR_RC_REPEAT,
  /** Setting could not be applied. */
  SNSR_RC_SETTING_FAILED,
  /** Setting is read-only. */
  SNSR_RC_SETTING_IS_READ_ONLY,
  /** Setting is not available in this context. */
  SNSR_RC_SETTING_NOT_AVAILABLE,
  /** Setting not found for this element. */
  SNSR_RC_SETTING_NOT_FOUND,
  /** Skipped by callback. */
  SNSR_RC_SKIP,
  /** Source channel not found. */
  SNSR_RC_SRC_CHANNEL_NOT_FOUND,
  /** Source element is not in this bin. */
  SNSR_RC_SRC_ELEMENT_NOT_IN_BIN,
  /** The source port is already connected. */
  SNSR_RC_SRC_PORT_IN_USE,
  /** Stopped by callback. */
  SNSR_RC_STOP,
  /** Stream operation failed. */
  SNSR_RC_STREAM,
  /** End-of-stream reached. */
  SNSR_RC_STREAM_END,
  /** Unknown configuration object type. */
  SNSR_RC_UNKNOWN_OBJECT_TYPE,
  /** Setting has no configured value. */
  SNSR_RC_VALUE_NOT_SET,
  /** SnsrCodeModel task is for an older SDK release. Re-convert from
      the source *.snsr file. */
  SNSR_RC_CODE_MODEL_TOO_OLD,
  /** Invalid return code (SNSR_RC_RESERVED_B is not used). */
  SNSR_RC_RESERVED_B,
  /** No task model data found. */
  SNSR_RC_NO_MODEL,
  /** Task value does not match requirement. */
  SNSR_RC_REQUIRE_MISMATCH,
  /** Task version does not match the requirement. */
  SNSR_RC_VERSION_MISMATCH,
  /**
   * The header file version does not match that of the library.
   * SNSR_VERSION in snsr.h does not match the version the
   * library archive was compiled with.
   *
   * Use the snsr.h shipped with the
   * TrulyHandsfree 6.12.0 SDK distribution.
   */
  SNSR_RC_LIBRARY_HEADER,
  /**
   * TrulyHandsfree library is too old.
   * Loading of a new task failed as it requires features that are not
   * available in this release of the library. Please contact Sensory for
   * assistance.
   * @see snsrLoad(), #SNSR_VERSION
   */
  SNSR_RC_LIBRARY_TOO_OLD,
  /** Operation timed out. */
  SNSR_RC_TIMED_OUT
} SnsrRC;

/**
 * @addtogroup callback User Callbacks
 * User-defined callback functions with associated data.
 * A #SnsrCallback handle encapsulates a user-defined function with the
 * #SnsrHandler signature, arbitrary user data as a `void *`, and a clean-up
 * function used to release the user data when the SnsrCallback handle is
 * released.
 *
 * #SnsrCallback handles are used for all events generated by #SnsrSession.
 * @{
 */

/**
 * Callback handle.
 * @todo Add detail.
 */
typedef struct SnsrCallback_ *SnsrCallback;

/**
 * Process event callbacks.
 * Called when a ::SnsrCallback instance is invoked.
 *
 * @param[in] s the ::SnsrSession handle the callback was registered with.
 * @param[in] key the name of the invoked ::SnsrSession callback setting.
 * @param[in,out] data user pointer, the @p data parameter of snsrCallback().
 * @return #SNSR_RC_OK if all is well. Errors are reported to the
 *         code that invoked the callback.  #SNSR_RC_STOP is typically used to
 *         terminate snsrFlowRun() or snsrForEach().
 */
typedef SnsrRC (*SnsrHandler)(SnsrSession s, const char *key, void *data);

/**
 * Release private user data.
 * Called to release the private user data associated with a ::SnsrCallback
 * when it is deleted.
 *
 * @param[in] data user pointer, the @p data parameter of snsrCallback().
 */
typedef void (*SnsrDataRelease)(const void *data);

/**
 * Creates a new ::SnsrCallback instance. These are used to invoke
 * library callbacks and iterators.
 * - New handles have a reference count of `0`. Use snsrRetain()
 *   if keeping a reference and snsrRelease() to release such a reference.
 * - ::SnsrHandler @p h will be called when this callback is invoked.
 * - ::SnsrDataRelease @p r will be called when the callback reference count
 *   reaches `0` and is deleted. @p r must release the user @p data structure.
 *
 * @param[in] h the function to call when this callback is invoked.
 * @param[in] r a function that releases @p data when the callback is destroyed.
 *              Use `NULL` if no clean-up is required.
 * @param[in] data private user pointer.
 * @return A new callback instance.
 *         `NULL` if `h` is `NULL` or heap allocation failed.
 * @see snsrRun(), snsrForEach().
 */
SnsrCallback
snsrCallback(SnsrHandler h, SnsrDataRelease r, void *data);
/** @} callback */
/** @} session  */


/**
 * @addtogroup stream Stream
 * Stream abstraction API.
 * This section describes the stream abstraction through which all
 * ::SnsrSession inputs and outputs are vectored.
 *
 * The stream API is small and has well-defined semantics. Streams
 * can be created on different underlying data streams (file names, `FILE *`
 * handles, memory segments, live audio sources and sinks, and so on).
 * Only the @ref stream-provider are type-specific,
 * all other functions are independent of the underlying stream type.
 * @ref stream-ops that take ::SnsrStream handles as arguments operate on any of
 * the types. The same recognition function, for example, could be used
 * to recognize from file or live input.
 *
 * ::SnsrStream handles are reference-counted (snsrRetain(), snsrRelease()).
 * Handles are disposed of when their reference counts reach zero.
 * New handles are created with a reference count of `0`.
 *
 * No operations are done on the underlying data provider until
 * snsrStreamOpen() is called, and no operations are done after
 * snsrStreamClose() has been called.
 *
 * @note The current implementation does not do any thread-level locking.
 * Threaded access to stream handles must be protected by explicit
 * synchronization calls.
 *
 * @{
 */

/**
 * @addtogroup stream-ops Operations
 * Stream operations.
 * Functions in this group are used for basic ::SnsrStream operations, such
 * as opening, reading, writing and closing streams. Additional utility
 * functions are provided to print formatted data to a stream, and to
 * inspect stream state.
 *
 * The error state of a ::SnsrStream handle persists only until the next
 * operation on the stream. This behaviour differs from ::SnsrSession errors,
 * which must be cleared explicitly.
 *
 * These functions do not change ::SnsrStream handle reference counts.
 * They are therefore safe to use on handles where reference count is zero.
 * @{
 */

/**
 * Opaque stream handle.
 * All ::SnsrSession input and output is vectored through this type.
 */
typedef struct SnsrStream_  *SnsrStream;

/**
 * Opaque code model handle.
 * @see snsrStreamFromCode(), snsrSave(), #SNSR_FM_SOURCE.
 */
typedef const struct SnsrCodeModel_ *SnsrCodeModel;




/**
 * Stream introspection key.
 * @see snsrStreamGetMeta().
 */
typedef enum {
  /** Number of bytes read from the stream. */
  SNSR_ST_META_BYTES_READ,
  /** Number of bytes written to the stream. */
  SNSR_ST_META_BYTES_WRITTEN,
  /** Boolean, 1 if the stream is open. */
  SNSR_ST_META_IS_OPEN,
  /** Boolean, 1 if the stream is readable. */
  SNSR_ST_META_IS_READABLE,
  /** Boolean, 1 if the stream is writable. */
  SNSR_ST_META_IS_WRITABLE,
  /** Number of times the stream has been opened. */
  SNSR_ST_META_OPEN_COUNT
} SnsrStreamMeta;

/**
 * Reports stream end status.
 * Returns `1` if the stream has reached its end.
 * * Follows Unix semantics:
 *   - The end-of-stream indicator is only valid after a read attempt.
 *   - A read on a ::SnsrStream `b` could very well return `0`, even if
 *     `snsrStreamAtEnd(b)` returned `0` just before attempting the read.
 * * Equivalent to `(snsrStreamRC(b) == SNSR_RC_EOF)`.
 *
 * @param[in] b ::SnsrStream handle.
 * @return A boolean value, `1` if at end-of-stream, `0` if not.
 * @see snsrStreamRC().
 */
int
snsrStreamAtEnd(SnsrStream b);

/**
 * Closes an open stream.
 * - Any remaining buffered output is flushed.
 * - Any remaining buffered input is discarded.
 * - Closing a stream that is not open is not an error.
 * - This does not destroy the ::SnsrStream data structure,
 *   use snsrRelease() to do so.
 * - Streams that are still open when the last reference is removed
 *   are closed before the handle is released.
 *
 * @param[in] b stream to close.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrStreamClose(SnsrStream b);

/**
 * Copies from SnsrStream to SnsrStream.
 * - Opens @p dst and @p src if these streams are not already open.
 * - Copies @p sizeInBytes bytes from @p src to @p dst, then returns the
 *   number of bytes successfully written to @p dst.
 * - If an error occurred (including an end-of-stream condition on either
 *   @p dst or @p src) the return value will be less than @p sizeInBytes.
 * - Errors in @p src are copied to @p dst; use `snsrStreamRC(dst)` to
 *   retrieve these.
 *
 * @param[in] dst destination stream.
 * @param[in] src source stream.
 * @param[in] sizeInBytes number of bytes to copy from @p src to @p dst.
 * @return The number of bytes successfully written to @p dst.
 * @see snsrStreamRC().
 */
size_t
snsrStreamCopy(SnsrStream dst, SnsrStream src, size_t sizeInBytes);

/**
 * Retrieves a detailed stream error message.
 * - This human-readable error message containing the reason for failure.
 * - Use for display or logging only, as the content of the message is
 *   unspecified and system-specific.
 * - Do not parse this to determine program flow,
 *   use the ::SnsrRC return code instead.
 *
 * @param[in] b stream to examine.
 * @return A detailed message describing the most recent reason for failure
 *         in ::SnsrStream @p b.
 *         The memory pointed to is owned by this library and must not be
 *         released. It is not reference-counted, calling snsrRetain() or
 *         snsrRelease() on this handle will panic the heap allocator.
 *         The handle remains valid only until the next API call on @p b.
 */
const char *
snsrStreamErrorDetail(SnsrStream b);

/**
 * Reads one line from a stream.
 * Reads a line from stream @p b, delimited by the character @p delim into
 * @p buffer. This is similar to `getdelim()`, except that it operates on
 * a ::SnsrStream and does not allocate memory for the destination buffer.
 *
 * - Opens the @p b stream if it is not already open.
 * - Reads up to `bufferSize - 1` bytes from @p b into @p buffer, stops when
 *   @p delim has been read (@p buffer will include @p delim).
 *   Terminates the string in @p buffer with `\0` and
 *   returns the number of characters read (not including the terminating `\0`).
 * - If the delimiter is not found after reading `bufferSize - 1` bytes, the
 *   error code in @p b is set to #SNSR_RC_DELIM_NOT_FOUND.
 * - `buffer != NULL` and `bufferSize >= 2` is required.
 * - `buffer` will contain all characters read and will be terminated with `\0`,
 *    even if an error occurs.
 *
 * @param[in]  b input stream.
 * @param[out] buffer destination buffer.
 * @param[in]  bufferSize size of @p buffer in bytes.
 * @param[in]  delim delimiter character to search for.
 * @return The number of characters read and placed into @p buffer.
 */
size_t
snsrStreamGetDelim(SnsrStream b, void *buffer, size_t bufferSize, int delim);

/**
 * Queries stream metadata.
 * Returns ::SnsrStream metadata, selected by the ::SnsrStreamMeta @p key.
 * If an error occurs this function returns `0` and sets the error state in
 * stream @p b. Use snsrStreamRC() to check the error state.
 *
 * @param[in] b stream to query.
 * @param[in] key query type.
 * @return The integer value for @p key.
 * @see snsrStreamRC().
 */
size_t
snsrStreamGetMeta(SnsrStream b, SnsrStreamMeta key);

/**
 * Opens a stream.
 * - Read and write operations are valid only on open streams.
 * - Newly created streams are not open.
 * - Streams are always opened in binary mode.
 * - Some stream types can be closed and re-opened. This behaviour is
 *   documented in the `snsrStreamFrom*` section for each stream type. If
 *   re-opening is not supported snsrStreamOpen() will return
 *   #SNSR_RC_CANNOT_REOPEN when an attempt is made to open it a second time.
 *
 * @param[in] b stream to open.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrStreamOpen(SnsrStream b);

/**
 * Prints a formatted string to a stream.
 * This function is similar to `fprintf()`, but operates on a ::SnsrStream
 * instead of a `FILE *`.
 * Opens the @p b stream if it is not already open.
 * If an error occurs this function returns `0` and sets the error state in
 * stream @p b. Use snsrStreamRC() to check the error state.
 *
 * @param[in] b destination stream.
 * @param[in] format `fprintf()` style format string.
 * @param[in] ... arguments specified by the @p format string.
 * @return The number of bytes written to @p b, or 0 if an error occurred.
 * @see snsrStreamRC().
 */
size_t
snsrStreamPrint(SnsrStream b, const char *format, ...);

/**
 * Retrieves the most recent return code for a stream.
 *
 * @param[in] b stream to examine.
 * @return The ::SnsrRC code from stream @p b.
 */
SnsrRC
snsrStreamRC(SnsrStream b);

/**
 * Reads from a stream into a memory location.
 * - Opens the @p a stream if it is not already open.
 * - Reads @p nitems items, each @p size bytes long, from the stream @p b
 *   into @p buffer and returns the number of items read.
 * - If either @p size or @p nitems is zero this function returns `0` without
 *   reading anything.
 * - A short read (number of items read less than @p nitems) will occur only
 *   if the end of the stream was reached before @p nitems were read, or if
 *   an error occurred. The stream return code will be set appropriately.
 *   Use snsrStreamAtEnd() or snsrStreamRC() to distinguish between these
 *   conditions.
 * - If the end of the stream is encountered in the middle of an object, it
 *   returns the number of complete items read. The partial object is
 *   discarded and the stream remains in the end-of-stream condition.
 * - Read operations block until all the requested data have been read, or
 *   an error or end-of-stream is encountered.
 *
 * @param[in]  a stream to read from.
 * @param[out] buffer memory to write to.
 * @param[in]  size the size of an item, in bytes.
 * @param[in]  nitems the number of items to read.
 * @return The number of items read.
 * @see snsrStreamAtEnd(), snsrStreamRC(), snsrStreamSkip().
 */
size_t
snsrStreamRead(SnsrStream a, void *buffer, size_t size, size_t nitems);

/**
 * Reads from a stream and discards the results.
 * This function is similar to snsrStreamRead(), but discards the read data
 * instead of writing it to a memory buffer.
 *
 * @param[in] a stream to read from.
 * @param[in] size the size of an item, in bytes.
 * @param[in] nitems the number of items to read and discard.
 * @return The number of items read.
 * @see snsrStreamAtEnd(), snsrStreamRC(), snsrStreamRead().
 */
size_t
snsrStreamSkip(SnsrStream a, size_t size, size_t nitems);

/**
 * Writes from a buffer to a stream.
 * - Opens the @p a stream if it is not already open.
 * - Writes @p nitems items, each @p size bytes long, to the stream @p b,
 *   obtaining them from @p buffer. Returns the number of items written.
 * - Returns less than @p nitems only if an error occurred. Use
 *   snsrStreamRC() to obtain the error code.
 * - Write operations block until the requested data have been transferred
 *   to the underlying implementation, but may return well before the
 *   data have reached their final destination (written to disk, played from
 *   a speaker, etc.)
 *
 * @param[in] a stream to write to.
 * @param[in] buffer memory to read from.
 * @param[in] size the size of an item, in bytes.
 * @param[in] nitems the number of items to write.
 * @return The number of items written.
 * @see snsrStreamRC().
 */
size_t
snsrStreamWrite(SnsrStream a, const void *buffer, size_t size, size_t nitems);

/** @} stream-ops */

/**
 * @addtogroup stream-provider Type Constructors
 * This section describes the predefined stream type constructors.
 *
 * ::SnsrStream wrappers are provided for @c &lt;stdio.h&gt; files,
 * memory segments, FIFO buffers and audio recording devices.
 * Additional types provide support for ::SnsrStream transformations,
 * such as concatenation and format conversion.
 *
 * Functions in this group that take ::SnsrStream arguments retain these
 * handles and release them when they are no longer used. A ::SnsrStream
 * handle with a zero reference count used as an argument will therefore
 * be deallocated when it is no longer needed. Be sure to call
 * snsrRetain() on handles that need to survive these function
 * invocations.
 *
 * @todo Add SnsrStream Provider example, such as snsrStreamFromAsset()
 * @{
 */

/**
 * Process stream event callbacks.
 * Functions of this type are called when a snsrStreamRaise() stream
 * type is opened.
 *
 * @param[in] s the ::SnsrStream that was opened.
 * @param[in] data user pointer, the @p data parameter of snsrStreamRaise()
 * @return #SNSR_RC_OK if all is well. Errors are reported to the
 *         code that raised the callback, such as snsrStreamOpen().
 */
typedef SnsrRC (*SnsrStreamEvent)(SnsrStream s, void *data);

/**
 * Stream audio format specifier.
 * @see snsrStreamFromAudioDevice(), snsrStreamFromAudioStream().
 */
typedef enum {
  /**
   * Read-only default audio stream format.
   */
  SNSR_ST_AF_DEFAULT,

  /**
   * Read-only default audio stream format, low latency.
   * Low latency, at the expense of higher CPU overhead.
   */
  SNSR_ST_AF_DEFAULT_LOW_LATENCY,

  /**
   * Read-only 16-bit little-endian LPCM at 16 kHz.
   */
  SNSR_ST_AF_LPCM_S16_16K,

  /**
   * Read-only 16-bit little-endian LPCM at 16 kHz.
   * Expected args
   *   @code size_t *samples @endcode
   * Returns the number of audio samples read from the RIFF
   * header in @p samples.
   */
  SNSR_ST_AF_LPCM_S16_16K_SAMPLES,

  /**
   * Read-only 16-bit little-endian LPCM at 16 kHz, low latency.
   * Low latency, at the expense of higher CPU overhead.
   */
  SNSR_ST_AF_LPCM_S16_16K_LOW_LATENCY,

  /**
   * Read-only 16-bit little-endian LPCM with a user-specified device.
   * Expected args
   *   @code const char *device @endcode
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVICE, "default");
   * @endcode
   */
  SNSR_ST_AF_DEVICE,

  /**
   * Read-only 16-bit LE LPCM with a user-specified device, low latency.
   * Low latency, at the expense of higher CPU overhead.
   * Expected args
   *   @code const char *device @endcode
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVICE_LOW_LATENCY, "default");
   * @endcode
   */
  SNSR_ST_AF_DEVICE_LOW_LATENCY,

  /**
   * Read-only 16-bit LE LPCM, user-specified device, sample rate, and mode.
   * Expected args:
   *   @code const char *device, unsigned int rate, SnsrStreamMode mode @endcode
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVICE_RATE_MODE,
   *                               "default", 16000, SNSR_ST_MODE_READ);
   * @endcode
   */
  SNSR_ST_AF_DEVICE_RATE_MODE,

  /**
   * Read-only 16-bit LE LPCM, device, rate, and mode. Low latency.
   * Low latency, at the expense of higher CPU overhead.
   *
   * Expected args:
   *   @code const char *device, unsigned int rate, SnsrStreamMode mode @endcode
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVICE_RATE_MODE_LOW_LATENCY,
   *                               "default", 16000, SNSR_ST_MODE_READ);
   * @endcode
   */
  SNSR_ST_AF_DEVICE_RATE_MODE_LOW_LATENCY,

  /**
   * Read-only 16-bit little-endian LPCM with a user-specified device.
   * Expected args
   *   @code int devid @endcode
   *
   * @note On Windows, @c devid is the device ID used with
   * the Multimedia Extensions, such as [waveInGetDevCaps()][1].
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVID, 0);
   * @endcode
   *
   * [1]: https://msdn.microsoft.com/en-us/library/dd743841(v=vs.85).aspx
   */
  SNSR_ST_AF_DEVID,

  /**
   * Read-only 16-bit LE LPCM with a user-specified device, low latency.
   * Low latency, at the expense of higher CPU overhead.
   * Expected args
   *   @code int devid @endcode
   *
   * @note On Windows, @c devid is the device ID used with
   * the Multimedia Extensions, such as [waveInGetDevCaps()][1].
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVID_LOW_LATENCY, 0);
   * @endcode
   *
   * [1]: https://msdn.microsoft.com/en-us/library/dd743841(v=vs.85).aspx
   */
  SNSR_ST_AF_DEVID_LOW_LATENCY,

  /**
   * Read-only 16-bit LE LPCM, user-specified device, sample rate, and mode.
   * Expected args:
   *   @code int devid, unsigned int rate, SnsrStreamMode mode @endcode
   *
   * @note On Windows, @c devid is the device ID used with
   * the Multimedia Extensions, such as [waveInGetDevCaps()][1].
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVID_RATE_MODE,
   *                               0, 16000, SNSR_ST_MODE_READ);
   * @endcode
   *
   * [1]: https://msdn.microsoft.com/en-us/library/dd743841(v=vs.85).aspx
   */
  SNSR_ST_AF_DEVID_RATE_MODE,

  /**
   * Read-only 16-bit LE LPCM, device, rate, and mode. Low latency.
   * Low latency, at the expense of higher CPU overhead.
   *
   * Expected args:
   *   @code int devid, unsigned int rate, SnsrStreamMode mode @endcode
   *
   * @note On Windows, @c devid is the device ID used with
   * the Multimedia Extensions, such as [waveInGetDevCaps()][1].
   *
   * Example:
   * @code
   * b = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVID_RATE_MODE_LOW_LATENCY,
   *                               0, 16000, SNSR_ST_MODE_READ);
   * @endcode
   *
   * [1]: https://msdn.microsoft.com/en-us/library/dd743841(v=vs.85).aspx
   */
  SNSR_ST_AF_DEVID_RATE_MODE_LOW_LATENCY

} SnsrStreamAudioFormat;


/**
 * Stream input or output mode.
 * @see snsrStreamFromFILE(), snsrStreamFromMemory().
 */
typedef enum {
  /** Read from this input stream. */
  SNSR_ST_MODE_READ,
  /** Write to this output stream. */
  SNSR_ST_MODE_WRITE
} SnsrStreamMode;

#if defined(__ANDROID__) || defined(DOXYGEN)
#include <android/asset_manager_jni.h>
/**
 * Wraps a stream around an Android asset.
 * Creates a new read-only ::SnsrStream on @p filename in the compressed assets
 * section of an Android application.
 * @p filename is relative to `assets/` and should not include this as part
 * of the name.

 * @note Available on Android SDKs only.
 * @param[in] mgr      Android [AAssetManager][1] object.
 * @param[in] filename path to the asset, relative to the `assets/` directory.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 * [1]: https://developer.android.com/ndk/reference/group___asset.html
 */
SnsrStream
snsrStreamFromAsset(AAssetManager *mgr, const char *filename);
#endif

#include <stdio.h>

/**
 * Wraps a stream around a FILE handle.
 * Creates a new ::SnsrStream from on a @c &lt;stdio.h&gt;
 * @c FILE @c * handle.
 * Calls to snsrStreamClose() on this handle will not close the
 * wrapped @c FILE @c *.
 * This ::SnsrStream type does not support re-opening.

 * Example:
 * @code
 * SnsrStream out = snsrStreamFromFILE(stderr, SNSR_ST_MODE_WRITE);
 * snsrStreamOpen(out);
 * snsrStreamPrint(out, "hello world\n");
 * snsrRelease(out);
 * @endcode
 * @note Requires @c FILE support in the standard C library.
 * @param[in] handle @c FILE @c *, typically @c stdout or @c stderr.
 * @param[in] mode   open for reading or writing, but not both.
 * @return A new ::SnsrStream handle or @c NULL if memory allocation failed.
 */
SnsrStream
snsrStreamFromFILE(FILE *handle, SnsrStreamMode mode);

/**
 * Wraps a stream around a stdio file.
 * Creates a new ::SnsrStream using the @c &lt;stdio.h&gt; file functions.
 *
 * Only a subset of the @c fopen() modes are supported:
 *   - @c r for read-only access.
 *   - @c w for write-only access, truncating the file to zero on open.
 *   - @c a for write-only append access.
 *
 * This stream type supports re-opening. The behaviour follows that of
 * @c fopen() with the given access mode.
 *
 * @note Requires @c FILE support in the standard C library.
 * @param[in] filename the name of a file to open.
 * @param[in] mode     @c r, @c w, or @c a.
 * @return A new ::SnsrStream handle or @c NULL if memory allocation failed.
 */
SnsrStream
snsrStreamFromFileName(const char *filename, const char *mode);

/**
 * Streams live audio from an audio capture device.
 * Creates a new ::SnsrStream attached to the specified audio device.
 *
 * @note Supports audio capture with [ALSA][1] on Linux, with
 * [Audio Queue Services][2] on macOS and iOS, and with
 * the [Windows Multimedia Extentions][3] Wave API on Windows.
 *
 * #SNSR_ST_AF_DEFAULT uses the default capture device with the format
 * (16-bit little-endian LPCM) and sample rate (16 kHz) required by most
 * SDK recognizers. This is the recommended format specifier.
 *
 * Example:
 * @code
 *  // Use a specific ALSA device, rather than the default.
 *  // The "plughw" device typically includes sample rate conversion,
 *  // use this if the hardware does not support sampling at 16 kHz.
 *  SnsrStream a = snsrStreamFromAudioDevice(SNSR_ST_AF_DEVICE, "plughw:1,0");
 * @endcode
 *
 * @param[in] format Audio device selector and format specifier.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 * @see @ref alsa-stream.c, @ref wmme-stream.c
 *
 * [1]: http://www.alsa-project.org/main/index.php/ALSA_Library_API
 * [2]: https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/AudioQueueProgrammingGuide/Introduction/Introduction.html
 * [3]: https://msdn.microsoft.com/en-us/library/dd743847(v=vs.85).aspx
 */
SnsrStream
snsrStreamFromAudioDevice(SnsrStreamAudioFormat format, ...);

/**
 * Converts stream audio format.
 * Creates a new ::SnsrStream on an existing stream, @p a. The new
 * stream produces audio in @p format, converting as needed.
 *
 * The audio format header in @p a will be read when the stream is first opened.
 * * If this is a known, but unsupported format, snsrStreamOpen() will report
 *   an error.
 * * If the format is not known, the data in @p a will be treated as
 *   a headerless little-endian 16-bit LPCM stream.
 *
 * The initial implementation supports the 16-bit LPCM RIFF WAVE format,
 * at 16 kHz sampling rate, for a single channel only.
 *
 * Example:
 * @code
 * SnsrStream a = snsrStreamFromFileName("hbg.wav", "r");
 * a = snsrStreamFromAudioStream(a, SNSR_ST_AF_DEFAULT);
 * snsrStreamOpen(a);
 * // read 16-bit PCM data from a
 * snsrRelease(a);
 * @endcode
 *
 * @note When this stream type is used for output, all stream data will
 * be buffered in memory until the stream is closed. The audio header and
 * the data will then be written to @c a, the wrapped SnsrStream.
 *
 * @param[in] a source ::SnsrStream.
 * @param[in] format output target audio format.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromAudioStream(SnsrStream a, SnsrStreamAudioFormat format, ...);

/**
 * Wraps a stream around a circular buffer.
 * Creates a new ::SnsrStream on a circular buffer with first-in-first-out
 * semantics.
 * The initial buffer will be large enough to hold @p initialSizeInBytes
 * `char`s, and will dynamically grow up to @p maxSizeInBytes.
 * If the ::SnsrStream the bufffer cannot be expanded, snsrStreamWrite() will
 * set the ::SNSR_RC_EOF condition on the stream.
 * This ::SnsrStream type does not support re-opening.
 * @note snsrStreamRead() on streams of this type has non-blocking behavior,
 *  and will return #SNSR_RC_EOF if the buffer runs empty.
 *
 * @param[in] initialSizeInBytes the minimum size of the allocated ring buffer.
 * @param[in] maxSizeInBytes the limit the buffer is allowed to expand to.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromBuffer(size_t initialSizeInBytes, size_t maxSizeInBytes);

/**
 * Wraps a stream around C code.
 * Creates a new ::SnsrStream from an opaque ::SnsrCodeModel handle.
 *
 * The ::SnsrCodeModel handle is an identifier loaded from an object file,
 * compiled from C code generated by a call to snsrSave() with a
 * #SNSR_FM_SOURCE format argument. These data are typically mapped to
 * the read-only TEXT (code) segment.
 *
 * Use this on platforms where the TEXT segment is read from ROM to reduce
 * the amount of RAM a model needs to run.
 *
 * Example:
 * @code
 * extern SnsrCodeModel snsrmodel;
 * SnsrSession s;
 * snsrNew(&s);
 * snsrLoad(s, snsrStreamFromCode(snsrmodel));
 * @endcode
 *
 * @param[in] identifier ::SnsrCodeModel handle.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 * @see snsrSave(), #SNSR_FM_SOURCE.
 */
SnsrStream
snsrStreamFromCode(SnsrCodeModel identifier);


/**
 * Wraps a stream around a memory segment.
 * Creates a new ::SnsrStream on a memory segment starting at @p buffer.
 * ::SnsrStream operations will be limited to the first @p bufferSize bytes
 * of this segment.
 * This ::SnsrStream type does not support re-opening.
 *
 * Example:
 * @code
 * char *data = malloc(1000);
 * SnsrStream b = snsrStreamFromMemory(data, 1000, SNSR_ST_MODE_WRITE);
 * snsrStreamOpen(b);
 * snsrStreamPrint(b, "hello world, data=%p\n", data);
 * snsrRelease(b);
 * printf("wrote: %s\n", data);
 * free(data);
 * @endcode
 * @param[in] buffer pointer to a memory segment.
 * @param[in] bufferSize size of the memory segment the stream has access to.
 * @param[in] mode  open for reading or writing, but not both.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromMemory(void *buffer, size_t bufferSize, SnsrStreamMode mode);

/**
 * Wraps a stream around an open stream.
 * Creates a new ::SnsrStream on an existing, open ::SnsrStream.
 * The new stream mode (read or write) matches the @p source stream.
 * Read and write operations are passed through to the @p source stream, but
 * the number of bytes read or written is limited to @p sizeInBytes.
 * In- and output operations will set the stream status to #SNSR_RC_EOF
 * when this limit is reached.
 *
 * This function will retain @p source, and release it when the stream
 * is closed. Be sure to snsrRetain() the @p source handle if it is to
 * remain valid.

 * Closing this ::SnsrStream will not close @p source.
 *
 * @param[in] source source stream.
 * @param[in] sizeInBytes new stream read or write limit.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromOpenStream(SnsrStream source, size_t sizeInBytes);

/**
 * Concatenates two streams.
 * Creates a new ::SnsrStream that is a concatenation of @p a and @p b.
 *
 * Streams @p a and @p b will be each be opened exactly once. These should
 * be unopened (snsrStreamGetMeta() for #SNSR_ST_META_OPEN_COUNT must be `0`)
 * when snsrStreamFromStreams() is called.
 * This limitation does not apply to streams returned by this function,
 * these can be nested to arbitrary depth to concatenate more than two
 * streams.
 *
 * The new stream will read from (or write to) @p a until end-of-stream
 * is reached, then switch to stream @p b until that is exhausted,
 * at which point #SNSR_RC_EOF is returned.
 *
 * Streams @p a and @p b must have identical modes. If one stream was opened
 * for reading, and the other for writing, this function will set the
 * error code to #SNSR_RC_WRONG_MODE.
 *
 * Closing this stream will close the current source stream. The next
 * open call will open the next source stream. When no further source
 * streams remain snsrStreamOpen() returns #SNSR_RC_CANNOT_REOPEN.
 *
 * Example:
 * @code
 * SnsrStream s;
 * s = snsrStreamFromStreams(
 *       snsrStreamFromFileName("one.txt", "w"),
 *       snsrStreamFromStreams(snsrStreamFromFileName("two.txt", "w"),
 *                             snsrStreamFromFILE(stdout, SNSR_ST_MODE_WRITE)));
 *  snsrStreamOpen(s);
 *  snsrStreamPrint(s, "This writes to \"one.txt\"\n");
 *  snsrStreamClose(s);
 *  snsrStreamOpen(s);
 *  snsrStreamPrint(s, "This writes to \"two.txt\"\n");
 *  snsrStreamClose(s);
 *  snsrStreamOpen(s);
 *  snsrStreamPrint(s, "This writes to stdout\n");
 *  snsrRelease(s);
 *  @endcode
 *
 * @param[in] a first source stream.
 * @param[in] b second source stream.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromStreams(SnsrStream a, SnsrStream b);

/**
 * Wraps a stream around a C string.
 * Creates a new read-only ::SnsrStream from a zero-terminated C string.
 *
 * Equivalent to:
 * @code
 * snsrStreamFromMemory(string, strlen(string), SNSR_ST_MODE_READ);
 * @endcode
 * This stream type does not support re-opening.
 *
 * @param[in] string a `\0`-terminated C string.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamFromString(const char *string);

/**
 * Inserts an event marker into a stream.
 * Creates a new ::SnsrStream that is readable, but provides no data.
 * Any read or skip operation will immediately return end-of-stream.
 *
 * Calls `e(b, data)` when ::SnsrStream `b` is opened, unless `e == NULL`.
 *
 * Calls `r(data)` when `b` is released, unless `r == NULL`.
 * This function should release the `data` handle.
 *
 * Use this stream type with snsrStreamFromStreams() to embed
 * events in a stream.
 *
 * @param[in]     e the function to call when this stream is opened. Must not
 *                  be `NULL`.
 * @param[in]     r a function that releases @p data when this stream is
 *                  deallocated. Use `NULL` if no clean-up is required.
 * @param[in,out] data user pointer, the @p data parameter of ::SnsrStreamEvent.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStreamRaise(SnsrStreamEvent e, SnsrDataRelease r, void *data);

/** @} stream-provider */

/** @addtogroup stream-spi Provider API
 * This section describes the Service Provider Interface used
 * to add new ::SnsrStream types.
 *
 * Each ::SnsrStream type exposes a single API function used to create a
 * new instance of this type. This constructor function sets up the required
 * data structures, copies function arguments into these structures and then
 * calls snsrStream_alloc() to allocate a new ::SnsrStream, which it returns.
 *
 * The constructor function must **not** open the underlying data stream. The
 * SnsrStream_Vmt::open() callback function will be used to do this instead.
 * Delaying the open operation allows for function compositions, such as a
 * ::SnsrStream that concatenates other ::SnsrStream s.
 *
 * @{
 */

/* SnsrStream plug-in SPI */
/** SnsrStream virtual method table.
 * This method table defines stream behaviour when used as an argument to
 * snsrStream_alloc(). This struct must remain valid for the entire life of
 * all the streams of this type. Static allocation is recommended.
 *
 * When one of these functions return (open, close) or set (read, write)
 * a ::SnsrRC error code other than #SNSR_RC_OK, and additional
 * error detail is available, it should also set a detailed human-readable
 * error message using snsrStream_setDetail().
 *
 * Error codes must be in the range
 * from #SNSR_RC_OK to #SNSR_RC_FORMAT_NOT_SUPPORTED.
 */
typedef struct {
  /** The name of the stream type, terminated with \\0.
   *  This field is required and must not be NULL.
   */
  const char *name;
  /** Opens the underlying data stream.
   *  - Called in response to a snsrStreamOpen() API call.
   *  - Will only be called on closed SnsrStream handles.
   *  - Must open the underlying stream in binary mode, unless documented
   *    otherwise.
   *  - Must return #SNSR_RC_OK if the operation succeeded, and an
   *    error code if the resource could not be opened.
   */
  SnsrRC (*open)(SnsrStream b);
  /** Closes the underlying data stream.
   *  - Called in response to a snsrStreamClose() API call.
   *  - Will only be called on open SnsrStream handles.
   *  - Buffered output must be flushed.
   *  - Buffered input must be discarded.
   *  - Must return #SNSR_RC_OK if the operation succeeded,
   *    and an error code if the underlying resource could not be closed.
   */
  SnsrRC (*close)(SnsrStream b);
  /** Releases private SnsrStream data.
   *  - Called just before the SnsrStream handle is de-allocated.
   *  - The handle will have been closed when this function is called -
   *    there is no need to check.
   *  - Must release resources allocated by the type’s creation function.
   */
  void (*release)(SnsrStream b);
  /** Reads data and makes it available as a SnsrStream.
   *  - Called in response to a snsrStreamRead() API call.
   *  - The handle will be open when this function is called and
   *    @p sizeInBytes @c > @c 0.
   *  - Must read @p sizeInBytes bytes into the buffer pointed to by data.
   *  - The read must block (i.e. not return) until all the requested data
   *    have been read, or an error is encountered.
   *  - Must return the number of actual bytes read.
   *  - This number of bytes read must be @p sizeInBytes, unless:
   *      * An error occurred, in which case snsrStream_setRC(b, <error>)
   *        with an appropriate error code must be called.
   *      * The end of the stream was reached, in which case
   *        snsrStream_setRC(b, SNSR_RC_EOF) must be called.
   */
  size_t (*read)(SnsrStream b, void *data, size_t sizeInBytes);
  /** Writes data to the underlying data stream.
   *   - Called in response to a snsrStreamWrite() API call.
   *   - The handle will be open when this function is called
   *     and @p sizeInBytes @c > @c 0.
   *   - Must write @p sizeInBytes bytes, reading them from the buffer
   *     pointed to by data.
   *   - The write must block (i.e. not return) until all of the
   *     requested data have been written to the underlying stream,
   *     or an error is encountered.
   *   - Must return the number of bytes actually written.
   *   - This number of bytes must be @p sizeInBytes, unless an error occurred,
   *     in which case snsrStream_setRC(b,) must be called with an
   *     appropriate error code.
   */
  size_t (*write)(SnsrStream b, const void *data, size_t sizeInBytes);
} SnsrStream_Vmt;

/**
 * Allocates a new ::SnsrStream.
 * Called to allocate a new ::SnsrStream data structure, typically in
 * the type-specific creation function.
 *
 * The behaviour of the new ::SnsrStream type is defined by the @p def
 * virtual method table.
 *
 * @p data is an arbitrary pointer that can be retrieved from the
 * ::SnsrStream handle with snsrStream_getData(). It is typically used
 *  to hold instance-specific data.
 *
 * @param[in] def virtual method table.
 * @param[in] data user pointer.
 * @param[in] readable `1` if the stream can be read from `0` if not.
 * @param[in] writable `1` if the stream can be written to, `0` if not.
 * @return A new ::SnsrStream handle or `NULL` if memory allocation failed.
 */
SnsrStream
snsrStream_alloc(SnsrStream_Vmt *def, void *data, int readable, int writable);

/**
 * Gets the user data pointer from a stream.
 * Returns the @p data parameter passed to snsrStream_alloc().
 *
 * @param[in] b ::SnsrStream handle.
 * @return The @p data pointer for this stream instance.
 */
void *
snsrStream_getData(SnsrStream b);

/**
 * Gets the virtual method table from a stream.
 * Returns the ::SnsrStream_Vmt @p *def parameter passed to snsrStream_alloc().
 *
 * @param[in] b ::SnsrStream handle.
 * @return The @p def ::SnsrStream_Vmt virtual method table pointer for this
 *         stream instance.
 */
SnsrStream_Vmt *
snsrStream_getVmt(SnsrStream b);

/**
 * Sets stream return code.
 * Sets the return code for ::SnsrStream @p h to @p code.
 *
 * Only a subset of the ::SnsrRC return codes are valid for streams,
 * @p code must be in the range from
 * #SNSR_RC_OK to #SNSR_RC_FORMAT_NOT_SUPPORTED.
 *
 * @param[in] h ::SnsrStream handle.
 * @param[in] code Return code, #SNSR_RC_OK for success.
 */
void
snsrStream_setRC(SnsrStream h, SnsrRC code);

/**
 * Sets a detailed stream error message.
 * Sets the detailed human-readable error message in stream @p b to @p format,
 * which a standard C library `fprintf()` format string.
 *
 * This detailed error message is made available through the
 * snsrStreamErrorDetail() API call.
 *
 * @param[in] b ::SnsrStream handle.
 * @param[in] format `fprintf()` stype format string.
 * @param[in] ... arguments specified by the @p format string.
 */
void
snsrStream_setDetail(SnsrStream b, const char *format, ...);

/** @} stream-spi */
/** @} stream     */


/** @addtogroup session Session
 * @{
 */

/**
 * Clears the ::SnsrSession error code.
 * Once a ::SnsrSession error occurs (i.e. a return code other than
 * #SNSR_RC_OK is reported), all library calls on the session handle will
 * immediately return with the same error code.
 *
 * Call this function to clear the error detail and
 * reset the code to #SNSR_RC_OK.
 *
 * @param[in] s session handle.
 */
void
snsrClearRC(SnsrSession s);

/**
 * Sets the detailed session error message.
 * Sets the detailed error message returned by snsrErrorDetail().
 * Use this to propgate a human-readable error message generated
 * by a callback function to the session handle.
 * @param[in] s session handle.
 * @param[in] format `fprintf()` style format string.
 * @param[in] ... arguments specified by the @p format string.
 */
void
snsrDescribeError(SnsrSession s, const char *format, ...);

/**
 * Duplicates a session handle.
 * Creates a new ::SnsrSession handle that is a clone of a source handle.
 * This function is roughly equivalent to:
 * @code
 * SnsrStream b = snsrStreamFromBuffer(1024, 1073741824);
 * snsrSave(s, SNSR_FM_CONFIG, b);
 * snsrLoad(*dst, b);
 * snsrRelease(b);
 * @endcode
 * The new ::SnsrSession @p dst contains all of the configuration from @p s,
 * but none of the runtime settings. Immutable configuration settings
 * (such as acoustic models) are shared between @p s and @p dst. Using snsrDup()
 * to create two runtime instances of a task will therefore use less memory
 * than loading the same task data into two ::SnsrSession handles.
 *
 * @param[in]  s source session handle.
 * @param[out] dst pointer to an allocated ::SnsrSession handle which will
 *                 receive a clone of @p s.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrDup(SnsrSession s, SnsrSession *dst);

/**
 * Retrieves a detailed session error message.
 * This human-readable error message should be used
 * for display or logging only. The content of the message is system-specific.
 * Do not parse this to determine program flow, use the ::SnsrRC return
 * code instead.
 * @param[in] s session handle.
 * @return A detailed message describing the most recent reason for failure.
 *         The memory pointed to is owned by this library and must not be
 *         released. It is not reference-counted, calling snsrRetain() or
 *         snsrRelease() on this handle will panic the heap allocator.
 *         The handle remains valid only until the next API call on @p s.
 */
const char *
snsrErrorDetail(SnsrSession s);

/**
 * Calls a user function for each iteration over a list.
 * Invokes @p c for each iteration over setting @p key.
 * @p key must reference an iterator setting. These are task-specific -
 * see the @ref task documentation for a list of available iterators.
 *
 * If @p c returns an error (i.e. not #SNSR_RC_OK) the iteration
 * loop is terminated early, and snsrForEach() returns this result code.
 *
 * @param[in] s source session handle.
 * @param[in] key iterator key, see task documentation.
 * @param[in] c a ::SnsrCallback invoked for each iteration.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrForEach(SnsrSession s, const char *key, SnsrCallback c);

/**
 * Retrieves a double value from a session setting.
 * Returns the `double` value set for @p key in ::SnsrSession @p s.
 * The @p key and @p value arguments must not be `NULL`.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @param[in]  s session handle.
 * @param[in]  key setting key.
 * @param[out] value `double` value associated with @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrGetInt(), snsrSetDouble().
 */
SnsrRC
snsrGetDouble(SnsrSession s, const char *key, double *value);

/**
 * Retrieves an int value from a session setting.
 * Returns the `int` value set for @p key in ::SnsrSession @p s.
 * The @p key and @p value arguments must not be `NULL`.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @param[in]  s session handle.
 * @param[in]  key setting key.
 * @param[out] value `int` value associated with @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrGetDouble(), snsrSetInt().
 */
SnsrRC
snsrGetInt(SnsrSession s, const char *key, int *value);

/**
 * Retrieves a stream handle from a runtime session setting.
 * Returns the ::SnsrStream value set for @p key in ::SnsrSession @p s.
 * The @p key and @p stream arguments must not be `NULL`.
 *
 * By default, the returned @p stream ::SnsrStream handle is valid only until
 * the next library call on the @p s ::SnsrSession handle. This validity can
 * be extended by retaining the handle with snsrRetain(). Be sure to
 * (eventually) release retained handles or a memory leak will result.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @note This function retrieves runtime settings (such as recognition audio or
 * enrolled phrase spot modules) only. It cannot be used to get the stream
 * handles set with snsrStreamSet().
 *
 * @param[in]  s session handle.
 * @param[in]  key setting key.
 * @param[out] stream ::SnsrStream handle associated with @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrGetStream(SnsrSession s, const char *key, SnsrStream *stream);

/**
 * Retrieves a string value from a runtime session setting.
 * Returns the string value for @p key in ::SnsrSession @p s.
 * The @p key and @p value arguments must not be `NULL`.
 *
 * By default, the returned @p value string is valid only until
 * the next library call on the @p s ::SnsrSession handle. This validity can
 * be extended by retaining the handle with snsrRetain(). Be sure to
 * (eventually) release retained handles or a memory leak will result.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @param[in]  s session handle.
 * @param[in]  key setting key.
 * @param[out] value C string value associated with @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrGetString(SnsrSession s, const char *key, const char **value);

/**
 * Loads a task model into a session.
 * Loads a @ref task or configuration settings from a ::SnsrStream
 * into ::SnsrSession @p s. The stream data must be in the format produced
 * by snsrSave().
 * This function retains a reference to @p inputStream on entry
 * and releases it on exit. @p inputStream will be opened, but will not
 * be closed explicitly. Streams are closed before deallocation, so
 * @p inputStream would be closed if no other references to it are held.
 *
 * @param[in]  s session handle.
 * @param[in]  inputStream readable ::SnsrStream that contains a task model.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrLoad(SnsrSession s, SnsrStream inputStream);

#ifndef SNSR_USE_SUBSET
/**
 * Creates a new ::SnsrSession handle.
 * Session handles contain the entire state for a task.
 *
 * If a ::SnsrSession handle has to be shared across threads, all calls into
 * this library that use the handle **must** be proctected by application-level
 * thread synchronization calls. The ::SnsrSession API calls are not re-entrant.
 *
 * Best practice is to use a single thread per handle. Using multiple handles
 * per thread is fully supported.
 *
 * snsrNew() is implemented as a macro, which validates the included
 * snsr.h against the one that the archive library was built with.
 * If these do not match, snsrNew() fails and returns #SNSR_RC_LIBRARY_HEADER.
 *
 * @param[out] s a pointer to an allocated ::SnsrSession handle.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 *         If @p s is not @c NULL a detailed error message can be retrieved with
 *         snsrErrorDetail().
 * @see snsrDup().
 */
# define snsrNew(s) snsrNewValidateVersion((s), SNSR_VERSION)
#else
# define snsrNew(s) snsrNewSubset((s), SNSR_VERSION)
#endif

SnsrRC
snsrNewSubset(SnsrSession *s, const char *version);

SnsrRC
snsrNewValidateVersion(SnsrSession *s, const char *version);


/**
 * Validates task requirements.
 * Checks that the value for the @p key, for the task loaded into @p s,
 * matches the required value @p value.
 *
 * If @p key is #SNSR_TASK_VERSION, this function uses [semantic version][1]
 * precedence to determine whether the task version meets the @p value
 * version requirement.
 *
 * If @p key is #SNSR_TASK_TYPE_AND_VERSION_LIST, @p version is a list of
 * acceptable #SNSR_TASK_TYPE and #SNSR_TASK_VERSION tuples.
 *
 * @p value is a semantic version @b range:
 *  <ul>
 *    <li> A @b range is composed of one or more @b sets, joined by @c " || ".
 *         A version matches a @b range if, and only if, every @b comparator
 *         in at least one of the @c " || " -separated @b sets is satisfied
 *         by the version.
 *    <li> A @b set is a list of @b comparators separated by whitespace.
 *         A @b set is satisfied by the _intersection_ of all the @b comparators
 *          it includes.
 *    <li> A @b comparator is an @b operator followed by a version.
 *    <li> @b operators include:
 *    <ul>
 *      <li> @c =, @c <, @c <=, @c >, @c >=: range comparisons.
 *      <li> @c ~: `taskVersion >= value && taskVersion < M` where M is
 *           the next major version number.
 *      <li> @c ^: `taskVersion >= value && taskVersion < M` where M is
 *           the next major version number,
 *           skipping over zeros from the left. For `taskVersion >= 1.0.0`
 *           this is equivalent to the @c ~ operator. `^0.minor.patch`
 *           allows only patch updates, e.g. `^0.1.1` will
 *           accept `0.1.2` but not `0.2.0`.
 *      <li> The default behavior when no operator is specified is @c ^.
 *    </ul>
 *  </ul>
 *
 * This function is used as a sanity check, to verify that the task model
 * loaded is of the correct type, and that it has a compatible version.
 *
 * Example:
 * @code
 * snsrLoad(s, snsrStreamFromFileName("spotter.snsr", "r"));
 * snsrRequire(s, SNSR_TASK_TYPE, SNSR_PHRASESPOT);
 * snsrRequire(s, SNSR_TASK_VERSION, "1.0.0");
 * snsrRequire(s, SNSR_TASK_VERSION, "0.5.0 || 1.0.0");
 * snsrRequire(s, SNSR_TASK_TYPE_AND_VERSION_LIST,
 *             SNSR_PHRASESPOT " ~0.5.0 || 1.0.0;"
 *             SNSR_PHRASESPOT_VAD " 1.2.3");
 *
 * @endcode
 *
 * @param[in] s session handle.
 * @param[in] key the name of a setting to validate.
 * @param[in] value string to match the @p key setting against.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * [1]: http://semver.org/
 */
SnsrRC
snsrRequire(SnsrSession s, const char *key, const char *value);


/**
 * Resets a ::SnsrSession to its initial state.
 * This call resets the session error code to #SNSR_RC_OK,
 * clears the error detail, discards all pending results,
 * clears internal buffers, and resets session time and sample counts to @c 0.
 *
 * snsrReset() is most useful to clear the state of a template model
 * (such as @ref tpl-spot-vad) that was stopped through a callback.
 *
 * @note Call snsrReset() only after snsrRun() has returned.
 * Invoking snsrReset() in a callback handler will lead to undefined behavior.
 *
 * @since TrulyHandsfree SDK 6.10.
 *
 * @param[in] s session handle.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrReset(SnsrSession s);


/**
 * Processes stream data in a session.
 * Enters the main ::SnsrSession processing loop. This function will
 * process data read from input streams, invoke
 * @ref callback registered in the session and (if defined) write to
 * output streams.
 *
 * Processing will continue until:
 *  * one of the data streams reach #SNSR_RC_EOF, in which case snsrRun()
 *    returns #SNSR_RC_STREAM_END, or
 *  * one of the invoked callbacks returns an error code, which snsrRun()
 *    will return. #SNSR_RC_INTERRUPTED, #SNSR_RC_REPEAT, #SNSR_RC_SKIP,
 *    #SNSR_RC_STOP or #SNSR_RC_TIMED_OUT should be used to indicate
 *    that the operation was stopped on request.
 *
 * @ref callback are invoked on the thread that snsrRun() was
 * called on. The processing loop is single-threaded, so it stalls while
 * waiting for user functions to return.
 *
 * snsrRun() will flush the internal recognition pipeline an input stream
 * reaches #SNSR_RC_EOF, unless #SNSR_AUTO_FLUSH is set to @c 0.
 *
 * @param[in] s session handle.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrSetHandler(), snsrSetStream(), #SNSR_AUTO_FLUSH.
 */
SnsrRC
snsrRun(SnsrSession s);

/**
 * Retrieves the session return code.
 * Returns the most recent ::SnsrRC return code from ::SnsrSession @p s.
 *
 * Use snsrClearRC() to reset the session error state to #SNSR_RC_OK.
 *
 * @param[in] s session handle.
 * @return The current @p s ::SnsrRC return code.
 * @see snsrClearRC(), snsrErrorDetail(), snsrRCMessage().
 */
SnsrRC
snsrRC(SnsrSession s);

/**
 * Describes a return code.
 * Provides a human-readable error message describing @p returnCode.
 * This message should be used for display or logging only. The content
 * of the message is unspecified and system-specific. Do not parse this
 * to determine program flow, use the ::SnsrRC return code instead.
 *
 * If a ::SnsrSession handle is available, consider using snsrErrorDetail()
 * instead, as this provides additional details.
 *
 * @param[in] returnCode ::SnsrRC returned by one of the API functions.
 * @return A character string describing the @p returnCode.
 *         The memory pointed to is owned by this library and must not be
 *         released. It is not reference-counted, calling snsrRetain() or
 *         snsrRelease() on this handle will panic the heap allocator.
 */
const char *
snsrRCMessage(SnsrRC returnCode);

/**
 * Retains a reference to an object.
 * Increments the reference count on @p handle. The @p handle will remain
 * valid until snsrRelease() is called on it.
 *
 * To avoid memory leaks, each call to snsrRetain() must be matched with
 * a corresponding call to snsrRelease().
 *
 * @note @p handle must be a valid reference-counted object returned by
 *  this library, such as ::SnsrSession, ::SnsrStream, ::SnsrCallback
 *  or a `const char *` returned snsrGetString().
 *  Behaviour on any other pointer is undefined and will likely
 *  lead to the process calling `abort()`.
 *
 * @param[in] handle a reference-counted handle,
 */
void
snsrRetain(const void *handle);

/**
 * Releases an object reference.
 * Decrements the reference count on @p handle. If the reference count
 * drops to zero, the @p handle will be deallocated.
 *
 * When this function returns @p handle will no longer be valid. Behaviour
 * is undefined if it is used as an argument to any of the functions in this
 * library.
 *
 * @note @p handle must be `NULL`, or a valid reference-counted object
 *  returned by this library, such as ::SnsrSession, ::SnsrStream,
 * ::SnsrCallback or a `const char *` returned snsrGetString().
 *  Behaviour on any other pointer is undefined and will likely
 *  lead to the process calling `abort()`.
 *
 * @param[in] handle a reference-counted handle, or `NULL`.
 */
void
snsrRelease(const void *handle);

/**
 * Serializes a session to a stream.
 * Saves the configuration or runtime settings of ::SnsrSession @p s to
 * writable ::SnsrStream @p outputStream.
 *
 * ::SnsrDataFormat specifies what is serialized:
 *   - #SNSR_FM_CONFIG serializes the task and all of its configuration
 *     settings.
 *   - #SNSR_FM_RUNTIME serializes runtime settings, such as
 *     phrase spotter enrollments in #SNSR_ENROLL tasks.
 *   - #SNSR_FM_SOURCE serializes the task and all of its configuration
 *     settings as compilable C code. Use this on platforms where
 *     code is directly read from ROM to reduce RAM requirements.
 *     See snsrStreamFromCode(), #SNSR_TAG_IDENTIFIER, #SNSR_MODEL_NAME.
 *     Code is TrulyHandsfree SDK version specific. If you upgrade the
 *     TrulyHandsfree SDK, also update any generated C source files.
 *
 * @param[in] s session handle.
 * @param[in] format ::SnsrDataFormat specifies the output format.
 * @param[in] outputStream writable destination stream.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrSave(SnsrSession s, SnsrDataFormat format, SnsrStream outputStream);

/**
 * Changes session configuration.
 * This untility function parses the @p keyValue string and sets
 * ::SnsrSession @p s configuration options. Use this to change task
 * configuration from user-supplied configuration strings provided to
 * a command-line application.
 *
 * @p keyValue should be of the form `"key=value ..."`, e.g. `"delay=30"` or
 * `"accuracy=0.5 delete-user=hbg"`.
 *
 * Use snsrSetDouble(), snsrSetInt() and snsrSetString() when the key
 * name is known at compile time, as these functions do not incur a
 * string parsing overhead.
 *
 * @param[in] s session handle.
 * @param[in] keyValue setting string, `key=value ...`.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrSetDouble(), snsrSetInt(), snsrSetString().
 */
SnsrRC
snsrSet(SnsrSession s, const char *keyValue);

/**
 * Sets a session configuration to a `double` value.
 * Sets the @p key in ::SnsrSession @p s to @p value.
 * @p key must not be `NULL`.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @param[in] s session handle.
 * @param[in] key setting key.
 * @param[in] value `double` value for @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrGetDouble(), snsrSetInt().
 */
SnsrRC
snsrSetDouble(SnsrSession s, const char *key, double value);


/**
 * Sets a session callback handler.
 * Registers callback @p c for @p key in ::SnsrSession @p s.
 * This callback will be invoked as specified in the task documentation.
 *
 * See @ref task for details on task-specific callback keys.
 *
 * @param[in] s session handle.
 * @param[in] key setting key.
 * @param[in] c a ::SnsrCallback instance, a user function with private data.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrSetHandler(SnsrSession s, const char *key, SnsrCallback c);

/**
 * Sets a session configuration to an `int` value.
 * Sets the @p key in ::SnsrSession @p s to @p value.
 * @p key must not be `NULL`.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.
 *
 * @param[in] s session handle.
 * @param[in] key setting key.
 * @param[in] value `int` value for @p key.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrGetInt(), snsrSetDouble().
 */
SnsrRC
snsrSetInt(SnsrSession s, const char *key, int value);

/**
 * Sets input and output streams for a session.
 * Associates @p stream with the source or sink specified by @p key.
 * Stream keys are task-specific and are described in @ref task.
 *
 * Example:
 * @code
 * snsrSetStream(s, SNSR_SOURCE_AUDIO_PCM,
 *               snsrStreamFromFileName("input.pcm", "r"));
 * @endcode
 *
 * @param[in] s session handle.
 * @param[in] key setting key.
 * @param[in] stream a task-specific input or output stream.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 * @see snsrRun().
 */
SnsrRC
snsrSetStream(SnsrSession s, const char *key, SnsrStream stream);

/**
 * Sets a session configuration to a string value.
 * Sets the @p key in ::SnsrSession @p s to @p value.
 * @p key must not be `NULL`.
 *
 * See @ref task for a list of keys and the contexts
 * in which they are available.

 * @param[in] s session handle.
 * @param[in] key setting key.
 * @param[in] value C string for @p key. This value is copied into the
 *   @p s session handle.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrSetString(SnsrSession s, const char *key, const char *value);

/** @} session */

/**
 * @addtogroup config Library Configuration
 * API functions and settings that control overall TrulyHandsfree
 * SDK behavior.
 * @{
 */

/**
 * Releases library-wide resources.
 * Resets the TrulyHandsfree SDK library to the initial defaults and releases
 * all library-wide resources.
 * Calling this function is optional. It is most useful to suppress false
 * positives when tracking down heap leaks with a tool such as [Valgrind][].
 * @note Do **not** call this function unless all SDK handles
 * (::SnsrSession, ::SnsrStream etc.) have been released. Doing so will lead to
 * undefined behavior.
 *
 * [Valgrind]: http://valgrind.org
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrTearDown(void);

/**
 * TrulyHandsfree Library configuration settings.
 * @see snsrConfig().
 */
typedef enum {
  /**
   * Hardware security device communication.
   * Use if, and only if, recommended by Sensory.
   * @code
   * snsrConfig(SNSR_CONFIG_SECURITY_CHIP, uint32_t *(*comms)(uint32_t *in));
   * @endcode
   */
  SNSR_CONFIG_SECURITY_CHIP
} SnsrConfig;


/**
 * Global library configuration changes.
 * Use this function to change the overall behavior of the TrulyHandsfree SDK.
 *
 * Note:
 *  - Call this function only when no library handles exist: Before any other
 *    API function at start-up, or after a call to snsrTearDown().
 *  - This function is @b not thread-safe.
 *  - The number of arguments expected depend entirely on the @p config
 *    parameter. Failure to provide all the expected arguments with the
 *    expected types will lead to undefined behaviour.
 *
 * @param[in] config configuration type. This type determines what
 *     additional arguments are expected.
 * @return #SNSR_RC_OK for success, any other value indicates failure.
 */
SnsrRC
snsrConfig(SnsrConfig config, ...);

/** @} config */

/* Compatibility functions, for convenience only.
 * These functions are not documented in the SDK API and are not
 * supported in any way.
 */

#if defined(_WIN32) || defined(USE_SNSR_GETOPT)
int
getopt(int argc, char *const argv[], const char *optstr);
#endif

#if defined(__cplusplus)
}
#endif
#endif /* SNSR_H */

/** @} c */
