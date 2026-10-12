import UIKit
import GameController

// Bits 0..3 = A/B/Select/Start, bits 4..7 = Right/Left/Up/Down.
// Each UITouch owns its state; lifting one finger cannot release another.
final class TouchControls: UIView {
    var onChange: ((UInt16) -> Void)?
    private var fingers: [ObjectIdentifier: CGPoint] = [:]
    private var dpad = CGRect.zero
    private var buttons: [(String, UInt16, CGRect)] = []
    private var value: UInt16 = 0
    private let feedback = UIImpactFeedbackGenerator(style: .light)
    var haptics = true
    var controlsOpacity: CGFloat = 0.72 { didSet { setNeedsDisplay() } }

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
        isMultipleTouchEnabled = true
        accessibilityLabel = "Commandes du jeu : croix directionnelle, A, B, Select, Start"
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is not supported") }

    override func layoutSubviews() {
        super.layoutSubviews()
        let s = safeAreaInsets
        let w = bounds.width, h = bounds.height
        let size: CGFloat = min(144, max(100, min(w, h) * 0.34))
        let bottom = h - s.bottom - 18
        dpad = CGRect(x: s.left + 22, y: bottom - size, width: size, height: size)
        let diameter: CGFloat = max(48, size * 0.40)
        let ax = w - s.right - diameter - 20
        let ay = bottom - diameter - size * 0.33
        let bx = ax - diameter - 12
        let by = bottom - diameter
        // On narrow screens put Start/Select on their own row. Their expanded
        // hit regions must not overlap the D-pad or B button.
        let auxiliaryY = w < 600 ? bottom - size - 76 : bottom - 43
        buttons = [
            ("A", 1, CGRect(x: ax, y: ay, width: diameter, height: diameter)),
            ("B", 2, CGRect(x: bx, y: by, width: diameter, height: diameter)),
            ("SELECT", 4, CGRect(x: w / 2 - 84, y: auxiliaryY, width: 76, height: 40)),
            ("START", 8, CGRect(x: w / 2 + 8, y: auxiliaryY, width: 76, height: 40))
        ]
        cancelAll()
        setNeedsDisplay()
    }
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        dpad.insetBy(dx: -15, dy: -15).contains(point) || buttons.contains { $0.2.insetBy(dx: -10, dy: -10).contains(point) }
    }
    private func update(_ touches: Set<UITouch>, remove: Bool) {
        for touch in touches {
            if remove { fingers.removeValue(forKey: ObjectIdentifier(touch)) }
            else { fingers[ObjectIdentifier(touch)] = touch.location(in: self) }
        }
        var next: UInt16 = 0
        for p in fingers.values {
            if dpad.insetBy(dx: -20, dy: -20).contains(p) {
                let x = (p.x - dpad.midX) / (dpad.width / 2)
                let y = (p.y - dpad.midY) / (dpad.height / 2)
                if x > 0.28 { next |= 16 }
                if x < -0.28 { next |= 32 }
                if y < -0.28 { next |= 64 }
                if y > 0.28 { next |= 128 }
            }
            for (_, mask, rect) in buttons where rect.insetBy(dx: -10, dy: -10).contains(p) { next |= mask }
        }
        if next != value {
            if next & ~value != 0, haptics { feedback.impactOccurred(intensity: 0.45) }
            value = next
            onChange?(next)
            setNeedsDisplay()
        }
    }
    func cancelAll() { fingers.removeAll(); value = 0; onChange?(0); setNeedsDisplay() }
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, remove: false) }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, remove: false) }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, remove: true) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, remove: true) }
    override func draw(_ rect: CGRect) {
        func label(_ text: String, _ box: CGRect, _ pressed: Bool, small: Bool = false) {
            let background = (pressed ? UIColor.systemMint : UIColor(white: 0.15, alpha: 1)).withAlphaComponent(controlsOpacity)
            background.setFill()
            UIBezierPath(roundedRect: box, cornerRadius: small ? 12 : box.width / 2).fill()
            UIColor.white.withAlphaComponent(0.3).setStroke()
            UIBezierPath(roundedRect: box, cornerRadius: small ? 12 : box.width / 2).stroke()
            let attrs: [NSAttributedString.Key: Any] = [.font: UIFont.systemFont(ofSize: small ? 11 : 24, weight: .bold),
                                                      .foregroundColor: UIColor.white]
            let size = (text as NSString).size(withAttributes: attrs)
            (text as NSString).draw(at: CGPoint(x: box.midX - size.width / 2, y: box.midY - size.height / 2), withAttributes: attrs)
        }
        let third = dpad.width / 3
        label("◀", CGRect(x: dpad.minX, y: dpad.minY + third, width: third, height: third), value & 32 != 0)
        label("▶", CGRect(x: dpad.minX + third * 2, y: dpad.minY + third, width: third, height: third), value & 16 != 0)
        label("▲", CGRect(x: dpad.minX + third, y: dpad.minY, width: third, height: third), value & 64 != 0)
        label("▼", CGRect(x: dpad.minX + third, y: dpad.minY + third * 2, width: third, height: third), value & 128 != 0)
        for (name, mask, box) in buttons { label(name, box, value & mask != 0, small: mask >= 4) }
    }
}

final class ControllerInput {
    let input: InputState
    var onPause: (() -> Void)?
    private var pauseHeld = false
    init(input: InputState) { self.input = input }
    var connected: Bool { !GCController.controllers().isEmpty }
    func poll() {
        var mask: UInt16 = 0
        var pause = false
        for controller in GCController.controllers() {
            if let pad = controller.extendedGamepad {
                let x = abs(pad.dpad.xAxis.value) > 0.1 ? pad.dpad.xAxis.value : pad.leftThumbstick.xAxis.value
                let y = abs(pad.dpad.yAxis.value) > 0.1 ? pad.dpad.yAxis.value : pad.leftThumbstick.yAxis.value
                if x > 0.35 { mask |= 16 }; if x < -0.35 { mask |= 32 }
                if y > 0.35 { mask |= 64 }; if y < -0.35 { mask |= 128 }
                // Nintendo spatial layout: A east, B south. Optional swap.
                let swap = UserDefaults.standard.bool(forKey: "swapControllerAB")
                if (swap ? pad.buttonA : pad.buttonB).isPressed { mask |= 1 }
                if (swap ? pad.buttonB : pad.buttonA).isPressed { mask |= 2 }
                if pad.buttonOptions?.isPressed == true || pad.leftShoulder.isPressed { mask |= 4 }
                if pad.buttonMenu.isPressed { mask |= 8 }
                pause = pause || (pad.leftShoulder.isPressed && pad.rightShoulder.isPressed)
            } else if let pad = controller.microGamepad {
                if pad.dpad.xAxis.value > 0.35 { mask |= 16 }
                if pad.dpad.xAxis.value < -0.35 { mask |= 32 }
                if pad.dpad.yAxis.value > 0.35 { mask |= 64 }
                if pad.dpad.yAxis.value < -0.35 { mask |= 128 }
                if pad.buttonA.isPressed { mask |= 1 }; if pad.buttonX.isPressed { mask |= 2 }
                if pad.buttonMenu.isPressed { mask |= 8 }
            }
        }
        input.setController(mask)
        if pause && !pauseHeld { onPause?() }
        pauseHeld = pause
    }
}
