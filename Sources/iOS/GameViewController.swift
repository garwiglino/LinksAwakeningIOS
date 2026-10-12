import UIKit
import MetalKit
import AVFoundation
import GameController

final class GameViewController: UIViewController {
    private let engine = GameEngine()
    private let metal = MTKView()
    private let controls = TouchControls()
    private let menuButton = UIButton(type: .system)
    private var renderer: PixelRenderer?
    private var controller: ControllerInput?
    private var prepared = false
    private var menuShown = false
    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { [.bottom, .left, .right] }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .allButUpsideDown }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 0.025, green: 0.04, blue: 0.055, alpha: 1)
        view.addSubview(metal)
        view.addSubview(controls)
        controls.haptics = !UserDefaults.standard.bool(forKey: "disableHaptics")
        menuButton.setImage(UIImage(systemName: "pause.circle.fill"), for: .normal)
        menuButton.tintColor = .white
        menuButton.backgroundColor = UIColor.black.withAlphaComponent(0.4)
        menuButton.layer.cornerRadius = 22
        menuButton.accessibilityLabel = "Pause et réglages"
        menuButton.addTarget(self, action: #selector(showMenu), for: .touchUpInside)
        view.addSubview(menuButton)
        controls.onChange = { [weak self] value in self?.engine.input.setTouch(value) }
        engine.onError = { [weak self] message in self?.showError(message) }
        let c = ControllerInput(input: engine.input)
        c.onPause = { [weak self] in self?.showMenu() }
        controller = c
        do {
            renderer = try PixelRenderer(view: metal)
            renderer?.pollInput = { [weak self] in self?.controller?.poll() }
            try engine.prepare()
            prepared = true
        } catch { DispatchQueue.main.async { self.showError(error.localizedDescription) } }
        let notifications: [(Notification.Name, Selector)] = [
            (UIApplication.willResignActiveNotification, #selector(suspend)),
            (UIApplication.didBecomeActiveNotification, #selector(becameActive)),
            (AVAudioSession.interruptionNotification, #selector(interrupted(_:))),
            (AVAudioSession.routeChangeNotification, #selector(routeChanged(_:))),
            (AVAudioSession.mediaServicesWereResetNotification, #selector(suspend)),
            (.GCControllerDidConnect, #selector(controllerChanged)),
            (.GCControllerDidDisconnect, #selector(controllerChanged))
        ]
        for (name, selector) in notifications { NotificationCenter.default.addObserver(self, selector: selector, name: name, object: nil) }
    }
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        if prepared && !engine.running && !menuShown { showMenu() }
    }
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let safe = view.safeAreaLayoutGuide.layoutFrame
        let landscape = safe.width > safe.height
        let hideControls = controller?.connected == true && UserDefaults.standard.bool(forKey: "hideTouchWithController")
        controls.isHidden = hideControls
        controls.frame = view.bounds
        if hideControls || landscape { metal.frame = safe }
        else {
            // Portrait keeps the game clear of fingers; landscape uses side space.
            let controlHeight = min(260, safe.height * 0.43)
            metal.frame = CGRect(x: safe.minX, y: safe.minY + 40, width: safe.width,
                                 height: max(1, safe.height - controlHeight - 40))
        }
        menuButton.frame = CGRect(x: safe.maxX - 50, y: safe.minY + 4, width: 44, height: 44)
    }
    @objc private func controllerChanged() {
        engine.input.setController(0)
        view.setNeedsLayout()
    }
    @objc private func suspend() {
        controls.cancelAll()
        engine.pause()
        metal.isPaused = true
    }
    @objc private func becameActive() {
        metal.isPaused = false
        if prepared && !engine.running && !menuShown && presentedViewController == nil { showMenu() }
    }
    @objc private func interrupted(_ notification: Notification) {
        if let kind = notification.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
           kind == AVAudioSession.InterruptionType.began.rawValue { suspend() }
        else if UIApplication.shared.applicationState == .active { becameActive() }
    }
    @objc private func routeChanged(_ notification: Notification) {
        if let raw = notification.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
           raw == AVAudioSession.RouteChangeReason.oldDeviceUnavailable.rawValue {
            suspend()
            if presentedViewController == nil { showMenu() }
        }
    }
    private func resume() {
        guard prepared else { return }
        do {
            try engine.resume()
            metal.isPaused = false
        } catch { showError(error.localizedDescription) }
    }
    @objc private func showMenu() {
        guard prepared, !menuShown, presentedViewController == nil else { return }
        controls.cancelAll()
        // Opening a menu must not overwrite a manual snapshot before "Load".
        engine.pause(save: false)
        menuShown = true
        let menu = UIAlertController(title: "Link’s Awakening DX", message: "Sauvegarde de reprise automatique quand l’app passe en arrière-plan.\nManette : L + R ouvre ce menu.", preferredStyle: .alert)
        func add(_ title: String, action: @escaping () -> Void) {
            menu.addAction(UIAlertAction(title: title, style: .default) { [weak self] _ in
                self?.menuShown = false
                DispatchQueue.main.async { action() }
            })
        }
        add("Reprendre") { [weak self] in self?.resume() }
        add("Sauvegarder maintenant") { [weak self] in
            guard let self else { return }
            do { try self.engine.saveNow(); self.resume() } catch { self.showError(error.localizedDescription) }
        }
        add("Charger la reprise") { [weak self] in
            guard let self else { return }
            do { try self.engine.loadNow(); self.resume() } catch { self.showError(error.localizedDescription) }
        }
        add("Réglages") { [weak self] in self?.showSettings() }
        add("Exporter les sauvegardes") { [weak self] in self?.exportSaves() }
        menu.popoverPresentationController?.sourceView = menuButton
        menu.popoverPresentationController?.sourceRect = menuButton.bounds
        present(menu, animated: true)
    }
    private func showSettings() {
        let settings = SettingsController()
        settings.onDone = { [weak self] in
            guard let self else { return }
            self.renderer?.integerScaling = UserDefaults.standard.bool(forKey: "integerScaling")
            self.controls.haptics = !UserDefaults.standard.bool(forKey: "disableHaptics")
            self.view.setNeedsLayout()
            self.resume()
        }
        let navigation = UINavigationController(rootViewController: settings)
        navigation.modalPresentationStyle = .formSheet
        navigation.isModalInPresentation = true
        present(navigation, animated: true)
    }
    private func exportSaves() {
        do {
            try engine.saveNow()
            let root = try FileManager.default.url(for: .applicationSupportDirectory,
                                                  in: .userDomainMask, appropriateFor: nil, create: false)
            let saves = root.appendingPathComponent("LinksAwakeningDX/Saves")
            let files = try FileManager.default.contentsOfDirectory(at: saves, includingPropertiesForKeys: nil)
                .filter { ["sav", "state"].contains($0.pathExtension) }
            let sheet = UIActivityViewController(activityItems: files, applicationActivities: nil)
            sheet.popoverPresentationController?.sourceView = menuButton
            sheet.completionWithItemsHandler = { [weak self] _, _, _, _ in self?.resume() }
            present(sheet, animated: true)
        } catch { showError(error.localizedDescription) }
    }
    private func showError(_ message: String) {
        guard presentedViewController == nil else { return }
        engine.pause(save: false)
        let alert = UIAlertController(title: "Link’s Awakening DX", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak self] _ in
            DispatchQueue.main.async { self?.showMenu() }
        })
        present(alert, animated: true)
    }
}

final class SettingsController: UITableViewController {
    var onDone: (() -> Void)?
    private let rows = [("Pixels entiers", "integerScaling"), ("Inverser A/B sur manette", "swapControllerAB"),
                        ("Masquer le tactile avec une manette", "hideTouchWithController"), ("Désactiver les vibrations", "disableHaptics")]
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Réglages"
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Terminé", style: .done, target: self, action: #selector(done))
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int { rows.count + 1 }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .default, reuseIdentifier: nil)
        cell.selectionStyle = .none
        if indexPath.row < rows.count {
            let (title, key) = rows[indexPath.row]
            cell.textLabel?.text = title
            cell.textLabel?.numberOfLines = 2
            let toggle = UISwitch()
            toggle.isOn = UserDefaults.standard.bool(forKey: key)
            toggle.addAction(UIAction { _ in UserDefaults.standard.set(toggle.isOn, forKey: key) }, for: .valueChanged)
            cell.accessoryView = toggle
        } else {
            cell.textLabel?.text = "Volume"
            let slider = UISlider(frame: CGRect(x: 0, y: 0, width: 170, height: 44))
            slider.value = UserDefaults.standard.object(forKey: "volume") as? Float ?? 0.8
            slider.addAction(UIAction { _ in UserDefaults.standard.set(slider.value, forKey: "volume") }, for: .valueChanged)
            cell.accessoryView = slider
        }
        return cell
    }
    @objc private func done() { dismiss(animated: true) { self.onDone?() } }
}
