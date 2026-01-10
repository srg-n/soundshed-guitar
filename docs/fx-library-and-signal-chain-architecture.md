# FX Library and Signal Chain Editor Architecture

## Overview

The FX Library and Signal Chain Editor system provides a drag-and-drop interface for building and modifying audio effect chains. The architecture consists of three main layers:

1. **Frontend UI** (TypeScript/HTML/CSS) - Visual editor and FX browser
2. **Message Protocol** (JSON) - Bidirectional communication between UI and plugin
3. **Backend DSP** (C++) - Audio processing and graph execution

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        Frontend UI                          │
│  ┌──────────────────┐         ┌─────────────────────────┐  │
│  │   FX Library     │         │  Signal Path Editor     │  │
│  │   (fxSelector)   │────────▶│  (signalPath)          │  │
│  │                  │ drag    │                         │  │
│  │  - Categories    │ drop    │  - Node Visualization   │  │
│  │  - Effect List   │         │  - Drag-Drop Handlers   │  │
│  │  - Search        │         │  - Parameter Panel      │  │
│  └──────────────────┘         └─────────────────────────┘  │
│           │                              │                  │
│           └──────────┬───────────────────┘                  │
│                      │ JSON Messages                        │
└──────────────────────┼──────────────────────────────────────┘
                       │
           ┌───────────▼──────────────┐
           │   Message Protocol       │
           │   (bridge.ts)            │
           │                          │
           │   - addNode             │
           │   - replaceNode         │
           │   - reorderNode         │
           │   - deleteNode          │
           │   - updateNodeParam     │
           │   - state (broadcast)   │
           └───────────┬──────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│                     Backend Plugin                          │
│  ┌──────────────────┐         ┌─────────────────────────┐  │
│  │  GuitarFXPlugin  │         │   GraphDSPManager       │  │
│  │                  │         │                         │  │
│  │  - UI Messages   │────────▶│  - Preset Loading       │  │
│  │  - State Mgmt    │         │  - Resource Resolution  │  │
│  │  - Serialization │         │  - DSP Preparation      │  │
│  └──────────────────┘         └──────────┬──────────────┘  │
│                                           │                 │
│                                ┌──────────▼──────────────┐  │
│                                │ SignalGraphExecutor     │  │
│                                │                         │  │
│                                │ - Topological Sort      │  │
│                                │ - Buffer Management     │  │
│                                │ - Effect Processors     │  │
│                                │ - Audio Processing      │  │
│                                └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Frontend Components

### 1. FX Library (`src/resources/ui/ts/fxSelector.ts`)

The FX Library provides a categorized browser for available effects.

#### Key Responsibilities:
- **Category Navigation**: 8 categories (Dynamics, Amp, Cab, EQ, Modulation, Delay, Reverb, Utility)
- **Effect Listing**: Displays effects from `EffectTypeRegistry`
- **Search Functionality**: Real-time filtering across all effects
- **Drag Initiation**: Starts drag operation with effect type data

#### DOM Structure:
```html
<section class="fx-selector-panel">
  <div class="fx-selector-categories">
    <!-- Category tabs rendered by renderCategories() -->
  </div>
  <div class="fx-selector-effects-list">
    <!-- Effect items rendered by renderEffectsList() -->
    <div class="fx-item" 
         data-effect-type="dynamics_gate" 
         draggable="true">
      <!-- Effect details -->
    </div>
  </div>
</section>
```

#### Initialization Flow:
```typescript
// 1. Called from main.ts on startup
initFxSelector()
  ├─ Binds search input handler
  ├─ renderCategories() - Creates category tabs
  └─ renderEffectsList() - Populates effects for active category
      └─ bindFxItemDragHandlers() - Attaches drag handlers to items
```

#### Drag Event Handling:
```typescript
el.addEventListener("dragstart", (e: DragEvent) => {
  // Set effect type in drag data
  e.dataTransfer.setData("application/x-fx-effect", effectType);
  e.dataTransfer.effectAllowed = "copy";
  
  // Add visual class for CSS styling
  document.body.classList.add("fx-dragging");
});
```

**Data Transfer Format**: The key `"application/x-fx-effect"` contains the effect type string (e.g., `"dynamics_gate"`, `"amp_nam"`).

### 2. Signal Path Editor (`src/resources/ui/ts/signalPath.ts`)

The Signal Path Editor visualizes and allows manipulation of the effect chain.

