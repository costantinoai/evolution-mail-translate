# Evolution Mail Translation - Architecture Diagrams

## 1. COMPLETE TRANSLATION FLOW (START TO END)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ USER INTERACTION LAYER                                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  User selects email in Evolution mail client                            │
│  ↓                                                                        │
│  User presses Ctrl+Shift+T (or clicks "Translate Message" menu)        │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ ACTION HANDLER (translate-mail-ui.c)                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  action_translate_message_cb()                                          │
│  ├─ Check if already translated?                                       │
│  │  ├─ YES → Call translate_dom_restore_original()                    │
│  │  └─ NO → Continue                                                   │
│  └─ Extract message body HTML                                          │
│     └─ Call get_selected_message_body_html()                           │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ CONTENT EXTRACTION (translate-content.c)                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  translate_get_selected_message_body_html_from_shell_view()            │
│  ├─ Get selected message UID from Camel folder                        │
│  ├─ Parse MIME structure (multipart analysis)                         │
│  ├─ Find best body part:                                              │
│  │  ├─ Prefer HTML part (text/html)                                   │
│  │  └─ Fall back to plain text (text/plain)                           │
│  ├─ Decode from original charset to UTF-8                            │
│  └─ If plain text: convert to simple HTML wrapper                     │
│     └─ Returns: HTML string ready for translation                     │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ CENTRALIZED TRANSLATION LOGIC (translate-common.c)                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  translate_common_translate_async(body_html, callback, user_data)      │
│  ├─ Validate input (non-empty HTML)                                   │
│  ├─ Get target language from GSettings                                │
│  │  └─ Key: "org.gnome.evolution.translate/target-language"          │
│  │  └─ Default: "en" (English)                                        │
│  ├─ Create provider instance:                                         │
│  │  └─ Call translate_provider_new_by_id("argos")  ← HARDCODED ID   │
│  │  └─ Returns: Argos provider GObject                                │
│  └─ Initiate async translation                                        │
│     └─ Call translate_provider_translate_async()                      │
│     └─ Pass: HTML, target_lang, callback                              │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ PROVIDER INTERFACE (translate-provider.h)                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GInterface TranslateProvider                                           │
│  ├─ translate_async()      ← Implementation in Argos provider          │
│  ├─ translate_finish()     ← Implementation in Argos provider          │
│  ├─ get_id()               → Returns "argos"                           │
│  └─ get_name()             → Returns "Argos Translate (offline)"      │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ ARGOS PROVIDER (translate-provider-argos.c)                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  tp_argos_translate_async()                                            │
│  ├─ Validate input & create GTask                                     │
│  ├─ Find helper script location (priority order):                     │
│  │  ├─ $TRANSLATE_HELPER_PATH (env override)                         │
│  │  ├─ /usr/share/evolution-translate/translate/translate_runner.py  │
│  │  └─ ~/.local/lib/evolution-translate/translate/translate_runner.py│
│  ├─ Find Python interpreter (priority order):                        │
│  │  ├─ $TRANSLATE_PYTHON_BIN (env override)                          │
│  │  └─ ~/.local/lib/evolution-translate/venv/bin/python              │
│  ├─ Get install-on-demand setting from GSettings                     │
│  │  └─ Key: "org.gnome.evolution.translate.provider/install-on-demand"
│  ├─ Build command line:                                              │
│  │  python translate_runner.py --target <lang>                       │
│  │                             --html                                 │
│  │                             [--install-on-demand | --no-...]      │
│  ├─ Spawn subprocess with pipes:                                     │
│  │  GSubprocess with:                                                │
│  │  ├─ G_SUBPROCESS_FLAGS_STDIN_PIPE (send HTML)                    │
│  │  └─ G_SUBPROCESS_FLAGS_STDOUT_PIPE (receive JSON)                │
│  └─ Send HTML via stdin                                              │
│     └─ Set callback: on_helper_done (async)                          │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
                    ╔──────────▼─────────╗
                    ║   SUBPROCESS       ║
                    ║   (Python Helper)  ║
                    ╚──────────┬─────────╝
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ PYTHON TRANSLATION HELPER (translate_runner.py)                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  main()                                                                 │
│  ├─ Parse command-line arguments                                      │
│  │  ├─ --target <lang>                                               │
│  │  ├─ --html or --text                                             │
│  │  ├─ --install-on-demand or --no-install-on-demand               │
│  │  └─ --debug (optional)                                           │
│  ├─ Read HTML from stdin                                             │
│  └─ Call translate_offline()                                         │
│                                                                          │
│  translate_offline(text, target, is_html, install_on_demand)         │
│  ├─ Setup GPU acceleration (CUDA if available)                      │
│  ├─ Detect source language (langdetect):                            │
│  │  ├─ For HTML: extract text content first                         │
│  │  └─ Auto-detect from content                                     │
│  ├─ Check installed translation models:                             │
│  │  └─ List available in argostranslate                             │
│  ├─ If models missing & install-on-demand:                          │
│  │  ├─ Update package index                                         │
│  │  ├─ Find translation package                                     │
│  │  ├─ Download model                                               │
│  │  └─ Install to ~/.local/share/argos-translate/packages/          │
│  ├─ Get translator from ArgosTranslate                              │
│  ├─ Translate content:                                              │
│  │  ├─ If HTML: Call translate_html_carefully()                    │
│  │  └─ If plain: Simple translator.translate()                     │
│  └─ Return result                                                    │
│                                                                          │
│  translate_html_carefully(translator, html_content)                   │
│  ├─ Parse HTML with BeautifulSoup (html.parser)                      │
│  ├─ Recursively process elements:                                    │
│  │  ├─ Skip: Comments, DOCTYPE, non-text nodes                      │
│  │  ├─ Skip: Very short text, numbers-only, symbols                │
│  │  └─ Translate: Text nodes only                                   │
│  ├─ Preserve whitespace (leading/trailing)                          │
│  ├─ Keep all HTML tags, attributes, structure intact                │
│  └─ Return modified HTML with translated text                       │
│                                                                          │
│  Output:                                                               │
│  └─ Write JSON to stdout:                                            │
│     {"translated": "translated HTML content here"}                   │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
                    ╔──────────▼─────────╗
                    ║   JSON RESPONSE    ║
                    ║  {"translated":".."}
                    ╚──────────┬─────────╝
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ RESPONSE HANDLING (translate-provider-argos.c)                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  on_helper_done() callback                                             │
│  ├─ Read stdout from subprocess                                       │
│  ├─ Check for errors (stderr)                                        │
│  ├─ Parse JSON response:                                             │
│  │  └─ Call extract_translated_field(json_string)                    │
│  │  └─ Uses json-glib library                                        │
│  │  └─ Extracts "translated" field value                             │
│  ├─ Return translated content via GTask                              │
│  └─ Call user callback (on_translate_finished)                       │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ UI UPDATE CALLBACK (translate-mail-ui.c)                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  on_translate_finished() callback                                      │
│  ├─ Finish provider async operation                                   │
│  ├─ Get translated HTML from result                                  │
│  └─ Call translate_dom_apply_to_shell_view()                         │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ DOM MANAGEMENT (translate-dom.c)                                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  translate_dom_apply_to_shell_view(shell_view, translated_html)       │
│  ├─ Get EMailDisplay from shell view                                 │
│  ├─ Save original message state:                                     │
│  │  ├─ Store original HTML                                          │
│  │  ├─ Store original MIME parts                                    │
│  │  └─ Store message UID for change detection                       │
│  ├─ Load translated HTML into display:                              │
│  │  └─ e_mail_display_set_part_list()                               │
│  └─ Mark as "translated" for toggle support                         │
│                                                                          │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────────┐
│ DISPLAY UPDATE                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Evolution email preview pane shows translated content                 │
│  User can:                                                              │
│  ├─ Read translated email                                            │
│  ├─ Press Ctrl+Shift+T again to restore original                     │
│  ├─ Press Ctrl+Shift+O to show original                              │
│  └─ Select another email (clears translation state)                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. PROVIDER PATTERN ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────────┐
│ PROVIDER REGISTRY & FACTORY PATTERN                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Provider Management Functions:                                │
│  ├─ translate_provider_register(GType)                         │
│  │  └─ Add new provider type to registry                      │
│  ├─ translate_provider_new_by_id(id)                           │
│  │  └─ Create instance by ID ("argos")                        │
│  └─ translate_provider_list_ids()                              │
│     └─ List all registered provider IDs                       │
│                                                                 │
│  Internal State:                                               │
│  └─ GHashTable: id → GType                                    │
│                                                                 │
└───┬────────────────────────────────────────────┬──────────────┘
    │                                            │
    │                                   ┌────────▼──────────┐
    │                                   │ Argos Provider    │
    │                                   │ Type: "argos"     │
    │                                   │                   │
    │                                   │ Registered in:    │
    │                                   │ translate-module.c│
    │                                   │ e_module_load()   │
    │                                   └────────┬──────────┘
    │                                            │
    │ ┌─────────────────────────────────────────▼──────────────┐
    │ │ GInterface TranslateProvider                            │
    │ ├─────────────────────────────────────────────────────────┤
    │ │                                                         │
    │ │  Required Methods (implementation by each provider):   │
    │ │  ├─ translate_async()                                 │
    │ │  │  └─ Async translation (non-blocking)              │
    │ │  ├─ translate_finish()                                │
    │ │  │  └─ Retrieve async result                         │
    │ │  ├─ get_id()                                          │
    │ │  │  └─ Return provider identifier                    │
    │ │  └─ get_name()                                        │
    │ │     └─ Return human-readable name                    │
    │ │                                                         │
    │ │  Interface Wrappers (implementation in translate-     │
    │ │  provider.c):                                         │
    │ │  ├─ translate_provider_translate_async()              │
    │ │  ├─ translate_provider_translate_finish()             │
    │ │  ├─ translate_provider_get_id()                       │
    │ │  └─ translate_provider_get_name()                     │
    │ │                                                         │
    │ └────────────────────────────────────────────────────────┘
    │
    └─────────────────────────────────────────────────┐
                                                      │
                    ┌─────────────────────────────────▼─────────┐
                    │ TranslateProviderArgos (Implementation)   │
                    ├──────────────────────────────────────────┤
                    │                                          │
                    │ struct _TranslateProviderArgos           │
                    │   GObject parent_instance;              │
                    │                                          │
                    │ Methods:                                 │
                    │ ├─ tp_argos_translate_async()            │
                    │ │  └─ Spawns Python subprocess         │
                    │ ├─ tp_argos_translate_finish()           │
                    │ │  └─ Extracts result from GTask       │
                    │ ├─ tp_argos_get_id()                     │
                    │ │  └─ Returns "argos"                   │
                    │ └─ tp_argos_get_name()                   │
                    │    └─ Returns "Argos Translate (offline)"│
                    │                                          │
                    │ Subprocess Communication:                │
                    │ ├─ Input: HTML via stdin                │
                    │ ├─ Output: JSON via stdout              │
                    │ └─ Helper: translate_runner.py          │
                    │                                          │
                    └──────────────────────────────────────────┘

