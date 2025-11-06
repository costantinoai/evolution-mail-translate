# Evolution Mail Translation Extension - Architecture Analysis

## Overview
The Evolution Mail Translation Extension is a GNOME Evolution plugin that provides offline, privacy-preserving email translation using ArgosTranslate. All processing happens locally with zero data transmission.

---

## 1. TRANSLATION FLOW (User Action → Translation)

### Entry Points (User Actions)
1. **Menu Action**: Tools → Translate → Translate Message
2. **Keyboard Shortcut**: Ctrl+Shift+T (toggle translation on/off)
3. **Show Original**: Ctrl+Shift+O (restore original content)

### Translation Flow Diagram
```
User presses Ctrl+Shift+T
    ↓
action_translate_message_cb (translate-mail-ui.c)
    ↓
Check if already translated
  - If YES: Restore original (translate_dom_restore_original)
  - If NO: Continue
    ↓
Extract message body HTML (translate_content.c)
    ↓
translate_common_translate_async (translate-common.c)
    ├─ Get target language from GSettings
    ├─ Create provider instance ("argos")
    └─ Call translate_provider_translate_async
         ↓
TranslateProviderArgos.translate_async (translate-provider-argos.c)
    ├─ Locate Python helper (translate_runner.py)
    ├─ Locate Python binary (user venv)
    └─ Spawn subprocess with input piped to stdin
         ↓
translate_runner.py (Python helper)
    ├─ Parse command-line args (--target, --html, --install-on-demand)
    ├─ Detect source language (langdetect)
    ├─ Load/auto-download translation models (argostranslate)
    ├─ Translate content (preserving HTML structure)
    └─ Output JSON: {"translated": "..."}
         ↓
on_helper_done callback (translate-provider-argos.c)
    ├─ Extract "translated" field from JSON
    └─ Return via GTask
         ↓
on_translate_finished callback (translate-mail-ui.c)
    ↓
translate_dom_apply_to_shell_view (translate-dom.c)
    └─ Apply translated HTML to message display
```

---

## 2. ARCHITECTURE COMPONENTS

### A. C/GObject Layer (GNOME/Evolution Integration)

#### Module Registration (`translate-module.c`)
```c
e_module_load(GTypeModule *type_module)
  ├─ Register shell view extension
  ├─ Register browser extension
  ├─ Register Argos provider
  └─ Add provider to registry
```

**Location**: `/src/translate-module.c`

#### Provider Abstraction (Provider Pattern)

**Interface Definition** (`translate-provider.h`):
```c
GInterface TranslateProvider
  ├─ translate_async()        // async translation
  ├─ translate_finish()       // get result
  ├─ get_id()                 // provider identifier
  └─ get_name()               // human-readable name

Registry Functions:
  ├─ translate_provider_register(GType)      // register provider
  ├─ translate_provider_new_by_id(id)        // create provider instance
  └─ translate_provider_list_ids()           // list all providers
```

**Location**: `/src/providers/translate-provider.h/c`

**Argos Implementation** (`translate-provider-argos.c`):
- Provider ID: "argos"
- Provider Name: "Argos Translate (offline)"
- Implements async translation via subprocess spawning
- Currently the ONLY registered provider

**Location**: `/src/providers/translate-provider-argos.c`

#### UI Integration

**Shell View Extension** (`translate-shell-view-extension.c`):
- Hooks into Evolution shell view creation
- Filters for Mail view only
- Initializes UI components

**Mail UI** (`translate-mail-ui.c`):
- Adds "Translate" menu to main menu
- Adds toolbar button
- Defines keyboard shortcuts (Ctrl+Shift+T, Ctrl+Shift+O)
- Menu items:
  - Translate Message (Ctrl+Shift+T)
  - Show Original (Ctrl+Shift+O)
  - Translate Settings
- Action callbacks trigger the translation pipeline

**Location**: `/src/translate-mail-ui.c`, `/src/translate-shell-view-extension.c`

#### Content Extraction (`translate-content.c`)
```c
translate_get_selected_message_body_html_from_reader()
  ├─ Get selected message UID from folder
  ├─ Find best MIME part (HTML > Plain text)
  ├─ Decode to UTF-8
  └─ Convert plain text to HTML if needed
```