#### Key Responsibilities:
- **Graph Visualization**: Renders nodes and connectors from preset graph
- **Drop Zone Handling**: Accepts FX library items and reordering operations
- **Parameter Editing**: Side panel for adjusting effect parameters
- **Node Selection**: Click to select, keyboard to delete

#### Rendering Pipeline:
```typescript
renderSignalPathBar()
  └─ renderGraphSignalPath(preset)
      ├─ buildGraphLayout(nodes, edges) - Creates visual structure
      ├─ renderSegments(segments) - Generates HTML
      │   ├─ renderNodeElement(node) - Individual effect nodes
      │   └─ renderParallelBranches() - Handles splits/joins
      ├─ bindNodeClickHandlers(preset) - Node interaction
      └─ bindConnectorDropHandlers(preset) - Connector drops
```

#### Signal Path Node Structure:
Each node in the graph has:
```typescript
interface SignalPathNode {
  id: string;              // Unique identifier (e.g., "gate_1234")
  type: string;            // Effect type (e.g., "dynamics_gate")
  category: string;        // Category (e.g., "dynamics")
  displayName: string;     // UI label
  bypassed: boolean;       // Enable/disable state
  params: Record<string, number>;  // Numeric parameters
  config: Record<string, string>;  // String configuration
  resource?: ResourceRef;  // Optional NAM model or IR reference
}
```

#### Drop Zone Types:

**1. Signal Path Node Drop Zone** (Replace existing effect):
```typescript
el.addEventListener("drop", (e: DragEvent) => {
  const fxEffectType = e.dataTransfer?.getData("application/x-fx-effect");
  const targetNode = preset.graph.nodes.find(n => n.id === targetNodeId);
  const effectTypeInfo = EffectTypeRegistry.get(fxEffectType);
  
  // Only allow same-category replacement
  if (targetNode.category === effectTypeInfo.category) {
    sendReplaceSignalPathNode(targetNodeId, fxEffectType);
  }
});
```

**2. Connector Drop Zone** (Insert new effect):
```typescript
el.addEventListener("drop", (e: DragEvent) => {
  const fxEffectType = e.dataTransfer?.getData("application/x-fx-effect");
  
  // Find node before this connector
  const prevNode = findNodeBeforeConnector(el, preset);
  const insertAfter = prevNode?.id || "__input__";
  
  sendAddNode(fxEffectType, insertAfter);
});
```

#### Helper: `findSignalPathNodeBeforeConnector()`
Traverses DOM siblings to determine insertion point:
```typescript
function findSignalPathNodeBeforeConnector(connectorEl, preset) {
  let prevSibling = connectorEl.previousElementSibling;
  
  while (prevSibling) {
    if (prevSibling.classList.contains("signal-node")) {
      // Found a node - check if it's input or regular node
      if (prevSibling.classList.contains("input-node")) {
        return null; // Insert after __input__
      }
      return preset.graph.nodes.find(n => n.id === nodeId);
    }
    prevSibling = prevSibling.previousElementSibling;
  }
  return null;
}
```

### 3. Effect Type Registry (`src/resources/ui/ts/presetV2.ts`)

Maintains metadata about available effects on the frontend.

```typescript
class EffectRegistry {
  private types = Map<string, EffectTypeInfo>;
  
  register(type: string, info: EffectTypeInfo): void;
  get(type: string): EffectTypeInfo | undefined;
  getByCategory(category: string): EffectTypeInfo[];
  getAll(): EffectTypeInfo[];
}

interface EffectTypeInfo {
  type: string;              // "dynamics_gate"
  displayName: string;       // "Noise Gate"
  category: string;          // "dynamics"
  requiresResource: boolean; // true for amp_nam, ir_cab
  resourceType?: string;     // "nam" or "ir"
  parameters: ParameterDef[];
}
```

## Message Protocol

Communication between UI and plugin uses JSON messages via the WebView bridge.

### UI → Plugin Messages

**1. addSignalPathNode** - Insert new effect:
```typescript
{
  type: "addSignalPathNode",
  effectType: "dynamics_gate",  // Effect type from registry
  insertAfter: "amp_1"          // Signal path node ID or "__input__"
}
```

**2. replaceSignalPathNode** - Replace existing effect:
```typescript
{
  type: "replaceSignalPathNode",
  nodeId: "amp_1",              // Signal path node to replace
  newEffectType: "amp_crunch"   // New effect type (same category)
}
```

