/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hashid_engine.h"
#include "hashid_external.h"
#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HID_VERSION "1.0.2"
#define HID_HISTORY APP_DATA_PATH("history.txt")
#define HID_REPORT APP_DATA_PATH("report.txt")
#define HID_REPORT_TMP APP_DATA_PATH("report.txt.partial")
#define HID_REPORT_BACKUP APP_DATA_PATH("report.txt.backup")
#define HID_HISTORY_MAX_BYTES 16384U

typedef enum { HidViewMenu, HidViewInput, HidViewSettings, HidViewText, HidViewExternal } HidViewId;
typedef enum { HidMenuManual, HidMenuFile, HidMenuExternal, HidMenuHistory, HidMenuSettings, HidMenuAbout } HidMenu;
typedef struct {
    Gui* gui; Storage* storage; DialogsApp* dialogs; ViewDispatcher* dispatcher;
    Submenu* menu; TextInput* input; VariableItemList* settings; Widget* widget; View* external_view;
    FuriString* text; FuriString* path; FuriThread* worker; FuriMutex* mutex;
    HashidExternal* external; uint32_t baud; uint8_t baud_index;
    char manual[257]; volatile bool cancel; bool extended, show_hashcat, show_john, save_history;
    uint32_t processed, identified, invalid, start_tick, end_tick; bool report_saved;
    HidViewId current, text_back; bool views_added;
} HidApp;

typedef struct { HidApp* app; uint32_t revision; } HidExternalModel;
static const uint32_t hid_bauds[] = {115200U, 230400U, 460800U};
static const char* const hid_baud_names[] = {"115200", "230400", "460800"};

static void hid_switch(HidApp* app, HidViewId view) {
    app->current = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void hid_show(HidApp* app, const char* title, const char* body, HidViewId back) {
    widget_reset(app->widget);
    furi_string_printf(app->text, "\e#%s\n%s", title, body);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->text));
    app->text_back = back;
    hid_switch(app, HidViewText);
}

static bool hid_mkdir(HidApp* app) {
    return storage_simply_mkdir(app->storage, APP_DATA_PATH(""));
}

static bool hid_bounded_length(const char* value, size_t capacity, size_t* length) {
    if(!value || !length || !capacity) return false;
    const char* terminator = memchr(value, '\0', capacity);
    if(!terminator) return false;
    *length = (size_t)(terminator - value);
    return true;
}