**Location**: `/src/translate-content.c`

#### DOM State Management (`translate-dom.c`)
- Stores original message state before translation
- Manages translation state per EMailDisplay
- Detects message changes to clear stale translations
- Applies/restores HTML content

**Location**: `/src/translate-dom.c`

#### Common Logic (`translate-common.c`)
Centralized translation request function:
```c
translate_common_translate_async(
  const gchar *body_html,
  GAsyncReadyCallback callback,
  gpointer user_data)
```
- Gets target language from settings
- Creates provider instance
- Initiates async translation
- No duplication between UI components

**Location**: `/src/translate-common.c`

#### Utilities (`translate-utils.c`)
```c
GSettings *translate_utils_get_settings()
gchar *translate_utils_get_target_language()
GSettings *translate_utils_get_provider_settings()
gboolean translate_utils_get_install_on_demand()
```

**Location**: `/src/translate-utils.c`

#### Preferences UI (`translate-preferences.c`)
Settings dialog with:
- Target language selector (27 languages)
- Provider display (read-only, currently "argos")
- Venv path entry (optional, not yet implemented)
- Install-on-demand checkbox

**Location**: `/src/translate-preferences.c`

### B. Python Helper Layer

#### translate_runner.py
**Purpose**: Offline translation orchestration using ArgosTranslate

**Entry Point**:
```bash
python3 translate_runner.py \
  --target <lang>              # Target language code (ISO 639-1)
  [--html | --text]            # Content type (default: text)
  [--install-on-demand]        # Auto-download models (default: true)
  [--debug]                    # Debug logging to /tmp/translate_debug.log
```

**Input**: Plain text or HTML via stdin

**Output**: JSON response
```json
{"translated": "translated content here"}
```

**Key Functions**:
```python
setup_gpu_acceleration()         # CUDA support if available
auto_download_model(from, to)    # Auto-download translation models
translate_html_carefully()       # HTML-aware translation with structure preservation
translate_offline()              # Main translation logic
main()                           # CLI entry point
```

**Location**: `/tools/translate/translate_runner.py`

**Supported Operations**:
1. Language detection (auto-detect source language)
2. Model auto-download (if enabled)
3. HTML parsing and selective translation (preserves tags, attributes, comments)
4. Plain text translation
5. GPU acceleration (CUDA)
6. Fallback to no-op if dependencies missing

---

## 3. CONFIGURATION & SETTINGS

### GSettings Schema (`org.gnome.evolution.translate`)
```xml
Key: target-language (string)
  Default: 'en'
  Description: Target language code (ISO 639-1)

Key: provider-id (string)
  Default: 'argos'
  Description: Active translation provider ID

Key: preserve-format (boolean)
  Default: true
  Description: Whether to preserve HTML structure
```

**Location**: `/data/gschema/org.gnome.evolution.translate.gschema.xml`

### Provider Settings (`org.gnome.evolution.translate.provider`)
```xml
Key: venv-path (string)
  Default: ''
  Description: Optional Python venv path override

Key: install-on-demand (boolean)
  Default: true
  Description: Auto-download missing translation models
```

### Environment Variables (Development Overrides)
```bash
TRANSLATE_HELPER_PATH      # Override translate_runner.py location
TRANSLATE_PYTHON_BIN       # Override Python interpreter
TRANSLATE_FAKE_UPPERCASE   # Fake translation (uppercase all text)
```

---

## 4. LOCATION OF TRANSLATION LOGIC

### Provider Instantiation
**File**: `/src/translate-common.c` (line 53)
```c
g_autoptr(GObject) provider_obj = translate_provider_new_by_id ("argos");
```

### Subprocess Spawning
**File**: `/src/providers/translate-provider-argos.c` (lines 228-246)
```c
GSubprocess *proc = g_subprocess_newv(
  argvv,
  G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
  &error);

g_subprocess_communicate_utf8_async(proc, payload, ...);
```

### Response Parsing
**File**: `/src/providers/translate-provider-argos.c` (lines 60-102)
```c
static gchar *extract_translated_field(const gchar *json)
```
Extracts "translated" field from JSON using json-glib

