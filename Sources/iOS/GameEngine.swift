import Foundation
import AVFoundation
import UIKit

final class InputState {
    private let lock = NSLock()
    private var touch: UInt16 = 0
    private var controller: UInt16 = 0
    func setTouch(_ value: UInt16) { lock.lock(); touch = value; lock.unlock() }
    func setController(_ value: UInt16) { lock.lock(); controller = value; lock.unlock() }
    func reset() { lock.lock(); touch = 0; controller = 0; lock.unlock() }
    func snapshot() -> UInt16 {
        lock.lock(); defer { lock.unlock() }
        return touch | controller
    }
}

// The C runtime is confined to queue. Audio consumes a lock-free SPSC ring;
// Metal only reads a copied framebuffer through the core's small snapshot lock.
final class GameEngine {
    let input = InputState()
    private let queue = DispatchQueue(label: "fr.garwig.linksawakening.engine", qos: .userInteractive)
    private var timer: DispatchSourceTimer?
    private var opened = false
    private var frames = 0
    private var audio: AVAudioEngine?
    private var source: AVAudioSourceNode?
    private(set) var running = false
    var onError: ((String) -> Void)?

    func prepare() throws {
        let root = try FileManager.default.url(for: .applicationSupportDirectory,
                                              in: .userDomainMask, appropriateFor: nil, create: true)
        let saves = root.appendingPathComponent("LinksAwakeningDX/Saves", isDirectory: true)
        try FileManager.default.createDirectory(at: saves, withIntermediateDirectories: true)
        try queue.sync {
            guard dx_create(saves.path) else { throw coreError() }
            opened = true
            if dx_has_state(), !dx_load_state() {
                // A corrupt or older snapshot must not replace working battery RAM.
                DispatchQueue.main.async { [weak self] in
                    self?.onError?("La reprise n’a pas été chargée. La sauvegarde du jeu reste disponible.")
                }
            }
        }
    }

    func resume() throws {
        guard opened, !running else { return }
        try startAudio()
        running = true
        UIApplication.shared.isIdleTimerDisabled = true
        queue.sync {
            let timer = DispatchSource.makeTimerSource(queue: queue)
            // Hardware cadence, independent of 60/120 Hz ProMotion.
            let period = Int((70_224.0 / 4_194_304.0 * 1_000_000_000.0).rounded())
            timer.schedule(deadline: .now(), repeating: .nanoseconds(period), leeway: .microseconds(500))
            timer.setEventHandler { [weak self] in
                guard let self else { return }
                let mask = self.input.snapshot()
                if !dx_step(UInt8(mask & 15), UInt8((mask >> 4) & 15)) {
                    self.reportError()
                    return
                }
                self.frames += 1
                if self.frames % 300 == 0, !dx_flush_battery() { self.reportError() }
            }
            self.timer = timer
            timer.resume()
        }
    }

    func pause(save: Bool = true) {
        guard opened else { return }
        running = false
        input.reset()
        // Stop producer, then realtime consumer, before clearing the audio ring.
        queue.sync { timer?.setEventHandler {}; timer?.cancel(); timer = nil }
        audio?.stop()
        audio = nil
        source = nil
        queue.sync {
            if save {
                if !dx_flush_battery() { reportError() }
                if !dx_save_state() { reportError() }
            }
            dx_audio_clear()
        }
        UIApplication.shared.isIdleTimerDisabled = false
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    func saveNow() throws {
        try queue.sync {
            guard dx_flush_battery(), dx_save_state() else { throw coreError() }
        }
    }
    func loadNow() throws {
        // Menu already paused both audio and runtime.
        guard !running else { return }
        try queue.sync {
            guard dx_load_state() else { throw coreError() }
            dx_audio_clear()
        }
    }

    func startAudio() throws {
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playback, mode: .default, options: [])
        try session.setPreferredSampleRate(44_100)
        try session.setPreferredIOBufferDuration(0.01)
        try session.setActive(true)
        let engine = AVAudioEngine()
        guard let format = AVAudioFormat(standardFormatWithSampleRate: 44_100, channels: 2) else {
            throw NSError(domain: "DXAudio", code: 1, userInfo: [NSLocalizedDescriptionKey: "Format audio indisponible."])
        }
        let node = AVAudioSourceNode(format: format) { _, _, count, buffers in
            let list = UnsafeMutableAudioBufferListPointer(buffers)
            guard list.count == 2, let l = list[0].mData, let r = list[1].mData else { return -50 }
            dx_audio_read(l.assumingMemoryBound(to: Float.self), r.assumingMemoryBound(to: Float.self), Int(count))
            return noErr
        }
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: format)
        engine.mainMixerNode.outputVolume = UserDefaults.standard.object(forKey: "volume") as? Float ?? 0.8
        engine.prepare()
        try engine.start()
        audio = engine
        source = node
    }
    func setVolume(_ value: Float) {
        UserDefaults.standard.set(value, forKey: "volume")
        audio?.mainMixerNode.outputVolume = value
    }
    private func coreError() -> NSError {
        NSError(domain: "DXCore", code: 1, userInfo: [NSLocalizedDescriptionKey: String(cString: dx_error())])
    }
    private func reportError() {
        let message = String(cString: dx_error())
        DispatchQueue.main.async { [weak self] in self?.onError?(message) }
    }
}
