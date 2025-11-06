# Quick Reference - Translation Implementation Locations

## IMMEDIATE ACCESS: KEY CODE SECTIONS

### 1. TRANSLATION ENTRY POINT (User clicks "Translate")
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-mail-ui.c`
**Lines**: 67-91 (action_translate_message_cb)
```c
static void
action_translate_message_cb (GtkAction *action,
                             gpointer   user_data)
{
    EShellView *shell_view = user_data;
    g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

    // Check if already translated
    if (translate_dom_is_translated (shell_view)) {
        translate_dom_restore_original (shell_view);
        return;
    }

    // Extract message body HTML
    g_autofree gchar *body_html = get_selected_message_body_html (shell_view);
    
    // Initiate translation
    translate_common_translate_async (body_html,
                                      on_translate_finished,
                                      shell_view);
}
```

### 2. TRANSLATION REQUEST HANDLER (Centralized Logic)
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-common.c`
**Lines**: 40-77 (translate_common_translate_async)
```c
void
translate_common_translate_async (const gchar         *body_html,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    // Get target language from GSettings
    g_autofree gchar *target_lang = translate_utils_get_target_language ();

    // CREATE PROVIDER (HARDCODED "argos")
    g_autoptr(GObject) provider_obj = translate_provider_new_by_id ("argos");
    
    // Initiate async translation
    translate_provider_translate_async ((TranslateProvider*)provider_obj,
                                        body_html,
                                        TRUE,  // is_html
                                        NULL,  // auto-detect source
                                        target_lang,
                                        NULL,  // cancellable
                                        callback,
                                        user_data);
}
```

### 3. PROVIDER INTERFACE (GInterface Definition)
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/providers/translate-provider.h`
**Lines**: 15-35
```c
struct _TranslateProviderInterface {
    GTypeInterface parent_iface;

    /* Async translate. Implementations must be non-blocking. */
    void (*translate_async) (gpointer          self,
                             const gchar      *input,
                             gboolean          is_html,
                             const gchar      *source_lang_opt,
                             const gchar      *target_lang,
                             GCancellable     *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer          user_data);

    gboolean (*translate_finish) (gpointer      self,
                                  GAsyncResult *res,
                                  gchar       **out_translated_html,
                                  GError      **error);

    const gchar* (*get_id)   (gpointer self);
    const gchar* (*get_name) (gpointer self);
};
```

### 4. ARGOS PROVIDER IMPLEMENTATION (Subprocess Spawning)
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/providers/translate-provider-argos.c`
**Lines**: 134-247 (tp_argos_translate_async)

**Key sections:**
- Lines 163-192: Helper script location resolution
- Lines 194-212: Python binary location resolution
- Lines 220-226: Command building (python helper_script --target lang --html)
- Lines 228-234: Subprocess creation with stdin/stdout pipes
- Lines 242-246: Async communication

```c
static void
tp_argos_translate_async (gpointer              self,
                          const gchar          *input,
                          gboolean              is_html,
                          const gchar          *source_lang_opt,
                          const gchar          *target_lang,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
    // Build helper command
    const gchar *argvv[] = { python, helper_path, "--target", target_copy,
                              is_html ? "--html" : "--text", install_flag, NULL };
    
    // Spawn subprocess
    GSubprocess *proc = g_subprocess_newv (argvv, 
                G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE, 
                &error);
    
    // Send input via stdin, get response via stdout
    g_subprocess_communicate_utf8_async (proc,
                                         payload,
                                         cancellable,
                                         on_helper_done,
                                         task);
}
```

### 5. JSON RESPONSE PARSING
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/providers/translate-provider-argos.c`
**Lines**: 60-102 (extract_translated_field)
```c
static gchar *
extract_translated_field (const gchar *json)
{
    // Parse JSON: {"translated": "translated text here"}
    JsonParser *parser = json_parser_new ();
    json_parser_load_from_data (parser, json, -1, &error);
    
    JsonNode *root = json_parser_get_root (parser);
    JsonObject *obj = json_node_get_object (root);
    
    // Extract "translated" field
    const gchar *translated = json_object_get_string_member (obj, "translated");
    
    return g_strdup (translated);
}
```

### 6. PYTHON HELPER ENTRY POINT
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/tools/translate/translate_runner.py`
**Lines**: 296-342 (main function)

**Invocation signature**:
```bash
python3 translate_runner.py --target <lang> [--html|--text] [--install-on-demand|--no-install-on-demand] [--debug]
```

**Input**: Plain text or HTML via stdin

**Output**: JSON response via stdout
```json
{"translated": "translated content"}
```