**3. reorderSignalPathNode** - Move effect position:
```typescript
{
  type: "reorderSignalPathNode",
  nodeId: "eq_1",               // Signal path node to move
  targetNodeId: "amp_1"         // Insert after this signal path node
}
```

**4. deleteSignalPathNode** - Remove effect:
```typescript
{
  type: "deleteSignalPathNode",
  nodeId: "delay_1"             // Signal path node to remove
}
```

**5. updateSignalPathNodeParam** - Change parameter value:
```typescript
{
  type: "updateSignalPathNodeParam",
  nodeId: "gate_1",
  paramKey: "threshold",
  value: -45.0
}
```

**6. updateSignalPathNodeBypass** - Toggle effect on/off:
```typescript
{
  type: "updateSignalPathNodeBypass",
  nodeId: "reverb_1",
  bypassed: true
}
```

### Plugin → UI Messages

**state** - Full state broadcast:
```typescript
{
  type: "state",
  activePresetId: "preset-123",
  presets: [
    {
      id: "preset-123",
      name: "My Preset",
      version: 2,
      graph: {
        nodes: [...],
        edges: [...]
      },
      global: {...},
      embeddedResources: []
    }
  ],
  libraries: {
    nam: [...],
    ir: [...]
  }
}
```

The state message is broadcast after every graph modification, causing the UI to re-render.

## Backend Components

### 1. GuitarFXPlugin (`src/src/GuitarFXPlugin.cpp`)

Main plugin class that handles UI messages and coordinates DSP.

#### Message Handler Flow:
```cpp
HandleUIMessage(message)
  ├─ Parse JSON payload
  ├─ Route to specific handler based on "type"
  └─ Handler functions:
      ├─ HandleAddSignalPathNodeRequest()
      ├─ HandleReplaceSignalPathNodeRequest()
      ├─ HandleReorderSignalPathNodeRequest()
      ├─ HandleDeleteSignalPathNodeRequest()
      ├─ HandleUpdateSignalPathNodeParamRequest()
      └─ HandleUpdateSignalPathNodeBypassRequest()
```

#### Example: HandleAddSignalPathNodeRequest()
```cpp
void GuitarFXPlugin::HandleAddSignalPathNodeRequest(const json& payload) {
  const string effectType = payload["effectType"];
  const string insertAfter = payload["insertAfter"];
  
  // 1. Create new signal path node with default parameters
  SignalPathNode newNode;
  newNode.id = effectType + "_" + generateTimestamp();
  newNode.type = effectType;
  newNode.enabled = true;
  
  // 2. Get metadata from EffectRegistry
  auto effectInfo = EffectRegistry::Instance().GetTypeInfo(effectType);
  if (effectInfo) {
    newNode.category = effectInfo->category;
    newNode.label = effectInfo->displayName;
    for (const auto& param : effectInfo->parameters) {
      newNode.params[param.id] = param.defaultValue;
    }
  }
  
  // 3. Update graph structure - split edge
  auto edgeIt = find_if(edges.begin(), edges.end(),
    [&](const GraphEdge& e) { return e.from == insertAfter; });
  
  string nextNodeId = edgeIt->to;
  edgeIt->to = newNode.id;  // Point old edge to new node
  
  // Add new edge from new node to next node
  edges.push_back({newNode.id, nextNodeId, 0, 0, 1.0});
  
  // 4. Add node to graph
  mActivePreset->graph.nodes.push_back(newNode);
  
  // 5. Persist and apply changes
  mActivePresetJson = PresetStorage::SerializeToJson(*mActivePreset);
  ApplyPreset(*mActivePreset);
  BroadcastState();
}
```

#### Example: HandleReplaceSignalPathNodeRequest()
```cpp
void GuitarFXPlugin::HandleReplaceSignalPathNodeRequest(const json& payload) {
  const string nodeId = payload["nodeId"];
  const string newEffectType = payload["newEffectType"];
  
  // 1. Find signal path node in graph
  SignalPathNode* node = mActivePreset->graph.FindSignalPathNode(nodeId);
  
  // 2. Validate category match (safety check)
  auto oldInfo = EffectRegistry::Instance().GetTypeInfo(node->type);
  auto newInfo = EffectRegistry::Instance().GetTypeInfo(newEffectType);
  if (oldInfo->category != newInfo->category) {
    ReportErrorToUI("Cannot replace with different category");
    return;
  }
  
  // 3. Replace type and reset parameters
  node->type = newEffectType;
  node->label = newInfo->displayName;
  node->category = newInfo->category;
  node->params.clear();
  
  // Set defaults from new effect
  for (const auto& param : newInfo->parameters) {
    node->params[param.id] = param.defaultValue;
  }
  
  // 4. Apply changes
  mActivePresetJson = PresetStorage::SerializeToJson(*mActivePreset);
  ApplyPreset(*mActivePreset);
  BroadcastState();
}
```

