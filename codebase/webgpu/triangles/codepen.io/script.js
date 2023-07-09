// 🔎 Check out the blog post:
// https://alain.xyz/blog/raw-webgpu
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
// 🟦 Shaders
var vertWgsl = "\nstruct VSOut {\n    @builtin(position) Position: vec4<f32>,\n    @location(0) color: vec3<f32>,\n };\n\n@vertex\nfn main(@location(0) inPos: vec3<f32>,\n        @location(1) inColor: vec3<f32>) -> VSOut {\n    var vsOut: VSOut;\n    vsOut.Position = vec4<f32>(inPos, 1.0);\n    vsOut.color = inColor;\n    return vsOut;\n}";
var fragWgsl = "\n@fragment\nfn main(@location(0) inColor: vec3<f32>) -> @location(0) vec4<f32> {\n    return vec4<f32>(inColor, 1.0);\n}\n";
// 🌅 Renderer
// 📈 Position Vertex Buffer Data
var positions = new Float32Array([
    1.0, -1.0, 0.0,
    -1.0, -1.0, 0.0,
    0.0, 1.0, 0.0
]);
// 🎨 Color Vertex Buffer Data
var colors = new Float32Array([
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0 // 🔵
]);
// 📇 Index Buffer Data
var indices = new Uint16Array([0, 1, 2]);
var Renderer = /** @class */ (function () {
    function Renderer(canvas) {
        var _this = this;
        this.render = function () {
            // ⏭ Acquire next image from context
            _this.colorTexture = _this.context.getCurrentTexture();
            _this.colorTextureView = _this.colorTexture.createView();
            // 📦 Write and submit commands to queue
            _this.encodeCommands();
            // ➿ Refresh canvas
            requestAnimationFrame(_this.render);
        };
        this.canvas = canvas;
    }
    // 🏎️ Start the rendering engine
    Renderer.prototype.start = function () {
        return __awaiter(this, void 0, void 0, function () {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0: return [4 /*yield*/, this.initializeAPI()];
                    case 1:
                        if (!_a.sent()) return [3 /*break*/, 3];
                        this.resizeBackings();
                        return [4 /*yield*/, this.initializeResources()];
                    case 2:
                        _a.sent();
                        this.render();
                        return [3 /*break*/, 4];
                    case 3:
                        canvas.style.display = "none";
                        document.getElementById("error").innerHTML = "\n<p>Doesn't look like your browser supports WebGPU.</p>\n<p>Try using any chromium browser's canary build and go to <code>about:flags</code> to <code>enable-unsafe-webgpu</code>.</p>";
                        _a.label = 4;
                    case 4: return [2 /*return*/];
                }
            });
        });
    };
    // 🌟 Initialize WebGPU
    Renderer.prototype.initializeAPI = function () {
        return __awaiter(this, void 0, Promise, function () {
            var entry, _a, _b, e_1;
            return __generator(this, function (_c) {
                switch (_c.label) {
                    case 0:
                        _c.trys.push([0, 3, , 4]);
                        entry = navigator.gpu;
                        if (!entry) {
                            return [2 /*return*/, false];
                        }
                        // 🔌 Physical Device Adapter
                        _a = this;
                        return [4 /*yield*/, entry.requestAdapter()];
                    case 1:
                        // 🔌 Physical Device Adapter
                        _a.adapter = _c.sent();
                        // 💻 Logical Device
                        _b = this;
                        return [4 /*yield*/, this.adapter.requestDevice()];
                    case 2:
                        // 💻 Logical Device
                        _b.device = _c.sent();
                        // 📦 Queue
                        this.queue = this.device.queue;
                        return [3 /*break*/, 4];
                    case 3:
                        e_1 = _c.sent();
                        console.error(e_1);
                        return [2 /*return*/, false];
                    case 4: return [2 /*return*/, true];
                }
            });
        });
    };
    // 🍱 Initialize resources to render triangle (buffers, shaders, pipeline)
    Renderer.prototype.initializeResources = function () {
        return __awaiter(this, void 0, void 0, function () {
            var createBuffer, vsmDesc, fsmDesc, positionAttribDesc, colorAttribDesc, positionBufferDesc, colorBufferDesc, depthStencil, pipelineLayoutDesc, layout, vertex, colorState, fragment, primitive, pipelineDesc;
            var _this = this;
            return __generator(this, function (_a) {
                createBuffer = function (arr, usage) {
                    // 📏 Align to 4 bytes (thanks @chrimsonite)
                    var desc = {
                        size: (arr.byteLength + 3) & ~3,
                        usage: usage,
                        mappedAtCreation: true
                    };
                    var buffer = _this.device.createBuffer(desc);
                    var writeArray = arr instanceof Uint16Array
                        ? new Uint16Array(buffer.getMappedRange())
                        : new Float32Array(buffer.getMappedRange());
                    writeArray.set(arr);
                    buffer.unmap();
                    return buffer;
                };
                this.positionBuffer = createBuffer(positions, GPUBufferUsage.VERTEX);
                this.colorBuffer = createBuffer(colors, GPUBufferUsage.VERTEX);
                this.indexBuffer = createBuffer(indices, GPUBufferUsage.INDEX);
                vsmDesc = {
                    code: vertWgsl
                };
                this.vertModule = this.device.createShaderModule(vsmDesc);
                fsmDesc = {
                    code: fragWgsl
                };
                this.fragModule = this.device.createShaderModule(fsmDesc);
                positionAttribDesc = {
                    shaderLocation: 0,
                    offset: 0,
                    format: 'float32x3'
                };
                colorAttribDesc = {
                    shaderLocation: 1,
                    offset: 0,
                    format: 'float32x3'
                };
                positionBufferDesc = {
                    attributes: [positionAttribDesc],
                    arrayStride: 4 * 3,
                    stepMode: 'vertex'
                };
                colorBufferDesc = {
                    attributes: [colorAttribDesc],
                    arrayStride: 4 * 3,
                    stepMode: 'vertex'
                };
                depthStencil = {
                    depthWriteEnabled: true,
                    depthCompare: 'less',
                    format: 'depth24plus-stencil8'
                };
                pipelineLayoutDesc = { bindGroupLayouts: [] };
                layout = this.device.createPipelineLayout(pipelineLayoutDesc);
                vertex = {
                    module: this.vertModule,
                    entryPoint: 'main',
                    buffers: [positionBufferDesc, colorBufferDesc]
                };
                colorState = {
                    format: 'bgra8unorm',
                    writeMask: GPUColorWrite.ALL
                };
                fragment = {
                    module: this.fragModule,
                    entryPoint: 'main',
                    targets: [colorState]
                };
                primitive = {
                    frontFace: 'cw',
                    cullMode: 'none',
                    topology: 'triangle-list'
                };
                pipelineDesc = {
                    layout: layout,
                    vertex: vertex,
                    fragment: fragment,
                    primitive: primitive,
                    depthStencil: depthStencil
                };
                this.pipeline = this.device.createRenderPipeline(pipelineDesc);
                return [2 /*return*/];
            });
        });
    };
    // ↙️ Resize Canvas, frame buffer attachments
    Renderer.prototype.resizeBackings = function () {
        // ⛓️ Canvas Context
        if (!this.context) {
            this.context = this.canvas.getContext('webgpu');
            var canvasConfig = {
                device: this.device,
                alphaMode: "opaque",
                format: 'bgra8unorm',
                usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC
            };
            this.context.configure(canvasConfig);
        }
        var depthTextureDesc = {
            size: [this.canvas.width, this.canvas.height, 1],
            dimension: '2d',
            format: 'depth24plus-stencil8',
            usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC
        };
        this.depthTexture = this.device.createTexture(depthTextureDesc);
        this.depthTextureView = this.depthTexture.createView();
    };
    // ✍️ Write commands to send to the GPU
    Renderer.prototype.encodeCommands = function () {
        var colorAttachment = {
            view: this.colorTextureView,
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: 'clear',
            storeOp: 'store'
        };
        var depthAttachment = {
            view: this.depthTextureView,
            depthClearValue: 1,
            depthLoadOp: 'clear',
            depthStoreOp: 'store',
            stencilClearValue: 0,
            stencilLoadOp: 'clear',
            stencilStoreOp: 'store',
        };
        var renderPassDesc = {
            colorAttachments: [colorAttachment],
            depthStencilAttachment: depthAttachment
        };
        this.commandEncoder = this.device.createCommandEncoder();
        // 🖌️ Encode drawing commands
        this.passEncoder = this.commandEncoder.beginRenderPass(renderPassDesc);
        this.passEncoder.setPipeline(this.pipeline);
        this.passEncoder.setViewport(0, 0, this.canvas.width, this.canvas.height, 0, 1);
        this.passEncoder.setScissorRect(0, 0, this.canvas.width, this.canvas.height);
        this.passEncoder.setVertexBuffer(0, this.positionBuffer);
        this.passEncoder.setVertexBuffer(1, this.colorBuffer);
        this.passEncoder.setIndexBuffer(this.indexBuffer, 'uint16');
        this.passEncoder.drawIndexed(3, 1);
        this.passEncoder.end();
        this.queue.submit([this.commandEncoder.finish()]);
    };
    return Renderer;
}());
// Main
var canvas = document.getElementById('gfx');
canvas.width = canvas.height = 640;
var renderer = new Renderer(canvas);
renderer.start();
