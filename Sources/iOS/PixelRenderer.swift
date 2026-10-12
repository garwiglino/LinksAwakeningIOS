import MetalKit

final class PixelRenderer: NSObject, MTKViewDelegate {
    private let commandQueue: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState
    private let texture: MTLTexture
    private var pixels = [UInt32](repeating: 0xff000000, count: 160 * 144)
    private let slots = DispatchSemaphore(value: 3)
    var integerScaling = UserDefaults.standard.bool(forKey: "integerScaling")
    var pollInput: (() -> Void)?
    var onError: ((String) -> Void)?

    init(view: MTKView) throws {
        guard let device = MTLCreateSystemDefaultDevice(), let queue = device.makeCommandQueue(),
              let library = device.makeDefaultLibrary(),
              let vertex = library.makeFunction(name: "dxVertex"),
              let fragment = library.makeFunction(name: "dxFragment") else {
            throw NSError(domain: "DXMetal", code: 1, userInfo: [NSLocalizedDescriptionKey: "Metal est indisponible."])
        }
        commandQueue = queue
        let descriptor = MTLRenderPipelineDescriptor()
        descriptor.vertexFunction = vertex
        descriptor.fragmentFunction = fragment
        descriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
        pipeline = try device.makeRenderPipelineState(descriptor: descriptor)
        let tex = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: 160, height: 144, mipmapped: false)
        tex.usage = .shaderRead
        tex.storageMode = .shared
        guard let image = device.makeTexture(descriptor: tex) else {
            throw NSError(domain: "DXMetal", code: 2, userInfo: [NSLocalizedDescriptionKey: "Texture vidéo indisponible."])
        }
        texture = image
        super.init()
        view.device = device
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColorMake(0.025, 0.04, 0.055, 1)
        view.framebufferOnly = true
        view.preferredFramesPerSecond = 60
        view.delegate = self
    }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}
    func draw(in view: MTKView) {
        pollInput?()
        guard slots.wait(timeout: .now()) == .success else { return }
        guard let drawable = view.currentDrawable, let pass = view.currentRenderPassDescriptor,
              let buffer = commandQueue.makeCommandBuffer() else { slots.signal(); return }
        pixels.withUnsafeMutableBufferPointer { p in _ = dx_copy_frame(p.baseAddress, p.count) }
        // Each submitted command uses its own texture, so CPU uploads never race
        // GPU sampling on the preceding frame.
        guard let image = view.device?.makeTexture(descriptor: {
            let d = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: 160, height: 144, mipmapped: false)
            d.storageMode = .shared; d.usage = .shaderRead; return d
        }()) else { slots.signal(); return }
        pixels.withUnsafeBytes { data in
            image.replace(region: MTLRegionMake2D(0, 0, 160, 144), mipmapLevel: 0,
                          withBytes: data.baseAddress!, bytesPerRow: 160 * 4)
        }
        guard let encoder = buffer.makeRenderCommandEncoder(descriptor: pass) else { slots.signal(); return }
        let w = Double(view.drawableSize.width), h = Double(view.drawableSize.height)
        var scale = min(w / 160, h / 144)
        if integerScaling, scale >= 1 { scale = floor(scale) }
        let vw = 160 * scale, vh = 144 * scale
        encoder.setViewport(MTLViewport(originX: (w - vw) / 2, originY: (h - vh) / 2,
                                        width: vw, height: vh, znear: 0, zfar: 1))
        encoder.setRenderPipelineState(pipeline)
        encoder.setFragmentTexture(image, index: 0)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        encoder.endEncoding()
        buffer.present(drawable)
        buffer.addCompletedHandler { [slots] _ in slots.signal() }
        buffer.commit()
    }
}