EXTENSIBILITY EXAMPLE:
If you wanted to add a new provider (e.g., "google"):

1. Create: translate-provider-google.h/c
2. Define: TranslateProviderGoogle GObject
3. Implement: GoogleInterface (translate_async, translate_finish, etc.)
4. Register in e_module_load():
   translate_provider_google_type_register(type_module);
   translate_provider_register(TRANSLATE_TYPE_PROVIDER_GOOGLE);
5. Update translate-common.c to select provider:
   provider_obj = translate_provider_new_by_id("google");  // or keep "argos"
```

---

## 3. SETTINGS & CONFIGURATION FLOW

```
┌──────────────────────────────────────────────────────────────┐
│ GSettings Schema (org.gnome.evolution.translate)            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ ┌─ target-language (string, default: "en")                 │
│ │  └─ ISO 639-1 language code                             │
│ │     Used by: translate-utils.c:translate_utils_get_     │
│ │             target_language()                            │
│ │                                                           │
│ ├─ provider-id (string, default: "argos")                  │
│ │  └─ Currently hardcoded, not used by UI                 │
│ │                                                           │
│ └─ preserve-format (boolean, default: true)                │
│    └─ Currently unused (always passes --html to helper)   │
│                                                              │
│ GSettings Schema (org.gnome.evolution.translate.provider)  │
│                                                              │
│ ├─ venv-path (string, default: "")                         │
│ │  └─ Custom venv override (not yet implemented)          │
│ │                                                           │
│ └─ install-on-demand (boolean, default: true)             │
│    └─ Auto-download missing models                        │
│    └─ Used by: translate-utils.c:translate_utils_get_     │
│                install_on_demand()                         │
│                                                              │
└─────────────────────────┬──────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
  ┌───────────┐   ┌──────────────┐  ┌──────────────┐
  │ Util Code │   │ Provider     │  │ UI Prefs     │
  │           │   │ (C code)     │  │ Dialog       │
  │ translate-│   │              │  │              │
  │ utils.c   │   │ translate-   │  │ translate-   │
  │           │   │ provider-    │  │ preferences  │
  │ Read from │   │ argos.c      │  │              │
  │ GSettings │   │              │  │ Provides:    │
  │           │   │ Gets target  │  │ - Language   │
  │ Returns:  │   │ language     │  │   selector   │
  │ - target  │   │              │  │ - Provider   │
  │   lang    │   │ Gets install-│  │   (read-only)
  │ - install │   │ on-demand    │  │ - Venv path  │
  │   flag    │   │              │  │ - Install    │
  │           │   │ Uses for CLI │  │   toggle     │
  │           │   │ to helper    │  │              │
  │           │   │              │  │ Saves to:    │
  │           │   │              │  │ - target-    │
  │           │   │              │  │   language   │
  │           │   │              │  │ - install-   │
  │           │   │              │  │   on-demand  │
  └───────────┘   └──────────────┘  └──────────────┘