### 2. GraphDSPManager (`src/src/dsp/GraphDSPManager.h`)

Manages the DSP graph and coordinates audio processing.

#### Key Responsibilities:
- Load and validate presets
- Resolve resource references (NAM models, IRs)
- Prepare SignalGraphExecutor
- Process audio through the graph

#### Preset Loading Flow:
```cpp
bool GraphDSPManager::LoadPreset(const Preset& preset) {
  mCurrentPreset = preset;
  
  // 1. Create new executor
  mExecutor = make_unique<SignalGraphExecutor>();
  mExecutor->SetGraph(preset.graph);
  mExecutor->SetResourceLibrary(mResourceLibrary.get());
  
  // 2. Apply global settings
  mInputTrim = preset.global.inputTrim;
  mOutputTrim = preset.global.outputTrim;
  
  // 3. Resolve resources for all nodes
  ResolveResources(preset);
  
  // 4. Prepare for audio processing
  if (mSampleRate > 0) {
    mExecutor->Prepare(mSampleRate, mMaxBlockSize);
  }
  
  return true;
}
```

#### Resource Resolution:
```cpp
void GraphDSPManager::ResolveResources(const Preset& preset) {
  for (const auto& node : preset.graph.nodes) {
    if (!node.resource) continue;
    
    const auto& ref = *node.resource;
    filesystem::path resourcePath;
    
    // Handle different reference types
    if (ref.IsLibrary()) {
      // Library resource (e.g., "nam", "plexi-bright")
      resourcePath = mResourceLibrary->ResolvePath(
        ref.type, ref.id);
    }
    else if (ref.IsFilePath()) {
      // Direct file path
      resourcePath = ref.filePath;
    }
    else if (ref.IsEmbedded()) {
      // Embedded resource (portable presets)
      // TODO: Materialize from preset.embeddedResources
    }
    
    if (exists(resourcePath)) {
      mExecutor->LoadNodeResource(node.id, ref);
    }
  }
}
```

### 3. SignalGraphExecutor (`src/src/dsp/SignalGraphExecutor.cpp`)

Executes the audio processing graph.

#### Key Responsibilities:
- Topological sort for execution order
- Create effect processor instances
- Manage audio buffers
- Process audio through the graph

#### Initialization Flow:
```cpp
void SignalGraphExecutor::SetGraph(const SignalGraph& graph) {
  mGraph = graph;
  
  // 1. Validate graph structure
  ValidateGraph();
  
  // 2. Determine execution order (topological sort)
  BuildExecutionOrder();
  
  // 3. Create processor instances
  CreateProcessors();
}

void SignalGraphExecutor::BuildExecutionOrder() {
  // Kahn's algorithm for topological sort
  vector<string> order;
  map<string, int> inDegree;
  
  // Calculate in-degrees
  for (const auto& edge : mGraph.edges) {
    inDegree[edge.to]++;
  }
  
  // Start with nodes that have no incoming edges
  queue<string> queue;
  for (const auto& node : mGraph.nodes) {
    if (inDegree[node.id] == 0) {
      queue.push(node.id);
    }
  }
  
  // Process queue
  while (!queue.empty()) {
    string nodeId = queue.front();
    queue.pop();
    order.push_back(nodeId);
    
    // Reduce in-degree for neighbors
    for (const auto& edge : mGraph.edges) {
      if (edge.from == nodeId) {
        if (--inDegree[edge.to] == 0) {
          queue.push(edge.to);
        }
      }
    }
  }
  
  mExecutionOrder = order;
}

void SignalGraphExecutor::CreateProcessors() {
  for (const auto& node : mGraph.nodes) {
    auto processor = EffectRegistry::Instance().Create(node.type);
    if (processor) {
      mProcessors[node.id] = move(processor);
    }
  }
}
```