### 7. PYTHON TRANSLATION LOGIC
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/tools/translate/translate_runner.py`
**Lines**: 174-294 (translate_offline function)

**Key steps:**
1. Language detection (langdetect)
2. Check installed models
3. Auto-download if needed (if enabled)
4. Get translator from ArgosTranslate
5. HTML-aware translation (BeautifulSoup)
6. Return translated content

### 8. HTML-PRESERVING TRANSLATION
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/tools/translate/translate_runner.py`
**Lines**: 108-172 (translate_html_carefully)

Uses BeautifulSoup to:
- Parse HTML structure
- Identify text nodes only
- Skip tags, attributes, comments
- Translate only text content
- Reconstruct HTML with translations

### 9. GSettings SCHEMA
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/data/gschema/org.gnome.evolution.translate.gschema.xml`

**Available settings:**
```xml
<!-- Main schema: org.gnome.evolution.translate -->
target-language (string, default: 'en')     → User's target language
provider-id (string, default: 'argos')      → Active provider
preserve-format (boolean, default: true)    → HTML preservation flag

<!-- Provider schema: org.gnome.evolution.translate.provider -->
venv-path (string, default: '')             → Custom Python venv path
install-on-demand (boolean, default: true)  → Auto-download models
```

### 10. MESSAGE CONTENT EXTRACTION
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-content.c`
**Lines**: 100-153

**Process:**
1. Get selected message from folder
2. Find best MIME part (prefer HTML over plain text)
3. Decode to UTF-8
4. Convert plain text to HTML if necessary

### 11. MODULE INITIALIZATION (Plugin Loading)
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-module.c`
**Lines**: 35-47 (e_module_load)

```c
void
e_module_load (GTypeModule *type_module)
{
    // Register shell view extension (hooks into Mail view)
    translate_shell_view_extension_type_register (type_module);
    translate_browser_extension_type_register (type_module);

    // Register Argos provider
    translate_provider_argos_type_register (type_module);
    
    // Add Argos to provider registry
    translate_provider_register (TRANSLATE_TYPE_PROVIDER_ARGOS);
}
```

### 12. UI MENU/BUTTON REGISTRATION
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-mail-ui.c`
**Lines**: 185-240 (translate_mail_ui_init)

Defines:
- UI XML (menu structure)
- Action entries (with callbacks)
- Keyboard shortcuts
- Action groups
- UI manager integration

### 13. CONFIGURATION DIALOG
**File**: `/home/eik-tb/OneDrive_andreaivan.costantino@kuleuven.be/GitHub/evolution-mail-translate/src/translate-preferences.c`
**Lines**: 32-97 (translate_preferences_show)

Provides GUI for:
- Target language selection (27 languages)
- Provider display (read-only)
- Venv path entry
- Install-on-demand toggle

---

## FILE PATHS SUMMARY

### Critical C Files
```
/src/translate-module.c                    → Plugin entry point
/src/translate-common.c                    → Centralized translation logic
/src/translate-mail-ui.c                   → Menu/button integration
/src/providers/translate-provider.h/c      → Provider interface
/src/providers/translate-provider-argos.c  → Argos implementation (subprocess spawning)
```

### Critical Python Files
```
/tools/translate/translate_runner.py       → Translation orchestration
/tools/translate/gpu_utils.py              → GPU acceleration
```

### Configuration
```
/data/gschema/org.gnome.evolution.translate.gschema.xml  → Settings schema
```

---

## KEY CONCEPTS

### Provider Pattern
- Interface: `TranslateProvider` (GInterface)
- Registry: Dynamic provider registration
- Current: Only "argos" registered
- Extensible: New providers can be added

### Async Model
- All operations non-blocking
- Uses GTask for async results
- Callbacks: on_translate_finished
- Subprocess communication via stdin/stdout

### Configuration
- Primary: GSettings (`org.gnome.evolution.translate`)
- Secondary: Environment variables (development)
- UI: Preferences dialog

### Translation Flow
1. User triggers via menu/keyboard
2. UI extracts message HTML
3. Common layer gets target language
4. Provider spawns Python helper
5. Helper detects language, loads model, translates
6. JSON response parsed
7. DOM updated with translation

---

## IMPORTANT NOTES

1. **Hardcoded Provider**: Provider ID "argos" is hardcoded in `translate-common.c:53`
   - To support multiple providers, this would need to become configurable

2. **Provider Interface is Extensible**: The GInterface pattern supports adding new providers
   - Register new provider type in module initialization
   - Implement interface methods
   - Add to registry

3. **Python Integration**: Uses subprocess pipes (stdin/stdout)
   - Prevents blocking the main thread
   - Allows language detection, model management, GPU acceleration

4. **HTML Preservation**: BeautifulSoup parses HTML and translates only text nodes
   - Maintains all tags, attributes, comments
   - Best-effort structure preservation

5. **No External APIs**: All translation is offline
   - Models downloaded locally
   - GPU acceleration optional (CUDA)
   - Zero data transmission

