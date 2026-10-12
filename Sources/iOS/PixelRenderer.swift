import MetalKit

final class PixelRenderer: NSObject, MTKViewDelegate {
    private let commandQueue: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState
    private let textures: [MTLTexture]
    private var textureIndex = 0
    private var pixels = [UInt32](repeating: 0xff000000, count: 512 * 144)
    private let slots = DispatchSemaphore(value: 3)
    var wideScenery: Bool {
#if DEBUG
        return dx_wide_supported() && UserDefaults.standard.bool(forKey: "wideScenery")
#else
        return false
#endif
    }
    private var frameWidth: UInt32 = 160
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
        let tex = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: 512, height: 144, mipmapped: false)
        tex.usage = .shaderRead
        tex.storageMode = .shared
        let images = (0..<3).compactMap { _ in device.makeTexture(descriptor: tex) }
        guard images.count == 3 else {
            throw NSError(domain: "DXMetal", code: 2, userInfo: [NSLocalizedDescriptionKey: "Texture vidéo indisponible."])
        }
        textures = images
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
        let w = Double(view.drawableSize.width), h = Double(view.drawableSize.height)
        guard w > 0, h > 0 else { slots.signal(); return }
        let requested = wideScenery && w > h ? UInt32(min(512, max(160, (w / h * 144).rounded()))) : 160
        dx_request_video_width(requested)
        pixels.withUnsafeMutableBufferPointer { p in _ = dx_copy_video(p.baseAddress, p.count, &frameWidth) }
        // One ordered command queue and three bounded in-flight frames keep
        // uploads away from textures still sampled by the GPU.
        let image = textures[textureIndex]
        pixels.withUnsafeBytes { data in
            image.replace(region: MTLRegionMake2D(0, 0, Int(frameWidth), 144), mipmapLevel: 0,
                          withBytes: data.baseAddress!, bytesPerRow: Int(frameWidth) * 4)
        }
        guard let encoder = buffer.makeRenderCommandEncoder(descriptor: pass) else { slots.signal(); return }
        let sourceWidth = Double(frameWidth)
        var scale = min(w / sourceWidth, h / 144)
        if integerScaling, frameWidth == 160, scale >= 1 { scale = floor(scale) }
        let vw = sourceWidth * scale, vh = 144 * scale
        encoder.setViewport(MTLViewport(originX: (w - vw) / 2, originY: (h - vh) / 2,
                                        width: vw, height: vh, znear: 0, zfar: 1))
        encoder.setRenderPipelineState(pipeline)
        encoder.setFragmentTexture(image, index: 0)
        var uvScale = SIMD2<Float>(Float(frameWidth) / 512, 1)
        encoder.setFragmentBytes(&uvScale, length: MemoryLayout<SIMD2<Float>>.stride, index: 0)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        encoder.endEncoding()
        buffer.present(drawable)
        buffer.addCompletedHandler { [slots] _ in slots.signal() }
        textureIndex = (textureIndex + 1) % textures.count
        buffer.commit()
    }
}