#### Audio Processing Loop:
```cpp
void SignalGraphExecutor::Process(float** inputs, 
                                   float** outputs, 
                                   int numSamples) {
  // 1. Copy input to first buffer
  copy(inputs[0], mBuffers["in_0"], numSamples);
  
  // 2. Process nodes in topological order
  for (const auto& nodeId : mExecutionOrder) {
    const auto& node = FindNode(nodeId);
    auto* processor = mProcessors[nodeId].get();
    
    if (!node->enabled) {
      // Bypass: copy input to output
      copy(GetInputBuffer(nodeId), GetOutputBuffer(nodeId), numSamples);
      continue;
    }
    
    if (node->type == kNodeTypeInput || node->type == kNodeTypeOutput) {
      continue; // Special nodes
    }
    
    // Get input/output buffers
    float* inputBuffer = GetInputBuffer(nodeId);
    float* outputBuffer = GetOutputBuffer(nodeId);
    
    // Process
    processor->Process(inputBuffer, outputBuffer, numSamples);
  }
  
  // 3. Copy final output
  copy(mBuffers["out_0"], outputs[0], numSamples);
}
```

### 4. EffectRegistry (Backend) (`src/src/dsp/EffectRegistry.cpp`)

Singleton registry for effect types and factories.

```cpp
class EffectRegistry {
public:
  static EffectRegistry& Instance();
  
  // Registration (called at startup via REGISTER_EFFECT macro)
  void Register(const string& type, 
                const EffectTypeInfo& info,
                EffectFactory factory);
  
  // Factory
  unique_ptr<EffectProcessor> Create(const string& type) const;
  
  // Queries
  optional<EffectTypeInfo> GetTypeInfo(const string& type) const;
  vector<EffectTypeInfo> GetAllTypes() const;
  vector<EffectTypeInfo> GetTypesByCategory(const string& cat) const;
};
```

#### Registration Example:
```cpp
// In effect implementation file
REGISTER_EFFECT(
  NoiseGateProcessor,          // Class name
  "dynamics_gate",             // Type ID
  "Noise Gate",                // Display name
  "dynamics",                  // Category
  "Reduces noise below threshold",  // Description
  false                        // Requires resource?
)
```

This macro expands to create a static registrar that runs on startup.

## Complete Data Flow Example

### Scenario: User drags "Chorus" effect to signal chain

**1. Frontend - Drag Start:**
```typescript
// fxSelector.ts
el.addEventListener("dragstart", (e) => {
  e.dataTransfer.setData("application/x-fx-effect", "chorus_analog");
  document.body.classList.add("fx-dragging");
});
```

**2. Frontend - Drop on Connector:**
```typescript
// signalPath.ts
el.addEventListener("drop", (e) => {
  const fxEffectType = e.dataTransfer.getData("application/x-fx-effect");
  // fxEffectType = "chorus_analog"
  
  const prevNode = findSignalPathNodeBeforeConnector(el, preset);
  // prevNode.id = "amp_1"
  
  sendAddSignalPathNode("chorus_analog", "amp_1");
});
```

**3. Frontend - Send Message:**
```typescript
// fxSelector.ts
export function sendAddSignalPathNode(effectType, insertAfter) {
  postMessage({
    type: "addSignalPathNode",
    effectType: "chorus_analog",
    insertAfter: "amp_1"
  });
}
```

**4. Backend - Receive Message:**
```cpp
// GuitarFXPlugin.cpp
void GuitarFXPlugin::HandleUIMessage(const string& message) {
  auto payload = json::parse(message);
  const string type = payload["type"];
  
  if (type == "addSignalPathNode") {
    HandleAddSignalPathNodeRequest(payload);
  }
}
```

**5. Backend - Create Signal Path Node:**
```cpp
void GuitarFXPlugin::HandleAddSignalPathNodeRequest(const json& payload) {
  // Create signal path node
  SignalPathNode newNode;
  newNode.id = "chorus_analog_1673456789";
  newNode.type = "chorus_analog";
  newNode.category = "modulation";
  newNode.label = "Analog Chorus";
  newNode.params = {
    {"rate", 0.5},
    {"depth", 0.3},
    {"mix", 0.5}
  };
  
  // Update graph edges
  // Before: amp_1 → cab_1
  // After:  amp_1 → chorus_1 → cab_1
  
  // Add to preset
  mActivePreset->graph.nodes.push_back(newNode);
}
```