static bool hid_history_add(HidApp* app, const char* value) {
    if(!app->save_history) return true;
    if(!hid_mkdir(app)) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    size_t length;
    if(!hid_bounded_length(value, 257U, &length)) {
        storage_file_free(file);
        return false;
    }
    bool opened = storage_file_open(file, HID_HISTORY, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(opened && storage_file_size(file) + length + 1U > HID_HISTORY_MAX_BYTES) {
        storage_file_close(file);
        opened = storage_file_open(file, HID_HISTORY, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    }
    bool saved = opened && storage_file_write(file, value, length) == length &&
                 storage_file_write(file, "\n", 1U) == 1U && storage_file_sync(file);
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return saved;
}

static void hid_append_results(HidApp* app, FuriString* output, const char* value, const HidResults* results) {
    size_t checked_length;
    if(!hid_bounded_length(value, HID_MAX_INPUT + 1U, &checked_length)) return;
    (void)checked_length;
    HidCharacteristics info;
    hid_characteristics(value, checked_length, &info);
    furi_string_cat_printf(
        output, "Input length: %u\nForm: %s%s%s\nCandidates: %u%s\n\n",
        (unsigned)info.length, info.hexadecimal ? "hex " : "",
        info.base64ish ? "encoded " : "", info.has_separator ? "structured" : "plain",
        results->count, results->truncated ? " (bounded)" : "");
    if(!results->count) {
        furi_string_cat_str(output, "Unknown hash\n");
        return;
    }
    for(uint16_t i = 0; i < results->count; i++) {
        const HidCandidate* candidate = &hid_candidates[results->candidate_indices[i]];
        furi_string_cat_printf(output, "%u. %s", (unsigned)(i + 1U), candidate->name);
        if(app->show_hashcat && candidate->hashcat >= 0)
            furi_string_cat_printf(output, "\n   Hashcat: %ld", (long)candidate->hashcat);
        if(app->show_john && candidate->john)
            furi_string_cat_printf(output, "\n   John: %s", candidate->john);
        if(candidate->extended) furi_string_cat_str(output, "\n   Extended candidate");
        furi_string_cat_str(output, "\n");
    }
    furi_string_cat_str(output, "\nCandidates are possibilities, not certainty.\n");
}

static char* hid_trim(char* value, size_t capacity) {
    size_t original_length;
    if(!hid_bounded_length(value, capacity, &original_length)) return NULL;
    char* start = value;
    while(*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
    size_t length = original_length - (size_t)(value - start);
    while(length && (value[length - 1U] == ' ' || value[length - 1U] == '\t' ||
                     value[length - 1U] == '\r' || value[length - 1U] == '\n'))
        value[--length] = '\0';
    return value;
}

static void hid_manual_done(void* context) {
    HidApp* app = context;
    app->manual[sizeof(app->manual) - 1U] = '\0';
    char* value = hid_trim(app->manual, sizeof(app->manual));
    if(!value) { hid_show(app, "Invalid input", "Input is not terminated safely.", HidViewMenu); return; }
    size_t value_length;
    if(!hid_bounded_length(value, sizeof(app->manual), &value_length)) {
        hid_show(app, "Invalid input", "Input exceeds the manual-input bound.", HidViewMenu); return;
    }
    if(value != app->manual) memmove(app->manual, value, value_length + 1U);
    if(!app->manual[0]) { hid_show(app, "Invalid input", "Enter a non-empty encoded value.", HidViewMenu); return; }
    HidResults results;
    if(!hid_identify(app->manual, value_length, app->extended, &results)) {
        hid_show(app, "Invalid input", "Input exceeds the supported bound or is empty.", HidViewMenu);
        return;
    }
    FuriString* result = furi_string_alloc();
    if(!result) { hid_show(app, "Allocation failed", "Could not allocate the result view.", HidViewMenu); return; }
    hid_append_results(app, result, app->manual, &results);
    bool history_saved = hid_history_add(app, app->manual);
    if(!history_saved)
        furi_string_cat_str(result, "\nHISTORY NOT SAVED: check SD card and free space.\n");
    hid_show(app, "Identification", furi_string_get_cstr(result), HidViewMenu);
    furi_string_free(result);
}

static bool hid_flipper_data_path(const char* path) {
    const char* extension = strrchr(path, '.');
    if(!extension) return false;
    return !strcmp(extension, ".nfc") || !strcmp(extension, ".rfid") ||
           !strcmp(extension, ".sub") || !strcmp(extension, ".ibtn");
}

static bool hid_digest_like_saved_value(const char* value, size_t length) {
    if(length >= 32U) return true;
    return length >= 13U && (value[0] == '$' || value[0] == '{' || value[0] == '*');
}

static bool hid_write_all(File* file, const char* text) {
    size_t length = strlen(text);
    return storage_file_write(file, text, length) == length;
}

static void hid_increment(HidApp* app, uint32_t* counter) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    (*counter)++;
    furi_mutex_release(app->mutex);
}

static int32_t hid_file_worker(void* context) {
    HidApp* app = context;
    app->start_tick = furi_get_tick(); app->processed = app->identified = app->invalid = 0U; app->report_saved = false;
    bool failed = !hid_mkdir(app);
    if(!failed && storage_file_exists(app->storage, HID_REPORT_BACKUP)) {
        if(!storage_file_exists(app->storage, HID_REPORT))
            failed = storage_common_rename(app->storage, HID_REPORT_BACKUP, HID_REPORT) != FSE_OK;
        else failed = storage_common_remove(app->storage, HID_REPORT_BACKUP) != FSE_OK;
    }
    if(!failed && storage_file_exists(app->storage, HID_REPORT_TMP))
        failed = storage_common_remove(app->storage, HID_REPORT_TMP) != FSE_OK;
    File* input = storage_file_alloc(app->storage); File* output = storage_file_alloc(app->storage);
    failed = failed || !input || !output;
    bool opened = !failed && storage_file_open(input, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPEN_EXISTING) &&
                  storage_file_open(output, HID_REPORT_TMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    char* line = malloc(HID_MAX_INPUT + 1U); failed = failed || !opened || !line; size_t used = 0U; bool overflow = false;
    if(opened) hid_write_all(output, "HashID FZ v" HID_VERSION " measured report\n\n");
    while(!failed && !app->cancel) {
        uint8_t byte; size_t got = storage_file_read(input, &byte, 1U);
        if(!got && storage_file_get_error(input) != FSE_OK) { failed = true; break; }
        bool finish = !got || byte == '\n';
        if(!finish) {
            if(byte != '\r') {
                if(used < HID_MAX_INPUT) line[used++] = (char)byte;
                else overflow = true;
            }
            continue;
        }
        line[used] = '\0';
        if(overflow) { hid_increment(app, &app->invalid); }
        else if(used) {
            char* value = line;
            char* equals = strrchr(line, '=');
            if(hid_flipper_data_path(furi_string_get_cstr(app->path)) && equals) value = equals + 1U;
            value = hid_trim(value, HID_MAX_INPUT + 1U);
            if(!value) { hid_increment(app, &app->invalid); used = 0U; overflow = false; if(!got) break; else continue; }
            size_t value_length;
            if(!hid_bounded_length(value, HID_MAX_INPUT + 1U, &value_length)) {
                hid_increment(app, &app->invalid); used = 0U; overflow = false; if(!got) break; else continue;
            }
            if(!hid_flipper_data_path(furi_string_get_cstr(app->path)) || hid_digest_like_saved_value(value, value_length)) {
                HidResults results;
                hid_increment(app, &app->processed);
                if(hid_identify(value, value_length, app->extended, &results)) {
                    if(results.count) hid_increment(app, &app->identified);
                    FuriString* row = furi_string_alloc_printf("Input: %s\n", value);
                    hid_append_results(app, row, value, &results);
                    if(!hid_write_all(output, furi_string_get_cstr(row))) failed = true;
                    furi_string_free(row);
                } else hid_increment(app, &app->invalid);
            }
        }
        used = 0U; overflow = false;
        if(!got) break;
    }
    if(line) free(line);
    if(opened && !storage_file_sync(output)) failed = true;
    if(input) { storage_file_close(input); storage_file_free(input); }
    if(output) { storage_file_close(output); storage_file_free(output); }
    if(!failed && !app->cancel) {
        bool had_report = storage_file_exists(app->storage, HID_REPORT);
        if(storage_file_exists(app->storage, HID_REPORT_BACKUP))
            storage_common_remove(app->storage, HID_REPORT_BACKUP);
        if(had_report && storage_common_rename(app->storage, HID_REPORT, HID_REPORT_BACKUP) != FSE_OK)
            failed = true;
        if(!failed && storage_common_rename(app->storage, HID_REPORT_TMP, HID_REPORT) == FSE_OK) {
            app->report_saved = true;
            if(had_report) storage_common_remove(app->storage, HID_REPORT_BACKUP);
        } else if(had_report) {
            storage_common_rename(app->storage, HID_REPORT_BACKUP, HID_REPORT);
        }
    }
    if(!app->report_saved) storage_common_remove(app->storage, HID_REPORT_TMP);
    app->end_tick = furi_get_tick();
    view_dispatcher_send_custom_event(app->dispatcher, 1U);
    return 0;
}

static void hid_join(HidApp* app) {
    if(app->worker) { furi_thread_join(app->worker); furi_thread_free(app->worker); app->worker = NULL; }
}

static void hid_start_file(HidApp* app) {
    DialogsFileBrowserOptions options; dialog_file_browser_set_basic_options(&options, "*", NULL);
    options.hide_ext = false; options.skip_assets = false;
    if(!dialog_file_browser_show(app->dialogs, app->path, app->path, &options)) return;
    app->cancel = false;
    app->worker = furi_thread_alloc_ex("HashIDFile", 4096U, hid_file_worker, app);
    if(!app->worker) {
        hid_show(app, "Allocation failed", "Could not allocate the file-analysis worker. No operation started.", HidViewMenu);
        return;
    }
    furi_thread_start(app->worker);
    hid_show(app, "Analyzing file", "Reading real text lines from microSD.\n\nBack requests cancellation.", HidViewMenu);
}

static void hid_show_history(HidApp* app) {
    File* file = storage_file_alloc(app->storage); FuriString* history = furi_string_alloc();
    if(!file || !history) {
        if(file) storage_file_free(file);
        if(history) furi_string_free(history);
        hid_show(app, "Allocation failed", "Could not allocate the history view.", HidViewMenu);
        return;
    }
    bool read_failed = false;
    if(storage_file_open(file, HID_HISTORY, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[257]; size_t got;
        while((got = storage_file_read(file, buffer, sizeof(buffer) - 1U)) > 0U) {
            buffer[got] = '\0'; furi_string_cat_str(history, buffer);
            if(furi_string_size(history) > 4096U) break;
        }
        if(storage_file_get_error(file) != FSE_OK) read_failed = true;
        storage_file_close(file);
    }
    storage_file_free(file);
    if(read_failed) furi_string_cat_str(history, "\nHISTORY READ FAILED: check SD card.\n");
    hid_show(app, "History", furi_string_size(history) ? furi_string_get_cstr(history) : "No saved manual inputs.", HidViewMenu);
    furi_string_free(history);
}

static void hid_external_draw(Canvas* canvas, void* model_context) {
    HidExternalModel* model = model_context; HidExternalSnapshot state; char line[96];
    hid_external_snapshot(model->app->external, &state);
    canvas_set_font(canvas, FontPrimary); canvas_draw_str(canvas, 1, 9, "External HashID");
    canvas_set_font(canvas, FontKeyboard);
    snprintf(line, sizeof(line), "%s %s", state.version[0] ? state.version : "waiting", state.state); canvas_draw_str(canvas, 1, 20, line);
    snprintf(line, sizeof(line), "Processed %lu", (unsigned long)state.files); canvas_draw_str(canvas, 1, 30, line);
    snprintf(line, sizeof(line), "Known %lu Unknown %lu", (unsigned long)state.matched, (unsigned long)state.changed); canvas_draw_str(canvas, 1, 40, line);
    snprintf(line, sizeof(line), "Bytes %llu Exit %ld", (unsigned long long)state.bytes, (long)state.exit_code); canvas_draw_str(canvas, 1, 50, line);
    canvas_draw_str(canvas, 1, 60, state.error[0] ? state.error : state.target);
    canvas_draw_str(canvas, 86, 9, state.running ? "OK Stop" : (model->app->extended ? "OK Ext" : "OK Run"));
}

static bool hid_external_input(InputEvent* event, void* context) {
    HidApp* app = context;
    if(event->key != InputKeyOk || event->type != InputTypeShort) return false;
    HidExternalSnapshot state; hid_external_snapshot(app->external, &state);
    if(!state.connected) return false;
    if(state.running) return hid_external_cancel(app->external);
    return hid_external_run(app->external, app->extended ? "EXTENDED" : "IDENTIFY");
}

static void hid_selected(void* context, uint32_t index) {
    HidApp* app = context;
    if(index == HidMenuManual) {
        text_input_reset(app->input); text_input_set_header_text(app->input, "Encoded hash/value");
        text_input_set_minimum_length(app->input, 1U);
        text_input_set_result_callback(app->input, hid_manual_done, app, app->manual, sizeof(app->manual), true);
        hid_switch(app, HidViewInput);
    } else if(index == HidMenuFile) hid_start_file(app);
    else if(index == HidMenuExternal) {
        if(!hid_external_start(app->external, app->baud)) {
            hid_show(app, "UART unavailable", "Could not acquire USART. Close other UART apps and check Settings baud.", HidViewMenu);
            return;
        }
        hid_switch(app, HidViewExternal);
    }
    else if(index == HidMenuHistory) hid_show_history(app);
    else if(index == HidMenuSettings) hid_switch(app, HidViewSettings);
    else hid_show(app, "About HashID FZ",
        "Version " HID_VERSION "\n\nNative port of psypanda/hashID 3.2.0-dev. Includes all 145 ordered upstream signatures and 272 candidate records from commit 7e8473a.\n\nOptional external mode runs genuine HashID on a Raspberry Pi, Linux laptop, desktop, or VM; Flipper controls it through 3.3 V UART.\n\nResults are format candidates, never proof. NFC/RFID UIDs are not treated as password hashes.\n\nGPL-3.0-or-later. No warranty.", HidViewMenu);
}

static void hid_baud_changed(VariableItem* item) {
    HidApp* app = variable_item_get_context(item);
    app->baud_index = variable_item_get_current_value_index(item);
    app->baud = hid_bauds[app->baud_index];
    variable_item_set_current_value_text(item, hid_baud_names[app->baud_index]);
}

static void hid_bool_changed(VariableItem* item) {
    HidApp* app = variable_item_get_context(item); uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "On" : "Off");
    uint32_t row = variable_item_list_get_selected_item_index(app->settings);
    if(row == 0U) app->extended = index; else if(row == 1U) app->show_hashcat = index;
    else if(row == 2U) app->show_john = index; else if(row == 3U) app->save_history = index;
}

static bool hid_custom(void* context, uint32_t event) {
    HidApp* app = context; if(event != 1U) return false; hid_join(app);
    uint32_t frequency = furi_kernel_get_tick_frequency();
    uint32_t ms = frequency ? (uint32_t)(((uint64_t)(app->end_tick - app->start_tick) * 1000U) / frequency) : 0U;
    char summary[384];
    snprintf(summary, sizeof(summary),
        "Status: %s\nProcessed: %lu\nIdentified: %lu\nInvalid/overlong: %lu\nElapsed: %lu.%03lu s\n\n%s",
        app->cancel ? "Cancelled" : app->report_saved ? "Completed" : "Read/write failure",
        (unsigned long)app->processed, (unsigned long)app->identified, (unsigned long)app->invalid,
        (unsigned long)(ms / 1000U), (unsigned long)(ms % 1000U),
        app->report_saved ? "Report saved in app data." : "No completed report was promoted.");
    hid_show(app, "File result", summary, HidViewMenu); return true;
}

static bool hid_back(void* context) {
    HidApp* app = context;
    if(app->worker) { app->cancel = true; return true; }
    if(app->current == HidViewMenu) view_dispatcher_stop(app->dispatcher);
    else if(app->current == HidViewExternal) { hid_external_stop(app->external); hid_switch(app, HidViewMenu); }
    else if(app->current == HidViewText) hid_switch(app, app->text_back);
    else hid_switch(app, HidViewMenu);
    return true;
}

static void hid_tick(void* context) {
    HidApp* app = context;
    if(app->current == HidViewExternal) {
        HidExternalModel* model = view_get_model(app->external_view); model->revision++;
        view_commit_model(app->external_view, true); return;
    }
    if(!app->worker) return;
    uint32_t processed, identified, invalid;
    if(furi_mutex_acquire(app->mutex, 0U) != FuriStatusOk) return;
    processed = app->processed; identified = app->identified; invalid = app->invalid;
    furi_mutex_release(app->mutex);
    furi_string_printf(app->text, "\e#Analyzing\nProcessed: %lu\nIdentified: %lu\nInvalid: %lu\n\nBack cancels safely.",
        (unsigned long)processed, (unsigned long)identified, (unsigned long)invalid);
    widget_reset(app->widget); widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->text));
}

static HidApp* hid_alloc(void) {
    HidApp* app = calloc(1U, sizeof(*app)); if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI); app->storage = furi_record_open(RECORD_STORAGE); app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->dispatcher = view_dispatcher_alloc(); app->menu = submenu_alloc(); app->input = text_input_alloc();
    app->settings = variable_item_list_alloc(); app->widget = widget_alloc(); app->external_view = view_alloc();
    app->text = furi_string_alloc(); app->path = furi_string_alloc_set("/ext"); app->external = hid_external_alloc();
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->gui || !app->storage || !app->dialogs || !app->dispatcher || !app->menu || !app->input || !app->settings || !app->widget || !app->external_view || !app->text || !app->path || !app->external || !app->mutex) return app;
    app->show_hashcat = app->show_john = true;
    app->save_history = false;
    app->baud = hid_bauds[0];
    submenu_set_header(app->menu, "HashID FZ v" HID_VERSION);
    submenu_add_item(app->menu, "Manual input", HidMenuManual, hid_selected, app);
    submenu_add_item(app->menu, "Analyze text/file", HidMenuFile, hid_selected, app);
    submenu_add_item(app->menu, "External HashID", HidMenuExternal, hid_selected, app);
    submenu_add_item(app->menu, "History", HidMenuHistory, hid_selected, app);
    submenu_add_item(app->menu, "Settings", HidMenuSettings, hid_selected, app);
    submenu_add_item(app->menu, "About", HidMenuAbout, hid_selected, app);
    const char* labels[] = {"Extended candidates", "Hashcat modes", "John formats", "Save manual history"};
    for(uint8_t i = 0; i < 4U; i++) {
        VariableItem* item = variable_item_list_add(app->settings, labels[i], 2U, hid_bool_changed, app);
        bool default_on = i == 1U || i == 2U;
        variable_item_set_current_value_index(item, default_on ? 1U : 0U);
        variable_item_set_current_value_text(item, default_on ? "On" : "Off");
    }
    VariableItem* baud = variable_item_list_add(app->settings, "External baud", COUNT_OF(hid_bauds), hid_baud_changed, app);
    variable_item_set_current_value_index(baud, 0U); variable_item_set_current_value_text(baud, hid_baud_names[0]);
    VariableItem* version = variable_item_list_add(app->settings, "Version", 1U, NULL, app); variable_item_set_current_value_text(version, HID_VERSION);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, hid_back); view_dispatcher_set_custom_event_callback(app->dispatcher, hid_custom);
    view_dispatcher_set_tick_event_callback(app->dispatcher, hid_tick, 250U);
    view_dispatcher_add_view(app->dispatcher, HidViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->dispatcher, HidViewInput, text_input_get_view(app->input));
    view_dispatcher_add_view(app->dispatcher, HidViewSettings, variable_item_list_get_view(app->settings));
    view_dispatcher_add_view(app->dispatcher, HidViewText, widget_get_view(app->widget)); app->views_added = true;
    view_set_context(app->external_view, app); view_set_draw_callback(app->external_view, hid_external_draw); view_set_input_callback(app->external_view, hid_external_input);
    view_allocate_model(app->external_view, ViewModelTypeLocking, sizeof(HidExternalModel));
    HidExternalModel* model = view_get_model(app->external_view); model->app = app; view_commit_model(app->external_view, false);
    view_dispatcher_add_view(app->dispatcher, HidViewExternal, app->external_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen); return app;
}

