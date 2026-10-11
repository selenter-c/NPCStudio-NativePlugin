# NIF Viewer Architecture

## Overview

The NIF Viewer is a native 3D model viewer integrated into NPC Studio's PrismaUI interface. It renders Skyrim NIF models using Direct3D 11 and displays them in the HTML/CSS/JavaScript UI.

## Architecture Decision Record

### Problem
Display interactive 3D previews of Skyrim NIF models within the PrismaUI HTML interface without WebGL support.

### Decision
Implement a native C++ rendering pipeline that:
1. Parses NIF files using the nifly library
2. Renders geometry to an offscreen D3D11 render target
3. Reads back pixels to CPU and encodes as base64 PNG
4. Delivers frames to JavaScript via PrismaUI's InteropCall mechanism
5. Displays in an HTML `<img>` element

### Alternatives Considered

**WebGL/Three.js**: Not supported by PrismaUI's Ultralight renderer.

**Custom Ultralight ImageSource**: The PrismaUI public API does not expose custom texture/image source registration. Would require framework modifications.

**File-based transport**: Writing PNGs to disk would cause cache invalidation issues and slower I/O.

### Trade-offs
- Base64 encoding adds ~33% overhead to frame data size
- GPU-to-CPU readback has latency (~1-2ms for 512x512)
- Acceptable for MVP; future optimization path exists if PrismaUI exposes texture APIs

## Component Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    HTML/CSS/JavaScript                       │
│  ┌─────────────────┐  ┌──────────────────┐  ┌───────────┐  │
│  │  Model Container │  │  Viewer Controls │  │  Status   │  │
│  │  (<img> element) │  │  (rotate/zoom)   │  │  Display  │  │
│  └────────┬────────┘  └────────┬─────────┘  └───────────┘  │
│           │                     │                            │
│           ▼                     ▼                            │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              NifViewerBridge (JS ↔ C++)                 │ │
│  │  - onNifViewerEvent() ← responses                      │ │
│  │  - nifViewerCommand() → commands                       │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Native C++ Layer                        │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │                  NifViewerService                       │ │
│  │  - Lifecycle management                                │ │
│  │  - State machine (Uninitialized→Ready→Rendering)       │ │
│  │  - Dirty flag for on-demand rendering                  │ │
│  └────────────────────────────────────────────────────────┘ │
│           │                                                  │
│     ┌─────┴─────┐                                            │
│     ▼           ▼                                            │
│  ┌─────────┐  ┌─────────────────┐                           │
│  │NifModel │  │NifRenderBackend │                           │
│  │ Loader  │  │  - D3D11 device │                           │
│  │ - nifly │  │  - Shaders      │                           │
│  │ - Parse │  │  - Camera       │                           │
│  └─────────┘  └────────┬────────┘                           │
│                        │                                     │
│                        ▼                                     │
│              ┌─────────────────┐                             │
│              │ NifRenderTarget │                             │
│              │ - Color texture │                             │
│              │ - Depth buffer  │                             │
│              └────────┬────────┘                             │
│                       │                                      │
│                       ▼                                      │
│              ┌─────────────────┐                             │
│              │NifFrameTransport│                             │
│              │ - GPU readback  │                             │
│              │ - BGRA→RGBA     │                             │
│              │ - Base64 encode │                             │
│              └─────────────────┘                             │
└─────────────────────────────────────────────────────────────┘
```

## Command Protocol

### JavaScript → C++
```json
{
  "command": "loadModel|unloadModel|rotate|zoom|resetCamera|resize|setVisible|getStatus",
  "requestId": 1,
  "payload": { ... }
}
```

### C++ → JavaScript
Events are delivered via `onNifViewerEvent(jsonStr)`:
```json
{
  "event": "ready|modelLoaded|frameUpdated|error|...",
  "data": { ... }
}
```

## Rendering Pipeline

1. **Load**: nifly parses NIF → extracts vertices, indices, normals, UVs
2. **Upload**: Create D3D11 vertex/index buffers
3. **Frame**: Auto-frame camera based on model bounding box
4. **Render**: Draw to offscreen render target (transparent background)
5. **Readback**: Copy to staging texture → map → BGRA pixels
6. **Encode**: Convert BGRA→RGBA → base64 string
7. **Deliver**: Send via InteropCall → display in `<img>`

## Performance Characteristics

- **Static models**: Render only when dirty (camera change, resize, load)
- **Interactive updates**: ~30 FPS target during drag
- **Readback cost**: ~1-2ms for 512×512 RGBA
- **Base64 overhead**: ~33% size increase over raw PNG

## File Structure

```
src/nifviewer/
├── NifViewerConfig.h      # Configuration constants
├── NifViewerService.h/cpp # Main service lifecycle
├── NifModelLoader.h/cpp   # NIF parsing with nifly
├── NifRenderBackend.h/cpp # D3D11 rendering
├── NifRenderTarget.h/cpp  # Offscreen render targets
├── NifFrameTransport.h/cpp# GPU readback + encoding
└── NifViewerBridge.h/cpp  # JS command handling
```

## Known Limitations

1. **No textures yet**: Current shader uses simple diffuse lighting only
2. **Single mesh**: Does not assemble multi-part characters
3. **No animation**: Static pose only
4. **No skeletal skinning**: Vertex weights not processed
5. **Base64 transport**: Not optimal for high-frequency updates

## Future Work

- Texture loading from game resources
- Character assembly (body + head + armor)
- Animation playback
- FaceGen/FaceTint support
- Direct GPU texture sharing if PrismaUI API allows