OVERRIDE MECHANISMS:

Environment Variables:
├─ TRANSLATE_HELPER_PATH
│  └─ Override translate_runner.py location
│  └─ Used: translate-provider-argos.c:lines 168-169
│
├─ TRANSLATE_PYTHON_BIN
│  └─ Override Python interpreter
│  └─ Used: translate-provider-argos.c:lines 198-199
│
└─ TRANSLATE_FAKE_UPPERCASE
   └─ Fake translation (debug mode)
   └─ Used: translate_runner.py:lines 192-201
```

---

## 4. SUBPROCESS COMMUNICATION DIAGRAM

```
┌──────────────────────────────────────────────────────────────┐
│ C CODE (translate-provider-argos.c)                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─ Build command:                                         │
│  │  python3 /usr/share/evolution-translate/translate/     │
│  │          translate_runner.py                           │
│  │          --target en                                   │
│  │          --html                                        │
│  │          --install-on-demand                           │
│  │                                                         │
│  └─ Create subprocess with pipes                          │
│     │                                                     │
│     │ stdin: G_SUBPROCESS_FLAGS_STDIN_PIPE                │
│     │        (C code writes to pipe)                      │
│     │        ↓                                            │
│     │   HTML CONTENT ───────────────→  Python helper     │
│     │                                 reads from stdin     │
│     │                                                     │
│     │ stdout: G_SUBPROCESS_FLAGS_STDOUT_PIPE              │
│     │         (C code reads from pipe)                    │
│     │                                                     │
│     │   Python helper             ←─────────────────────│
│     │   writes JSON to stdout      JSON RESPONSE         │
│     │   ({"translated": "..."})                          │
│     │                                                     │
│     └─ Async communication:                              │
│        g_subprocess_communicate_utf8_async()            │
│        ↓                                                 │
│        on_helper_done() callback when complete          │
│        ├─ Get stdout from subprocess                    │
│        ├─ Parse JSON response                          │
│        ├─ Extract "translated" field                   │
│        └─ Return to user callback                      │
│                                                         │
└──────────────────────────────────────────────────────────┘

