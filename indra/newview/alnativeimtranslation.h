/** Native, on-device Apple Translation bridge for the per-chat IM experiment.
 * Copyright (C) 2026, Alchemy Viewer Project.
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#ifndef AL_NATIVE_IM_TRANSLATION_H
#define AL_NATIVE_IM_TRANSLATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All calls and completions run on the viewer's main thread. JSON strings are
 * borrowed for the duration of the callback; copy them before returning.
 * Request IDs must be unique across languages and translate requests.
 * Cancellation and shutdown suppress callbacks, including already queued work.
 * No message text is logged by this bridge or sent to a translation server.
 */
typedef void (*ALNativeIMTranslationCallback)(uint64_t request_id, const char* json);

/* Requires macOS 26 for the public headless session initializer. */
int al_native_im_translation_available(void);

/* Success: {ok:true,languages:[{code,name}],system_language:"en-IE"}.
 * Failure: {ok:false,error_code:...,error:...}.
 * Language codes come from Apple's current supportedLanguages list.
 */
void al_native_im_translation_languages(uint64_t request_id, ALNativeIMTranslationCallback callback);

/* Pair readiness for chat controls. The languages result additionally contains
 * outgoing_status and incoming_status on every entry: installed, supported
 * (download needed), unsupported, or identity (same language/script; no model).
 * Outgoing is Write in -> entry; incoming is expected peer -> entry. Empty
 * source resolves the system language; empty peer resolves that source. Result
 * source, peer_source and peer_source_explicit identify these assumptions.
 * system_target is the matched system language; incoming_system_status reports
 * expected peer -> system language without requiring caller locale matching.
 * These are pair statuses, not global per-language installation claims. Query
 * again after setup or on focus to refresh; this function never downloads.
 */
void al_native_im_translation_languages_for_source(uint64_t request_id,
                                                  const char* source_language,
                                                  const char* peer_source_language,
                                                  ALNativeIMTranslationCallback callback);

/* Empty source = NaturalLanguage detection; empty target = first system
 * preferred language. Success: {ok:true,text,source,target}; same language is
 * a successful identity result. Failure: {ok:false,error_code,error,source?,target?}.
 * Supported errors include unavailable, empty_text, unknown_source,
 * unsupported_source, unsupported_target, unsupported_pair, not_installed,
 * translation_failed, service_unavailable. Missing packs never trigger an implicit download.
 */
void al_native_im_translate(uint64_t request_id, const char* text,
                            const char* source_language, const char* target_language,
                            ALNativeIMTranslationCallback callback);

/* Translate only prose from a JSON array of {text:string,protected:bool}.
 * Protected fragments are concatenated byte-identically and never enter the
 * engine or language recognizer. Fragment-edge whitespace is retained. A
 * protected-only message succeeds without consulting language packs. At most
 * 64 nonempty prose fragments are accepted (too_many_segments otherwise).
 * The same result/cancellation contract as al_native_im_translate applies.
 */
void al_native_im_translate_segments(uint64_t request_id, const char* segments_json,
                                     const char* source_language, const char* target_language,
                                     ALNativeIMTranslationCallback callback);

/* Opens a native language-pair setup panel; its Prepare button asks Apple for
 * permission to download missing language packs. Empty source/target defaults
 * to system language / Spanish. The panel never contains chat text.
 */
void al_native_im_translation_setup(const char* source_language, const char* target_language);
void al_native_im_translation_cancel(uint64_t request_id);
void al_native_im_translation_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
