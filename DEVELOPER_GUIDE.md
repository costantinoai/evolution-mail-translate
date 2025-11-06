# Evolution Translation Extension - Developer Guide

Complete technical documentation for developers, contributors, and maintainers.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Project Structure](#project-structure)
- [Code Architecture Details](#code-architecture-details)
- [Design Patterns](#design-patterns)
- [Data Flow](#data-flow)
- [Adding New Features](#adding-new-features)
- [Python Backend](#python-backend)
- [Build System](#build-system)
- [Testing](#testing)
- [Packaging and Releases](#packaging-and-releases)
- [Best Practices](#best-practices)
- [Troubleshooting Development](#troubleshooting-development)

---

## Architecture Overview

The Evolution Translation Extension follows a modular, provider-based architecture that cleanly separates UI, business logic, and translation providers.

### Key Design Principles
- **DRY (Don't Repeat Yourself)**: Centralized translation logic, shared utilities
- **Provider Pattern**: Extensible translation backend system
- **Async-First**: Non-blocking UI operations
- **Memory Safety**: Extensive use of GLib auto-cleanup
- **HTML-Aware**: Preserves email formatting during translation

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Evolution Mail Client                     │
├─────────────────────────────────────────────────────────────┤
│  UI Layer (Mail View & Browser Windows)                     │
│  ├─ translate-mail-ui.c (Main mail view integration)        │
│  └─ translate-browser-extension.c (Separate window support) │
├─────────────────────────────────────────────────────────────┤
│  Common Logic Layer                                          │
│  ├─ translate-common.c (Shared translation logic)           │
│  ├─ translate-utils.c (Utility functions)                   │
│  ├─ translate-content.c (Email body extraction)             │
│  ├─ translate-dom.c (DOM manipulation & state)              │
│  └─ translate-preferences.c (Settings UI)                   │
├─────────────────────────────────────────────────────────────┤
│  Provider Layer (Abstract translation interface)            │
│  ├─ translate-provider.h (GInterface definition)            │
│  └─ providers/translate-provider-argos.c (Implementation)   │
├─────────────────────────────────────────────────────────────┤
│  Python Translation Backend                                  │
│  └─ tools/translate/translate_runner.py                     │
│     ├─ ArgosTranslate integration                           │
│     ├─ HTML-aware translation                               │
│     └─ Language auto-detection                              │
└─────────────────────────────────────────────────────────────┘
```

## Project Structure

```
translate-extension/
├── src/                          # C source files
│   ├── example-module.c          # Entry point (EModule)
│   ├── translate-common.{c,h}    # Shared translation logic [NEW]
│   ├── translate-utils.{c,h}     # Utility functions [NEW]
│   ├── translate-mail-ui.{c,h}   # Main mail view UI
│   ├── translate-browser-extension.{c,h}  # Browser window UI
│   ├── translate-dom.{c,h}       # DOM state management
│   ├── translate-content.{c,h}   # Email content extraction
│   ├── translate-preferences.{c,h}  # Settings dialog
│   ├── providers/
│   │   ├── translate-provider.{c,h}       # Provider interface
│   │   └── translate-provider-argos.{c,h}  # Argos implementation
│   └── m-utils.{c,h}             # Evolution helper utilities
├── tools/translate/              # Python translation backend
│   ├── translate_runner.py      # Main translation script
│   └── setup_models.py           # Model installation utility
├── data/                         # Configuration and resources
│   └── gschema/                  # GSettings schema
├── docs/                         # Additional documentation
├── USER_GUIDE.md                 # End-user documentation
├── DEVELOPER_GUIDE.md            # This file
└── CMakeLists.txt                # Build configuration
```

## Code Architecture Details

### 1. Provider Pattern

The extension uses the GObject interface pattern to support multiple translation backends.

**Interface Definition** (`translate-provider.h`):
```c
G_DECLARE_INTERFACE (TranslateProvider, translate_provider, TRANSLATE, PROVIDER, GObject)

struct _TranslateProviderInterface {
    GTypeInterface parent_iface;

    void (*translate_async) (TranslateProvider   *self,
                            const gchar         *text,
                            gboolean             is_html,
                            const gchar         *source_lang,
                            const gchar         *target_lang,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data);

    gboolean (*translate_finish) (TranslateProvider  *self,
                                 GAsyncResult       *result,
                                 gchar             **translated,
                                 GError            **error);
};
```

**Benefits**:
- Easy to add new translation providers (Google, DeepL, etc.)
- Clean separation between UI and translation logic
- Testable in isolation

### 2. DRY Refactoring (Recent Improvement)

Previously, translation logic was duplicated in `translate-mail-ui.c` and `translate-browser-extension.c`. This has been refactored:

**Before** (~40 lines duplicated):
```c
// In both files:
- Get settings
- Extract target language
- Create provider
- Call translate_async
- Memory leak with target_lang_copy
```

**After** (centralized in `translate-common.c`):
```c
void translate_common_translate_async (const gchar *body_html,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);
```

**Impact**:
- Eliminated ~80 lines of duplicate code
- Fixed memory leak
- Single source of truth for translation logic
- Easier maintenance

### 3. DOM State Management

The `translate-dom.c` module manages translation state per email display:

```c
typedef struct {
    gchar *original_html;           // Original email content
    gchar *original_message_uid;    // Message identifier
    gboolean is_translated;         // Current state
} DomState;
```

**Key Functions**:
- `translate_dom_apply_to_shell_view()` - Apply translation to main view
- `translate_dom_apply_to_reader()` - Apply translation to browser window
- `translate_dom_restore_original()` - Revert to original content
- `translate_dom_is_translated()` - Check current state
- `translate_dom_clear_if_message_changed()` - Auto-cleanup on navigation

### 4. Content Extraction

The `translate-content.c` module handles MIME email parsing:

```c
gchar *translate_get_selected_message_body_html_from_shell_view (EShellView *shell_view);
gchar *translate_get_selected_message_body_html_from_reader (EMailReader *reader);
```

**Process**:
1. Extract `CamelMimeMessage` from Evolution
2. Find HTML or plain text body parts
3. Decode content (base64, quoted-printable, etc.)
4. Convert to UTF-8
5. Return as string for translation

---

## Design Patterns

### 1. Provider Pattern
**Purpose**: Allow multiple translation backends

**Implementation**:
```c
// Provider interface (translate-provider.h)
struct _TranslateProviderInterface {
    GTypeInterface parent_iface;

    void (*translate_async) (TranslateProvider *self,
                             const gchar *text,
                             const gchar *target_language,
                             GAsyncReadyCallback callback);

    gchar* (*translate_finish) (TranslateProvider *self,
                                GAsyncResult *result,
                                GError **error);
};

// Concrete implementation (translate-provider-argos.c)
static void translate_provider_iface_init (TranslateProviderInterface *iface) {
    iface->translate_async = tp_argos_translate_async;
    iface->translate_finish = tp_argos_translate_finish;
}
```

**Benefits**:
- Easy to add new providers (Google, DeepL, LibreTranslate)
- Swap providers at runtime
- Test with mock providers

### 2. Async/Callback Pattern
**Purpose**: Keep UI responsive during translation

**Implementation**:
```c
// Initiate async operation
translate_common_translate_async (
    context,
    is_shell_view,
    NULL,  // cancellable
    on_translate_finished,  // callback
    user_data
);

// Callback when complete
static void on_translate_finished (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data) {
    // Handle result
}
```

### 3. Factory/Registry Pattern
**Purpose**: Central provider management

**Implementation**:
```c
// Register provider
translate_provider_register (TRANSLATE_TYPE_PROVIDER_ARGOS);

// Get provider by ID
TranslateProvider *provider = translate_provider_new_by_id ("argos");
```

### 4. DRY (Don't Repeat Yourself)
**Implemented**:
- `translate-common.c`: Single translation logic
- `gpu_utils.py`: Shared GPU setup
- Provider interface: Reusable async pattern

---

## Data Flow

### Translation Request Flow

```
User clicks "Translate Message"
          │
          ▼
┌─────────────────────┐
│  UI Action Handler  │
│  (mail-ui.c or      │
│   browser-ext.c)    │
└─────────┬───────────┘
          │
          ▼
┌─────────────────────┐
│  translate_common_  │  <-- Centralized (DRY)
│  translate_async()  │
└─────────┬───────────┘
          │
          ├──> 1. Extract content (translate-content.c)
          ├──> 2. Get target language (translate-utils.c)
          ├──> 3. Call provider (translate-provider-argos.c)
          │           │
          │           ▼
          │    ┌──────────────────┐
          │    │ translate_runner │
          │    │ (Python)         │
          │    │  - GPU setup     │
          │    │  - HTML parsing  │
          │    │  - Translation   │
          │    └────────┬─────────┘
          │             │
          │<────────────┘
          │
          └──> 4. Apply to DOM (translate-dom.c)
                       │
                       ▼
              ┌──────────────────┐
              │ User sees        │
              │ translated email │
              └──────────────────┘
```

### HTML-Aware Translation Process

```
Input: "<p>Hello <b>world</b>!</p>"
   │
   ▼
┌────────────────────────────────────┐
│  BeautifulSoup HTML Parser         │
│  - Parse HTML into DOM tree        │
│  - Identify text nodes vs tags     │
└──────────────┬─────────────────────┘
               │
               ▼
     ┌─────────────────────┐
     │  Text Node: "Hello " │
     │  Tag: <b>            │
     │  Text Node: "world"  │
     │  Tag: </b>           │
     │  Text Node: "!"      │
     └─────────┬────────────┘
               │
               ▼
┌────────────────────────────────────┐
│  Translate Text Nodes Only         │
│  - "Hello " → "Hola "              │
│  - "world" → "mundo"               │
│  - "!" → "!"                       │
└──────────────┬─────────────────────┘
               │
               ▼
┌────────────────────────────────────┐
│  Reconstruct HTML                  │
│  - Preserve all tags/attributes    │
│  - Replace text nodes              │
└──────────────┬─────────────────────┘
               │
               ▼
Output: "<p>Hola <b>mundo</b>!</p>"
```

---

## Adding New Features

### Adding a New Translation Provider

### Step 1: Create Provider Files

Create `providers/translate-provider-<name>.{c,h}`:

```c
// translate-provider-myservice.h
#define TRANSLATE_TYPE_PROVIDER_MYSERVICE (translate_provider_myservice_get_type())
G_DECLARE_FINAL_TYPE (TranslateProviderMyService, translate_provider_myservice,
                      TRANSLATE, PROVIDER_MYSERVICE, GObject)

TranslateProviderMyService *translate_provider_myservice_new (void);
```

### Step 2: Implement the Interface

```c
// translate-provider-myservice.c
struct _TranslateProviderMyService {
    GObject parent_instance;
    // Your provider-specific data
};

static void translate_provider_interface_init (TranslateProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TranslateProviderMyService, translate_provider_myservice, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TRANSLATE_TYPE_PROVIDER, translate_provider_interface_init))

static void
translate_async_impl (TranslateProvider   *self,
                     const gchar         *text,
                     gboolean             is_html,
                     const gchar         *source_lang,
                     const gchar         *target_lang,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    // Your implementation here
    // Use GTask for async operations
}

static gboolean
translate_finish_impl (TranslateProvider  *self,
                      GAsyncResult       *result,
                      gchar             **translated,
                      GError            **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
translate_provider_interface_init (TranslateProviderInterface *iface)
{
    iface->translate_async = translate_async_impl;
    iface->translate_finish = translate_finish_impl;
}
```

### Step 3: Register Provider

In `providers/translate-provider.c`:

```c
void translate_provider_register_builtin_providers (void)
{
    translate_provider_register ("argos", TRANSLATE_TYPE_PROVIDER_ARGOS);
    translate_provider_register ("myservice", TRANSLATE_TYPE_PROVIDER_MYSERVICE); // Add this
}
```

### Step 4: Update CMakeLists.txt

```cmake
list(APPEND SOURCES
    providers/translate-provider-myservice.h
    providers/translate-provider-myservice.c
)
```

## Key APIs

### Translation Functions

```c
// Initiate translation (centralized)
void translate_common_translate_async (const gchar *body_html,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);

// Get target language from settings
gchar *translate_utils_get_target_language (void);

// Apply translation to display
void translate_dom_apply_to_shell_view (EShellView *shell_view,
                                        const gchar *translated_html);

// Restore original content
void translate_dom_restore_original (EShellView *shell_view);
```

### Provider Interface

```c
// Create provider by ID
GObject *translate_provider_new_by_id (const gchar *provider_id);

// Async translation
void translate_provider_translate_async (TranslateProvider *self,
                                        const gchar *text,
                                        gboolean is_html,
                                        const gchar *source_lang,
                                        const gchar *target_lang,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);

// Get results
gboolean translate_provider_translate_finish (TranslateProvider *self,
                                              GAsyncResult *result,
                                              gchar **translated,
                                              GError **error);
```

## Python Backend

### translate_runner.py

The Python backend handles the actual translation:

**Key Functions**:

```python
def translate_offline(text: str, target: str, is_html: bool) -> str:
    """
    Main translation function.
    - Detects source language
    - Loads appropriate model
    - Translates text or HTML
    - Returns translated content
    """

def translate_html_carefully(translator, html_content: str) -> str:
    """
    HTML-aware translation that preserves structure.
    - Parses HTML with BeautifulSoup
    - Only translates text nodes
    - Keeps all tags, attributes, styles intact
    """
```

**Protocol**:
- C code spawns Python process with stdin/stdout pipes
- Sends HTML content via stdin
- Python translates and writes to stdout
- C code reads result

## Build System

### CMake Configuration

```cmake
# Find Evolution libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(EVOLUTION_SHELL REQUIRED evolution-shell-3.0)
pkg_check_modules(EVOLUTION_MAIL REQUIRED evolution-mail-3.0)

# Build module
add_library(example-module MODULE ${SOURCES})
target_link_libraries(example-module ${EVOLUTION_SHELL_LDFLAGS} ...)

# Install to Evolution module directory
install(TARGETS example-module DESTINATION ${EVOLUTION_MODULE_DIR})
```

## Testing

### Manual Testing

1. **Build and install**:
   ```bash
   mkdir -p build && cd build
   cmake .. && make && sudo make install
   ```

2. **Run Evolution with debugging**:
   ```bash
   killall evolution
   TRANSLATE_HELPER_PATH="/usr/share/evolution-translate/translate/translate_runner.py" \
   TRANSLATE_PYTHON_BIN="$HOME/.local/lib/evolution-translate/venv/bin/python" \
   evolution 2>&1 | grep translate
   ```

3. **Test translation**:
   - Open an email in a foreign language
   - Click "Translate Message"
   - Verify translation appears
   - Click "Show Original"
   - Verify original content restored

### Common Issues

**Module Not Loading**:
```bash
# Check if module is installed
ls $(pkg-config --variable=moduledir libemail-engine)/*.so

# Check Evolution sees it
evolution --force-online 2>&1 | grep "Loading module"
```

**Python Script Issues**:
```bash
# Test Python script directly
echo "<html><body>Bonjour</body></html>" | \
    "$HOME/.local/lib/evolution-translate/venv/bin/python" \
    /usr/share/evolution-translate/translate/translate_runner.py --target en --html
```

---

## Testing

### Manual Testing

#### Quick Test
```bash
# Kill Evolution
killall evolution

# Start Evolution with debug output
evolution 2>&1 | grep -i translate

# Open an email in a foreign language
# Press Ctrl+Shift+T
# Verify translation appears
```

### Testing Checklist

#### Basic Functionality
- [ ] Translation in Mail View (Ctrl+Shift+T)
- [ ] Translation in Browser Window
- [ ] Toggle back to original
- [ ] Plain text email translation
- [ ] HTML email translation with formatting preserved

#### Configuration
- [ ] Change target language in preferences
- [ ] Enable/disable install-on-demand
- [ ] Settings persist after restart

#### Install-on-Demand
- [ ] Auto-download missing model when needed
- [ ] Error message when disabled and model missing
- [ ] Model successfully installed and works

#### HTML Preservation
- [ ] Links work and URLs unchanged
- [ ] Images display correctly
- [ ] Complex formatting (tables, bold, italic, colors) preserved

#### Edge Cases
- [ ] Same language (no translation occurs)
- [ ] Empty email (no error)
- [ ] Very long email (>10KB)
- [ ] Special characters and emojis preserved

### Debugging

#### Enable Debug Mode

```bash
# Method 1: Environment variable
TRANSLATE_DEBUG=1 evolution

# Method 2: Python script directly
echo "Bonjour le monde" | python3 /usr/share/evolution-translate/translate/translate_runner.py \
    --target en --text --debug

# Method 3: Check debug log
tail -f /tmp/translate_debug.log
```

#### Debug Commands

```bash
# Check Evolution logs
journalctl --user -f | grep -i evolution

# List installed models
python3 <<EOF
import argostranslate.translate as trans
for lang in trans.get_installed_languages():
    print(f"Language: {lang.code} ({lang.name})")
    for translation in lang.translations_from:
        print(f"  -> {translation.to_lang.code}")
EOF

# Test translation directly
echo "Test email" | "$HOME/.local/lib/evolution-translate/venv/bin/python" \
    /usr/share/evolution-translate/translate/translate_runner.py --target en --text
```

### Common Issues

#### Module Not Loading
```bash
# Check if module exists in system location
ls -la /usr/lib/evolution/modules/libtranslate-module.so
# Or for multiarch systems:
ls -la /usr/lib/x86_64-linux-gnu/evolution/modules/libtranslate-module.so

# Check Evolution module search path (should be /usr/lib*/evolution/modules)
pkg-config --variable=moduledir evolution-shell-3.0

# Verify GSettings schemas compiled and readable
ls -l /usr/share/glib-2.0/schemas/gschemas.compiled
# Should be: -rw-r--r-- (world-readable, NOT -rw-r-----)

# Check if all Evolution schemas are accessible
gsettings list-schemas | grep evolution | wc -l
# Should show 40+ schemas, not just 2

# If schemas are not readable (restrictive umask issue):
sudo sh -c 'umask 0022 && glib-compile-schemas /usr/share/glib-2.0/schemas'

# Check if Evolution actually loads the module
G_MESSAGES_DEBUG=all evolution 2>&1 | grep -i "translate\|module"
```

**Important:** When compiling GSettings schemas with sudo, always use `umask 0022` to ensure the compiled file is world-readable. Restrictive root umask (like 0027) will create files with 640 permissions that normal users cannot read, causing Evolution to crash with "Settings schema not installed" errors.

#### Translation Not Working
```bash
# Verify Python environment
~/.local/lib/evolution-translate/venv/bin/python -c "import argostranslate"

# Check installed models
~/.local/lib/evolution-translate/venv/bin/python -c "
import argostranslate.package
print(argostranslate.package.get_installed_packages())
"
```

---

## Packaging and Releases

### Building Debian Package

#### Automated Build
```bash
cd evolution-mail-translate
./scripts/build-deb.sh
```

The script will:
1. Check for all required build dependencies
2. Clean previous build artifacts
3. Build the Debian package using `dpkg-buildpackage`
4. Run quality checks with `lintian`
5. Display the package location

#### Manual Build
```bash
# Clean previous builds
rm -rf build _build
rm -f ../*.deb ../*.changes ../*.buildinfo

# Build the package
dpkg-buildpackage -us -uc -b

# The .deb file will be in the parent directory
ls -l ../*.deb
```

**Build flags:**
- `-us` - Do not sign the source package
- `-uc` - Do not sign the changes file
- `-b` - Build binary package only (no source)

### Package Contents

The `.deb` package installs:

```
/usr/lib/x86_64-linux-gnu/evolution/modules/libtranslate-module.so
/usr/share/evolution-translate/translate/
  ├── translate_runner.py
  ├── install_default_models.py
  └── setup_models.py
/usr/share/glib-2.0/schemas/org.gnome.evolution.translate.gschema.xml
```

### Creating Releases

#### 1. Prepare the Release

```bash
# Update version in CMakeLists.txt
sed -i 's/VERSION "1.0.0"/VERSION "1.1.0"/' CMakeLists.txt

# Update debian/changelog
dch -v 1.1.0-1 "New release with feature X"

# Commit changes
git add CMakeLists.txt debian/changelog
git commit -m "Bump version to 1.1.0"
git push
```

#### 2. Build and Test Package

```bash
# Build the .deb package
./scripts/build-deb.sh

# Verify the package
lintian ../evolution-translate-extension_*.deb

# Test installation
sudo dpkg -i ../evolution-translate-extension_*.deb
```

#### 3. Create GitHub Release

```bash
# Tag the release
git tag -a v1.1.0 -m "Release version 1.1.0"
git push origin v1.1.0

# Create release on GitHub
gh release create v1.1.0 \
    --title "Evolution Translation Extension v1.1.0" \
    --notes "Release notes here..." \
    ../evolution-translate-extension_1.1.0-1_amd64.deb
```

Or use the GitHub web interface:
1. Go to repository releases page
2. Click "Draft a new release"
3. Create tag: `v1.1.0`
4. Upload the `.deb` file
5. Write release notes
6. Publish

#### Release Checklist

- [ ] Version bumped in `CMakeLists.txt`
- [ ] `debian/changelog` updated
- [ ] All tests passing
- [ ] Package builds without errors
- [ ] Package installs cleanly on fresh system
- [ ] Extension loads in Evolution
- [ ] Translation functionality works
- [ ] Git tag created and pushed
- [ ] GitHub release created with `.deb` attached
- [ ] README.md updated with new version

### Testing Package

```bash
# Test on clean Ubuntu VM
lxc launch ubuntu:24.04 test-pkg
lxc exec test-pkg -- bash

# Install Evolution
apt update && apt install evolution

# Test package installation
dpkg -i evolution-translate-extension_*.deb
apt-get install -f

# Verify files installed
dpkg -L evolution-translate-extension

# Test in Evolution
evolution &
```

---

## Best Practices

### Memory Management

1. **Use GLib autoptr macros**:
   ```c
   g_autofree gchar *text = g_strdup ("hello");
   g_autoptr(GObject) obj = g_object_new (...);
   // Auto-freed when out of scope
   ```

2. **Avoid leaks in async code**:
   ```c
   // BAD:
   gchar *lang = g_strdup ("en");
   async_function (..., lang);  // leak if async holds reference

   // GOOD (now in translate-common.c):
   g_autofree gchar *lang = g_strdup ("en");
   async_function (..., lang);  // cleaned up properly
   ```

### Error Handling

```c
g_autoptr(GError) error = NULL;
if (!translate_provider_translate_finish (provider, result, &translated, &error)) {
    g_warning ("Translation failed: %s", error->message);
    return;
}
```

### Async Patterns

```c
// Create GTask for async operation
GTask *task = g_task_new (self, cancellable, callback, user_data);
g_task_set_task_data (task, my_data, g_free);

// Run in thread
g_task_run_in_thread (task, my_async_worker);

// In worker:
static void my_async_worker (GTask *task, ...) {
    gchar *result = do_work ();
    g_task_return_pointer (task, result, g_free);
}

// In finish:
return g_task_propagate_pointer (G_TASK (result), error);
```

## Contributing

### Code Style

- Follow GNOME coding style
- Use tabs for indentation (width: 8)
- Max line length: 120 characters
- Document all public functions with gtk-doc style comments

### Commit Messages

```
Short summary (50 chars or less)

Detailed explanation of what changed and why.
Reference any related issues.

Fixes #123
```

### Pull Request Process

1. Fork the repository
2. Create a feature branch
3. Make your changes with clear commits
4. Add/update documentation
5. Test thoroughly
6. Submit PR with description

---

## Troubleshooting Development

### Compilation Errors

**Missing headers**:
```bash
# Install Evolution development packages
sudo apt install evolution-dev evolution-data-server-dev
```

**CMake can't find Evolution**:
```bash
# Check pkg-config
pkg-config --modversion evolution-shell-3.0
pkg-config --variable=moduledir libemail-engine
```

### Runtime Issues

**Segmentation faults**:
- Enable core dumps: `ulimit -c unlimited`
- Run with gdb: `gdb evolution`
- Check for NULL pointer dereferences

**GObject warnings**:
- Check type registration
- Verify interface implementation
- Use `G_DEFINE_TYPE` macros correctly

---

## Future Enhancements

### Planned Features

1. **Online Translation Providers**: Google Translate, DeepL API support
2. **Caching**: Cache translations to avoid re-translating same content
3. **Preferences UI**: Provider selection, quality settings
4. **Batch Translation**: Translate multiple emails at once
5. **Dictionary Mode**: Quick word/phrase translation

### Architecture Improvements

1. **Provider Discovery**: Plugin system for dynamic provider loading
2. **Test Suite**: Unit and integration tests
3. **Performance**: Long-running Python daemon with IPC
4. **Memory Optimization**: Translation caching

---

## Resources

- **[GNOME Evolution Wiki](https://wiki.gnome.org/Apps/Evolution)** - Official Evolution documentation
- **[Evolution Plugin Development](https://wiki.gnome.org/Apps/Evolution/PluginHowTo)** - Plugin development guide
- **[GObject Tutorial](https://docs.gtk.org/gobject/)** - GObject type system
- **[ArgosTranslate Docs](https://github.com/argosopentech/argos-translate)** - Translation backend
- **[Debian Packaging Guide](https://www.debian.org/doc/manuals/maint-guide/)** - Package creation

---

## Contributing

See the project structure and best practices sections above for development guidelines.

For end-user documentation, see [USER_GUIDE.md](USER_GUIDE.md).