static bool hid_valid(HidApp* app) { return app && app->gui && app->storage && app->dialogs && app->dispatcher && app->menu && app->input && app->settings && app->widget && app->external_view && app->text && app->path && app->external && app->mutex; }
static void hid_free(HidApp* app) {
    if(!app) return;
    app->cancel = true;
    hid_join(app);
    if(app->external) hid_external_free(app->external);
    if(app->dispatcher && app->views_added) { view_dispatcher_remove_view(app->dispatcher, HidViewExternal); view_dispatcher_remove_view(app->dispatcher, HidViewText); view_dispatcher_remove_view(app->dispatcher, HidViewSettings); view_dispatcher_remove_view(app->dispatcher, HidViewInput); view_dispatcher_remove_view(app->dispatcher, HidViewMenu); }
    if(app->path) furi_string_free(app->path);
    if(app->text) furi_string_free(app->text);
    if(app->mutex) furi_mutex_free(app->mutex);
    if(app->widget) widget_free(app->widget);
    if(app->external_view) view_free(app->external_view);
    if(app->settings) variable_item_list_free(app->settings);
    if(app->input) text_input_free(app->input);
    if(app->menu) submenu_free(app->menu);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->dialogs) furi_record_close(RECORD_DIALOGS);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t hashid_fz_app(void* p) {
    UNUSED(p); HidApp* app = hid_alloc(); if(!hid_valid(app)) { hid_free(app); return -1; }
    view_dispatcher_switch_to_view(app->dispatcher, HidViewMenu); view_dispatcher_run(app->dispatcher); hid_free(app); return 0;
}