### Python Implementation
**File**: `/tools/translate/translate_runner.py` (lines 174-294)
```python
def translate_offline(text, target, is_html, install_on_demand=True)
```

---

## 5. CURRENT PROVIDER ANALYSIS

### Provider: ArgosTranslate (Offline)

**Characteristics**:
- Hardcoded as the default (only) provider
- ID: "argos"
- Name: "Argos Translate (offline)"
- Non-blocking async implementation
- Uses subprocess communication (stdin/stdout)

**Workflow**:
1. C code spawns Python helper as subprocess
2. Passes HTML/text via stdin
3. Receives JSON response via stdout
4. Extracts translated content
5. Returns via GTask callback

**Configuration**:
- Target language: GSettings
- Auto-download: GSettings (install-on-demand flag)
- Python venv: User location (~/.local/lib/evolution-translate/venv)
- Helper script: /usr/share/evolution-translate/translate/translate_runner.py

**Limitations**:
- Single provider (hardcoded provider ID in translate-common.c:53)
- No multi-provider support planned in current architecture
- Provider selection UI exists but non-functional (read-only in preferences)

---

## 6. KEY FILES SUMMARY

### C/GObject Files
| File | Purpose |
|------|---------|
| `translate-module.c` | Module entry point, provider registration |
| `translate-provider.h/c` | Provider interface & registry |
| `translate-provider-argos.h/c` | Argos implementation |
| `translate-shell-view-extension.c` | Evolution extension hook |
| `translate-mail-ui.c` | Menu, toolbar, keyboard shortcuts |
| `translate-common.c` | Centralized translation logic |
| `translate-dom.c` | State management, HTML display |
| `translate-content.c` | Message body extraction |
| `translate-preferences.c` | Settings dialog |
| `translate-utils.c` | GSettings helpers |

### Python Files
| File | Purpose |
|------|---------|
| `translate_runner.py` | Translation orchestration |
| `gpu_utils.py` | CUDA/GPU acceleration |
| `setup_models.py` | Model installation |
| `install_default_models.py` | Default model installer |

### Configuration Files
| File | Purpose |
|------|---------|
| `org.gnome.evolution.translate.gschema.xml` | Main settings schema |
| `CMakeLists.txt` | Build configuration |

---

## 7. TRANSLATION TRIGGERS & CONFIGURATION

### Trigger Points
1. **Menu**: Tools → Translate Message
2. **Keyboard**: Ctrl+Shift+T
3. **Show Original**: Ctrl+Shift+O
4. **Preferences**: Edit → Preferences → Translate Settings

### Configuration Methods
1. **GUI**: Preferences dialog
   - Target language dropdown (27 languages)
   - Install-on-demand toggle
   - Venv path (placeholder, not implemented)

2. **GSettings**: Direct schema access
   ```bash
   gsettings get org.gnome.evolution.translate target-language
   gsettings set org.gnome.evolution.translate target-language es
   ```

3. **Environment Variables**: Development overrides
   ```bash
   TRANSLATE_HELPER_PATH=/custom/path/translate_runner.py
   TRANSLATE_PYTHON_BIN=/custom/python/bin/python3
   TRANSLATE_FAKE_UPPERCASE=1
   ```

---

## 8. OVERALL ARCHITECTURE SUMMARY