**6. Backend - Apply Changes:**
```cpp
  // Serialize updated preset
  mActivePresetJson = PresetStorage::SerializeToJson(*mActivePreset);
  
  // Load into DSP
  ApplyPreset(*mActivePreset);
    └─ mDSP->LoadPreset(*mActivePreset)
        └─ GraphDSPManager::LoadPreset()
            ├─ Create new SignalGraphExecutor
            ├─ SetGraph(preset.graph)
            │   ├─ ValidateGraph()
            │   ├─ BuildExecutionOrder()  // Topological sort
            │   └─ CreateProcessors()     // Instantiate effects
            └─ Prepare(sampleRate, blockSize)
                └─ For each processor:
                    processor->Prepare(sampleRate, blockSize)
  
  // Notify UI
  BroadcastState();
```

**7. Frontend - Receive State:**
```typescript
// messages.ts
handleIncomingMessage(message) {
  if (message.type === "state") {
    uiState.presets = message.presets;
    uiState.activePresetId = message.activePresetId;
    renderSignalPathBar();  // Re-render with new node
  }
}
```

**8. Audio Processing:**
```cpp
// Real-time audio thread
void GuitarFXPlugin::ProcessBlock(iplug::sample** inputs,
                                   iplug::sample** outputs,
                                   int numSamples) {
  if (mDSP && mDSP->HasPreset()) {
    mDSP->Process(inputs, outputs, numSamples);
      └─ SignalGraphExecutor::Process()
          ├─ Process in execution order:
          │   ["in", "gate_1", "amp_1", "chorus_1", "cab_1", "out"]
          │
          ├─ For each node:
          │   ├─ Get input buffer (from previous node)
          │   ├─ Get output buffer (to next node)
          │   └─ processor->Process(in, out, numSamples)
          │
          └─ Copy final output to plugin outputs
  }
}
```

## Graph Edge Management

The graph maintains connectivity through edges. Each operation must preserve graph validity.

### Edge Structure:
```cpp
struct GraphEdge {
  string from;      // Source node ID
  string to;        // Destination node ID
  int fromPort;     // Output port (0 for mono, 0-1 for stereo)
  int toPort;       // Input port
  double gain;      // Edge gain multiplier
};
```

### Graph Invariants:
1. Every signal path node (except input/output) must have at least one incoming and one outgoing edge
2. No cycles allowed (DAG - Directed Acyclic Graph)
3. Input node is always the source
4. Output node is always the sink
5. All edges must reference valid signal path node IDs

### Operation: Insert Signal Path Node
```
Before:  A → B

After:   A → NEW → B

Edges:
  Remove: {from: A, to: B}
  Add:    {from: A, to: NEW}
          {from: NEW, to: B}
```

### Operation: Remove Signal Path Node
```
Before:  A → REMOVED → B

After:   A → B

Edges:
  Remove: {from: A, to: REMOVED}
          {from: REMOVED, to: B}
  Add:    {from: A, to: B}
```

### Operation: Reorder Signal Path Node
```
Before:  A → MOVED → B → C

After:   A → B → MOVED → C

Steps:
  1. Remove MOVED from current position (reconnect A → B)
  2. Insert MOVED at new position (split B → C)
```

## Parameter Management

Parameters flow from UI to DSP in real-time.

### Parameter Update Flow:
```typescript
// UI Slider Change
slider.addEventListener("input", (e) => {
  const value = e.target.value;
  
  sendSignalPathNodeParamUpdate("gate_1", "threshold", -45.0);
    ↓
  postMessage({
    type: "updateSignalPathNodeParam",
    nodeId: "gate_1",
    paramKey: "threshold",
    value: -45.0
  });
});
```

```cpp
// Backend Handler
void HandleUpdateSignalPathNodeParamRequest(const json& payload) {
  const string nodeId = payload["nodeId"];
  const string paramKey = payload["paramKey"];
  const double value = payload["value"];
  
  // 1. Update preset storage
  SignalPathNode* node = mActivePreset->graph.FindSignalPathNode(nodeId);
  node->params[paramKey] = value;
  
  // 2. Update DSP (without full reload)
  ApplyNodeParameter(*node, paramKey, value);
    └─ Maps to specific plugin parameter or
       forwards to SignalGraphExecutor
  
  // Note: State is NOT broadcast for parameter changes
  // (too frequent, would cause UI jank)
}
```

