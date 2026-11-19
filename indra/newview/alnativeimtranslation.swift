// Copyright (C) 2026, Alchemy Viewer Project.
// Licensed under the GNU Lesser General Public License version 2.1 only.
// Native Apple Translation experiment. Public SDK APIs only; translation content
// stays on device. Keep the C ABI in sync with alnativeimtranslation.h.
import AppKit
import Foundation
import NaturalLanguage
import SwiftUI
import Translation

typealias ALNativeIMTranslationCompletion = @convention(c) (UInt64, UnsafePointer<CChar>) -> Void

@MainActor
private final class ALNativeIMTranslationOperation {
    var task: Task<Void, Never>?
    var session: AnyObject?
}

@MainActor
private enum ALNativeIMTranslationBridge {
    static var operations: [UInt64: ALNativeIMTranslationOperation] = [:]
    static var stopped = false
    static var setupWindow: NSWindow?
    static let setupDelegate = ALNativeIMTranslationSetupDelegate()

    static var systemLanguage: String { Locale.Language.systemLanguages.first?.minimalIdentifier ?? Locale.preferredLanguages.first ?? Locale.current.identifier }

    static func finish(_ id: UInt64, _ operation: ALNativeIMTranslationOperation, _ callback: ALNativeIMTranslationCompletion?, _ value: [String: Any]) {
        guard !stopped, operations[id] === operation else { return }
        operations.removeValue(forKey: id)
        guard let callback,
              let data = try? JSONSerialization.data(withJSONObject: value, options: [.sortedKeys]),
              let json = String(data: data, encoding: .utf8) else { return }
        json.withCString { callback(id, $0) }
    }

    static func begin(_ id: UInt64) -> ALNativeIMTranslationOperation? {
        guard !stopped, operations[id] == nil else { return nil }
        let operation = ALNativeIMTranslationOperation()
        operations[id] = operation
        return operation
    }

    static func failure(_ code: String, _ message: String, source: String = "", target: String = "") -> [String: Any] {
        ["ok": false, "error_code": code, "error": message, "source": source, "target": target]
    }

    @available(macOS 26.0, *)
    static func availability() -> LanguageAvailability {
        if #available(macOS 26.4, *) {
            return LanguageAvailability(preferredStrategy: .lowLatency)
        }
        return LanguageAvailability()
    }

    @available(macOS 26.0, *)
    static func session(source: Locale.Language, target: Locale.Language) -> TranslationSession {
        if #available(macOS 26.4, *) {
            return TranslationSession(installedSource: source, target: target, preferredStrategy: .lowLatency)
        }
        return TranslationSession(installedSource: source, target: target)
    }

    static func match(_ code: String, in languages: [Locale.Language]) -> Locale.Language? {
        let requested = Locale.Language(identifier: code)
        // A person's region (for example en-IE) does not make English an
        // unsupported translation language. Keep script distinctions such as
        // Simplified/Traditional Chinese while mapping to Apple's canonical ID.
        return languages.first { $0.isEquivalent(to: requested) } ?? languages.first { sameLanguage($0, requested) }
    }

    static func sameLanguage(_ first: Locale.Language, _ second: Locale.Language) -> Bool {
        first.languageCode == second.languageCode && first.script == second.script
    }
}

@MainActor
private final class ALNativeIMTranslationSetupDelegate: NSObject, NSWindowDelegate {
    func windowWillClose(_ notification: Notification) {
        // Detach SwiftUI so its translation task is canceled when the person
        // closes setup, rather than retaining a hidden preparation view.
        ALNativeIMTranslationBridge.setupWindow?.contentView = nil
        ALNativeIMTranslationBridge.setupWindow = nil
    }
}

@_cdecl("al_native_im_translation_available")
func nativeIMTranslationAvailable() -> Int32 {
    if #available(macOS 26.0, *) { return 1 }
    return 0
}