DATA FORMAT:

INPUT (via stdin):
┌──────────────────────────────┐
│ Raw HTML content             │
│ No JSON wrapper required     │
│ Example:                     │
│                              │
│ <html>                       │
│   <body>                     │
│     <p>Hello world</p>       │
│   </body>                    │
│ </html>                      │
└──────────────────────────────┘

OUTPUT (via stdout):
┌──────────────────────────────┐
│ JSON wrapped response        │
│                              │
│ {"translated": "content"}    │
│                              │
│ Example:                     │
│                              │
│ {"translated": "<html>\n    │
│   <body>\n      <p>Hola     │
│   mundo</p>\n    </body>\n  │
│ </html>"}                    │
└──────────────────────────────┘
```

---

## 5. FILE ORGANIZATION SUMMARY

```
evolution-mail-translate/
├── src/                              ← C/GObject source
│   ├── translate-module.c            ← Plugin entry point
│   ├── translate-common.c            ← Centralized translation logic
│   ├── translate-mail-ui.c           ← UI: menus, buttons, shortcuts
│   ├── translate-shell-view-extension.c ← Evolution extension hook
│   ├── translate-dom.c               ← DOM state management
│   ├── translate-content.c           ← Content extraction
│   ├── translate-preferences.c       ← Settings dialog
│   ├── translate-utils.c             ← GSettings utilities
│   ├── m-utils.c                     ← Menu utilities
│   └── providers/
│       ├── translate-provider.h/c    ← Provider interface & registry
│       └── translate-provider-argos.h/c ← Argos implementation
│
├── tools/translate/                  ← Python helper
│   ├── translate_runner.py           ← Main translation orchestrator
│   ├── gpu_utils.py                  ← GPU acceleration
│   ├── setup_models.py               ← Model installation
│   └── install_default_models.py     ← Default models installer
│
├── data/gschema/                     ← GSettings schema
│   └── org.gnome.evolution.translate.gschema.xml
│
├── CMakeLists.txt                    ← Build configuration
└── debian/                           ← Debian package configuration
```