```cpp
// DSP Update
SignalGraphExecutor::SetSignalPathNodeParam(nodeId, key, value) {
  auto* processor = mProcessors[nodeId].get();
  processor->SetParameter(key, value);
    └─ Effect processor applies parameter atomically
       (safe for real-time audio thread)
}
```

## Resource Management

Resources (NAM models, IR files) are loaded separately from graph structure.

### Resource Reference Types:

**1. Library Resource:**
```json
{
  "type": "nam",
  "id": "plexi-bright"
}
```
Resolved via ResourceLibrary to: `resources/amps/plexi-bright.nam`

**2. File Path:**
```json
{
  "filePath": "C:/Users/user/models/custom-amp.nam"
}
```
Direct file system path.

**3. Embedded Resource:**
```json
{
  "embeddedId": "emb-model-123"
}
```
References entry in `preset.embeddedResources[]` for portable sharing.

### Resource Loading:
```cpp
void SignalGraphExecutor::LoadSignalPathNodeResource(const string& nodeId,
                                            const ResourceRef& ref) {
  auto* processor = mProcessors[nodeId].get();
  
  if (auto* namProcessor = dynamic_cast<NAMProcessor*>(processor)) {
    namProcessor->LoadModel(resourcePath);
  }
  else if (auto* irProcessor = dynamic_cast<IRConvolutionProcessor*>(processor)) {
    irProcessor->LoadImpulseResponse(resourcePath);
  }
}
```

## Error Handling

Errors are reported back to the UI for display.

```cpp
void GuitarFXPlugin::ReportErrorToUI(string_view message,
                                      string_view detail) {
  json errorMsg;
  errorMsg["type"] = "error";
  errorMsg["message"] = message;
  errorMsg["detail"] = detail;
  
  SendMessageToUI(errorMsg.dump());
}
```

```typescript
// messages.ts
if (message.type === "error") {
  showNotification(message.message, "error");
  console.error("[Plugin Error]", message.message, message.detail);
}
```

## Performance Considerations

### Frontend:
- **Debounced Search**: Search input uses debouncing to avoid excessive filtering
- **Virtual Scrolling**: Could be added for large effect lists
- **Optimistic Updates**: UI updates immediately, then syncs with backend state

### Backend:
- **Lazy Resource Loading**: Resources loaded only when needed
- **Buffer Reuse**: SignalGraphExecutor reuses audio buffers
- **Lock-Free Parameters**: Parameter updates use atomic operations
- **Topological Sort Cache**: Execution order computed once per graph change

### Audio Thread Safety:
- Graph modifications happen on UI thread
- DSP swaps executor atomically
- Parameters use lock-free updates
- No allocations in Process() loop

## Testing

Key areas to test:

1. **Graph Integrity**: Verify edges remain valid after all operations
2. **Audio Processing**: Ensure no glitches during graph modifications
3. **Parameter Updates**: Test parameter changes don't cause dropouts
4. **Resource Loading**: Verify models/IRs load correctly
5. **State Persistence**: Test save/load preserves graph structure
6. **Error Recovery**: Invalid operations should not crash

## Future Enhancements

- **Parallel Paths**: Support for split/mix topologies
- **Undo/Redo**: Stack of graph operations
- **Drag Reordering**: Drag existing nodes to reorder
- **Visual Routing**: Click-and-drag edge creation
- **Effect Presets**: Save/load settings for individual effects
- **CPU Metering**: Show per-effect CPU usage

## File Reference

**Frontend:**
- `src/resources/ui/ts/fxSelector.ts` - FX Library component
- `src/resources/ui/ts/signalPath.ts` - Signal path editor
- `src/resources/ui/ts/presetV2.ts` - Graph manipulation utilities
- `src/resources/ui/ts/messages.ts` - Message handler
- `src/resources/ui/ts/bridge.ts` - WebView communication

**Backend:**
- `src/src/GuitarFXPlugin.cpp/.h` - Main plugin class
- `src/src/dsp/GraphDSPManager.h` - DSP coordination
- `src/src/dsp/SignalGraphExecutor.cpp/.h` - Graph execution
- `src/src/dsp/EffectRegistry.cpp/.h` - Effect type registry
- `src/src/presets/PresetTypes.h` - Data structures
- `src/src/presets/PresetStorage.cpp/.h` - Serialization

**Styles:**
- `src/resources/ui/css/signal-path.css` - Signal path styling
- `src/resources/ui/css/fx-selector.css` - FX library styling
- `src/resources/ui/css/base.css` - Global drag states