@_cdecl("al_native_im_translation_languages")
@MainActor
func nativeIMTranslationLanguages(_ id: UInt64, _ callback: ALNativeIMTranslationCompletion?) {
    guard let operation = ALNativeIMTranslationBridge.begin(id) else { return }
    operation.task = Task { @MainActor in
        guard #available(macOS 26.0, *) else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unavailable", "Native IM translation requires macOS 26 or later."))
            return
        }
        let languages = await ALNativeIMTranslationBridge.availability().supportedLanguages
        guard !languages.isEmpty else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("service_unavailable", "Apple Translation returned no supported languages. Check the macOS translation service and retry."))
            return
        }
        let entries = languages.map { language in
            ["code": language.minimalIdentifier,
             "name": Locale.current.localizedString(forIdentifier: language.minimalIdentifier) ?? language.minimalIdentifier]
        }.sorted { $0["name", default: ""].localizedStandardCompare($1["name", default: ""]) == .orderedAscending }
        ALNativeIMTranslationBridge.finish(id, operation, callback,
                      ["ok": true, "languages": entries, "system_language": ALNativeIMTranslationBridge.systemLanguage])
    }
}

private struct ALNativeIMTranslationPiece: Decodable {
    let text: String
    let protected: Bool
}

@_cdecl("al_native_im_translate")
@MainActor
func nativeIMTranslate(_ id: UInt64, _ textPointer: UnsafePointer<CChar>?,
                       _ sourcePointer: UnsafePointer<CChar>?, _ targetPointer: UnsafePointer<CChar>?,
                       _ callback: ALNativeIMTranslationCompletion?) {
    let text = textPointer.map { String(cString: $0) } ?? ""
    nativeIMStartTranslation(id, [ALNativeIMTranslationPiece(text: text, protected: false)],
                             sourcePointer.map { String(cString: $0) } ?? "",
                             targetPointer.map { String(cString: $0) } ?? "", callback)
}

@_cdecl("al_native_im_translate_segments")
@MainActor
func nativeIMTranslateSegments(_ id: UInt64, _ jsonPointer: UnsafePointer<CChar>?,
                               _ sourcePointer: UnsafePointer<CChar>?, _ targetPointer: UnsafePointer<CChar>?,
                               _ callback: ALNativeIMTranslationCompletion?) {
    let json = jsonPointer.map { String(cString: $0) } ?? ""
    guard let data = json.data(using: .utf8),
          let pieces = try? JSONDecoder().decode([ALNativeIMTranslationPiece].self, from: data) else {
        guard let operation = ALNativeIMTranslationBridge.begin(id) else { return }
        operation.task = Task { @MainActor in
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("invalid_input", "The protected-message description is invalid. The original message is retained."))
        }
        return
    }
    nativeIMStartTranslation(id, pieces,
                             sourcePointer.map { String(cString: $0) } ?? "",
                             targetPointer.map { String(cString: $0) } ?? "", callback)
}