### Layered Architecture
```
┌─────────────────────────────────────────────────┐
│ Evolution Mail Client (GNOME)                    │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│ Translation Extension Module (C/GObject)        │
├──────────────────────────────────────────────────┤
│ ┌──────────────────────────────────────────────┐ │
│ │ UI Layer                                      │ │
│ │ - Shell View Extension (hooks Evolution)    │ │
│ │ - Mail UI (menus, buttons, shortcuts)       │ │
│ │ - Preferences Dialog (settings UI)          │ │
│ │ - DOM Manager (HTML display state)          │ │
│ └────────────────┬─────────────────────────────┘ │
│                  │                                │
│ ┌────────────────▼─────────────────────────────┐ │
│ │ Translation Coordination Layer               │ │
│ │ - Common Logic (centralized requests)       │ │
│ │ - Content Extraction (MIME parsing)         │ │
│ │ - Utilities (GSettings, config)             │ │
│ └────────────────┬─────────────────────────────┘ │
│                  │                                │
│ ┌────────────────▼─────────────────────────────┐ │
│ │ Provider Abstraction Layer                   │ │
│ │ - Provider Interface (GInterface)            │ │
│ │ - Provider Registry                         │ │
│ │ - Argos Translate Provider                  │ │
│ └────────────────┬─────────────────────────────┘ │
└────────────────┬─────────────────────────────────┘
                 │ (subprocess communication)
                 │
┌────────────────▼─────────────────────────────────┐
│ Python Translation Helper                        │
├──────────────────────────────────────────────────┤
│ - translate_runner.py (orchestration)           │
│ - Language Detection (langdetect)               │
│ - ArgosTranslate (translation engine)           │
│ - Model Management (auto-download)              │
│ - GPU Acceleration (CUDA support)               │
│ - HTML Parser (BeautifulSoup)                   │
└────────────────┬─────────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────────┐
│ Translation Models & Libraries                   │
├──────────────────────────────────────────────────┤
│ - ArgosTranslate Models (offline)               │
│ - OpenNMT Models (via Argos)                    │
│ - CUDA/cuDNN (optional GPU acceleration)        │
└──────────────────────────────────────────────────┘
```

### Data Flow
```
Message HTML (EMailDisplay)
       ↓
Content Extraction (MIME/Camel API)
       ↓
Translate Request (GSettings for target lang)
       ↓
Provider Creation (Argos instance)
       ↓
Subprocess (Python helper via stdin)
       ↓
Language Detection + Model Loading
       ↓
Translation (ArgosTranslate engine)
       ↓
HTML Parser (structure preservation)
       ↓
JSON Response ({"translated": "..."})
       ↓
JSON Parsing (json-glib)
       ↓
DOM Update (EMailDisplay refresh)
       ↓
UI Display (formatted email in reader)
```

### Key Design Patterns
1. **Provider Pattern**: Extensible translation provider interface
2. **Async/Await Pattern**: Non-blocking GTask-based async operations
3. **Subprocess Pattern**: Python integration via subprocess pipes
4. **Observer Pattern**: Evolution extension mechanism
5. **Settings Pattern**: GSettings for configuration
6. **HTML Preservation**: BeautifulSoup for selective translation

---

## 9. HARDCODED VS. CONFIGURABLE ELEMENTS

### Hardcoded
- Provider ID: "argos" (in translate-common.c:53)
- Provider Name: "Argos Translate (offline)"
- Helper script location: /usr/share/evolution-translate/translate/translate_runner.py
- Menu structure (no menu customization)
- Keyboard shortcuts (fixed to Ctrl+Shift+T, Ctrl+Shift+O)

### Configurable
- Target language (27 languages via GSettings)
- Install-on-demand flag (GSettings)
- Python venv path (GSettings, but not used by default)
- Helper script override (TRANSLATE_HELPER_PATH env var)
- Python binary override (TRANSLATE_PYTHON_BIN env var)
- Debug mode (--debug flag in Python helper)

### Extensibility
- **Provider Interface**: New providers can be implemented
- **Provider Registry**: Dynamic provider registration
- **Future**: Multi-provider support could be added to GUI

---

## 10. DEPENDENCIES

### System Libraries
- GLib/GObject 2.0 (type system, async, signals)
- GTK 3+ (UI widgets)
- Evolution-Shell, Evolution-Mail, Evolution-Data-Server
- json-glib (JSON response parsing)
- libcamel (email MIME handling)

### Python Libraries
- argostranslate (translation models & engine)
- langdetect (language detection)
- beautifulsoup4 (HTML parsing)
- pycairo, pygobject (optional, for UI if needed)

### Optional
- CUDA/cuDNN (GPU acceleration)

---

## CONCLUSION

The Evolution Mail Translation Extension uses a clean, layered architecture with:
1. **Clean Separation**: C/GObject layer for Evolution integration, Python for translation logic
2. **Extensible Design**: Provider pattern allows multiple backends
3. **Async-First**: Non-blocking operations throughout
4. **Configuration-Driven**: GSettings for user preferences
5. **Privacy-Focused**: All processing offline, no external APIs
6. **Currently Single-Provider**: Argos is the hardcoded default, but architecture supports multiple providers