@MainActor
private func nativeIMStartTranslation(_ id: UInt64, _ pieces: [ALNativeIMTranslationPiece],
                                      _ suppliedSource: String, _ suppliedTarget: String,
                                      _ callback: ALNativeIMTranslationCompletion?) {
    guard let operation = ALNativeIMTranslationBridge.begin(id) else { return }
    let text = pieces.map(\.text).joined()
    let prosePieces = pieces.filter { !$0.protected && !$0.text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    let prose = prosePieces.map(\.text).joined(separator: "\n")
    let targetCode = suppliedTarget.isEmpty ? ALNativeIMTranslationBridge.systemLanguage : suppliedTarget
    operation.task = Task { @MainActor in
        guard #available(macOS 26.0, *) else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unavailable", "Native IM translation requires macOS 26 or later."))
            return
        }
        guard !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("empty_text", "There is no text to translate."))
            return
        }
        guard prosePieces.count <= 64 else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("too_many_segments", "The message has too many separate text fragments to translate. The original message is retained."))
            return
        }
        if prosePieces.isEmpty || prose.rangeOfCharacter(from: .letters) == nil {
            // Links, action URIs and other protected text never enter Apple's
            // engine, even for language identification or model availability.
            ALNativeIMTranslationBridge.finish(id, operation, callback,
                                              ["ok": true, "text": text, "source": suppliedSource, "target": targetCode])
            return
        }
        var sourceCode = suppliedSource
        if sourceCode.isEmpty {
            let recognizer = NLLanguageRecognizer()
            recognizer.processString(prose)
            let hypotheses = recognizer.languageHypotheses(withMaximum: 1)
            guard let detected = recognizer.dominantLanguage,
                  let probability = hypotheses[detected], probability >= 0.55 else {
                ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unknown_source", "Apple could not identify the message language. The original message is retained. Try a longer message, or choose Write in for outgoing translation.", target: targetCode))
                return
            }
            sourceCode = detected.rawValue
        }
        let availability = ALNativeIMTranslationBridge.availability()
        let languages = await availability.supportedLanguages
        guard !Task.isCancelled else { return }
        guard !languages.isEmpty else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("service_unavailable", "Apple Translation returned no supported languages. Check the macOS translation service and retry.", source: sourceCode, target: targetCode))
            return
        }
        guard let source = ALNativeIMTranslationBridge.match(sourceCode, in: languages) else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unsupported_source", "Apple Translation does not support the message language (\(sourceCode)).", source: sourceCode, target: targetCode))
            return
        }
        guard let target = ALNativeIMTranslationBridge.match(targetCode, in: languages) else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unsupported_target", "Apple Translation does not support the selected language (\(targetCode)). Choose another target language.", source: sourceCode, target: targetCode))
            return
        }
        if ALNativeIMTranslationBridge.sameLanguage(source, target) {
            ALNativeIMTranslationBridge.finish(id, operation, callback,
                          ["ok": true, "text": text, "source": source.minimalIdentifier, "target": target.minimalIdentifier])
            return
        }
        let status = await availability.status(from: source, to: target)
        guard !Task.isCancelled else { return }
        guard status != .unsupported else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("unsupported_pair", "Apple Translation does not support this language pair.", source: sourceCode, target: targetCode))
            return
        }
        guard status == .installed else {
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure("not_installed", "Download \(source.minimalIdentifier) and \(target.minimalIdentifier) using Translation setup, or System Settings > General > Language & Region > Translation Languages, then retry.", source: source.minimalIdentifier, target: target.minimalIdentifier))
            return
        }
        let session = ALNativeIMTranslationBridge.session(source: source, target: target)
        operation.session = session
        do {
            var translated = ""
            for piece in pieces {
                guard !Task.isCancelled else { return }
                guard !piece.protected,
                      let first = piece.text.firstIndex(where: { !$0.isWhitespace }),
                      let last = piece.text.lastIndex(where: { !$0.isWhitespace }) else {
                    translated += piece.text
                    continue
                }
                // Apple may trim fragment edges. Retain the original separators
                // around a protected URI instead of attaching prose to its URL.
                translated += piece.text[..<first]
                let result = try await session.translate(String(piece.text[first...last]))
                guard !Task.isCancelled else { return }
                translated += result.targetText
                translated += piece.text[piece.text.index(after: last)...]
            }
            ALNativeIMTranslationBridge.finish(id, operation, callback,
                          ["ok": true, "text": translated, "source": source.minimalIdentifier,
                           "target": target.minimalIdentifier])
        } catch {
            guard !Task.isCancelled else { return }
            let code: String
            let message: String
            switch error {
            case TranslationError.notInstalled:
                code = "not_installed"
                message = "Required language packs are unavailable. Open Translation setup, download both languages, then retry."
            case TranslationError.unableToIdentifyLanguage:
                code = "unknown_source"
                message = "Apple could not identify the message language. The original message is retained. Try a longer message, or choose Write in for outgoing translation."
            default:
                // Do not propagate arbitrary framework error descriptions that
                // could contain the original IM. The C++ caller retains it.
                code = "translation_failed"
                message = "Apple Translation could not translate this message. Retry or turn translation off for this chat."
            }
            ALNativeIMTranslationBridge.finish(id, operation, callback, ALNativeIMTranslationBridge.failure(code, message, source: sourceCode, target: targetCode))
        }
    }
}

@available(macOS 26.0, *)
@MainActor
private struct ALNativeIMTranslationSetup: View {
    @State var source: String
    @State var target: String
    @State private var languages: [Locale.Language] = []
    @State private var configuration: TranslationSession.Configuration?
    @State private var preparing = false
    @State private var status = "Choose both languages, then prepare on-device translation."

    private var sameLanguage: Bool {
        guard let from = ALNativeIMTranslationBridge.match(source, in: languages),
              let to = ALNativeIMTranslationBridge.match(target, in: languages) else { return false }
        return ALNativeIMTranslationBridge.sameLanguage(from, to)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("IM translation languages").font(.headline)
            Text("Apple translates on this Mac. Language downloads require your approval.")
            Picker("From", selection: $source) {
                ForEach(languages, id: \.minimalIdentifier) { language in
                    Text(languageName(language)).tag(language.minimalIdentifier)
                }
            }
            Picker("To", selection: $target) {
                ForEach(languages, id: \.minimalIdentifier) { language in
                    Text(languageName(language)).tag(language.minimalIdentifier)
                }
            }
            Text(status).font(.callout).fixedSize(horizontal: false, vertical: true)
            HStack {
                if preparing { ProgressView().controlSize(.small) }
                Spacer()
                Button("Prepare languages") {
                    guard let sourceLanguage = ALNativeIMTranslationBridge.match(source, in: languages),
                          let targetLanguage = ALNativeIMTranslationBridge.match(target, in: languages) else { return }
                    preparing = true
                    status = "Waiting for Apple Translation…"
                    if #available(macOS 26.4, *) {
                        configuration = .init(source: sourceLanguage, target: targetLanguage, preferredStrategy: .lowLatency)
                    } else {
                        configuration = .init(source: sourceLanguage, target: targetLanguage)
                    }
                    configuration?.invalidate()
                }.disabled(preparing || languages.isEmpty || sameLanguage)
            }
        }
        .padding(22)
        .frame(width: 420)
        .task {
            languages = await ALNativeIMTranslationBridge.availability().supportedLanguages.sorted { languageName($0) < languageName($1) }
            source = ALNativeIMTranslationBridge.match(source, in: languages)?.minimalIdentifier ?? languages.first?.minimalIdentifier ?? ""
            target = ALNativeIMTranslationBridge.match(target, in: languages)?.minimalIdentifier ?? languages.first { $0.minimalIdentifier != source }?.minimalIdentifier ?? ""
        }
        .translationTask(configuration) { session in
            do {
                try await session.prepareTranslation()
                status = "Languages are ready. Close this window and retry your IM."
            } catch {
                status = "Preparation did not finish. Retry, or download these languages in System Settings > General > Language & Region > Translation Languages."
            }
            preparing = false
        }
    }

    private func languageName(_ language: Locale.Language) -> String {
        Locale.current.localizedString(forIdentifier: language.minimalIdentifier) ?? language.minimalIdentifier
    }
}

@_cdecl("al_native_im_translation_setup")
@MainActor
func nativeIMTranslationSetup(_ sourcePointer: UnsafePointer<CChar>?, _ targetPointer: UnsafePointer<CChar>?) {
    guard !ALNativeIMTranslationBridge.stopped, #available(macOS 26.0, *) else { return }
    if let window = ALNativeIMTranslationBridge.setupWindow, window.isVisible {
        window.makeKeyAndOrderFront(nil)
        return
    }
    let source = sourcePointer.map { String(cString: $0) } ?? ""
    let target = targetPointer.map { String(cString: $0) } ?? ""
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 464, height: 300),
                          styleMask: [.titled, .closable], backing: .buffered, defer: false)
    window.title = "Apple Translation setup"
    window.isReleasedWhenClosed = false
    window.delegate = ALNativeIMTranslationBridge.setupDelegate
    window.contentView = NSHostingView(rootView: ALNativeIMTranslationSetup(source: source.isEmpty ? ALNativeIMTranslationBridge.systemLanguage : source,
                                                                target: target.isEmpty ? "es" : target))
    window.center()
    ALNativeIMTranslationBridge.setupWindow = window
    window.makeKeyAndOrderFront(nil)
}

@_cdecl("al_native_im_translation_cancel")
@MainActor
func nativeIMTranslationCancel(_ id: UInt64) {
    guard let operation = ALNativeIMTranslationBridge.operations.removeValue(forKey: id) else { return }
    operation.task?.cancel()
    if #available(macOS 26.0, *), let session = operation.session as? TranslationSession { session.cancel() }
}

@_cdecl("al_native_im_translation_shutdown")
@MainActor
func nativeIMTranslationShutdown() {
    ALNativeIMTranslationBridge.stopped = true
    for id in Array(ALNativeIMTranslationBridge.operations.keys) { nativeIMTranslationCancel(id) }
    ALNativeIMTranslationBridge.setupWindow?.close()
    ALNativeIMTranslationBridge.setupWindow = nil
}
