#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <zmq.h>
#include "clap/clap.h"

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__) && defined(CLAP_HAS_THREADS_H)
#   define CLAP_HAS_THREAD
#   include <threads.h>
#endif

#define MAX_MACRO_PARAMS 32

static const clap_plugin_descriptor_t s_my_plug_desc = {
        .clap_version = CLAP_VERSION_INIT,
        .id = "dev.defvs.snowblind-ingress",
        .name = "Snowblind Ingress",
        .vendor = "defvs",
        .url = "https://github.com/defvs/snowblind_ingress",
        .manual_url = "https://github.com/defvs/snowblind_ingress",
        .support_url = "https://github.com/defvs/snowblind_ingress",
        .version = "0.0.1",
        .description = "TODO: Description",
        .features = (const char *[]) {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_UTILITY, NULL},
};

typedef struct {
    uint8_t layer_id;
    float macro_params[MAX_MACRO_PARAMS];
} plugin_parameters_t;

typedef struct {
    clap_plugin_t plugin;
    const clap_host_t *host;
    const clap_host_log_t *host_log;
    const clap_host_thread_check_t *host_thread_check;
    const clap_host_state_t *host_state;
    void *zmq_context;
    void *zmq_socket;
    plugin_parameters_t parameters;
} my_plug_t;

////////////////////////////
// clap_plugin_note_ports //
////////////////////////////

static uint32_t my_plug_note_ports_count(const clap_plugin_t *plugin, bool is_input) {
    // Declare 1 note input
    return 1;
}

static bool
my_plug_note_ports_get(const clap_plugin_t *plugin, uint32_t index, bool is_input, clap_note_port_info_t *info) {
    if (index > 0)
        return false;
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "%s", "Note input");
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    return true;
}

static const clap_plugin_note_ports_t s_my_plug_note_ports = {
        .count = my_plug_note_ports_count,
        .get = my_plug_note_ports_get,
};

//////////////////
// clap_params //
/////////////////

static uint32_t my_plug_params_count(const clap_plugin_t *plugin) {
    return MAX_MACRO_PARAMS;
}

static bool my_plug_params_get_info(const clap_plugin_t *plugin, uint32_t param_index, clap_param_info_t *param_info) {
    if (param_index >= MAX_MACRO_PARAMS)
        return false;

    param_info->id = param_index;
    param_info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    snprintf(param_info->name, sizeof(param_info->name), "Macro %u", param_index + 1);
    snprintf(param_info->module, sizeof(param_info->module), "Macro");
    param_info->min_value = 0.0;
    param_info->max_value = 1.0;
    param_info->default_value = 0.5;

    return true;
}

static bool my_plug_params_get_value(const clap_plugin_t *plugin, clap_id param_id, double *out_value) {
    if (param_id >= MAX_MACRO_PARAMS)
        return false;

    my_plug_t *plug = plugin->plugin_data;
    *out_value = plug->parameters.macro_params[param_id];
    return true;
}

static bool my_plug_params_value_to_text(const clap_plugin_t *plugin, clap_id param_id, double value, char *out_buffer,
                                         uint32_t out_buffer_capacity) {
    snprintf(out_buffer, out_buffer_capacity, "%.2f", value);
    return true;
}

static bool my_plug_params_text_to_value(const clap_plugin_t *plugin, clap_id param_id, const char *param_value_text,
                                         double *out_value) {
    *out_value = atof(param_value_text);
    return true;
}

// Function to send all macro parameters via ZeroMQ
void send_all_macro_params(my_plug_t *plug) {
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(plug->parameters));
    memcpy(zmq_msg_data(&msg), &plug->parameters, sizeof(plug->parameters));
    zmq_msg_send(&msg, plug->zmq_socket, 0);
    zmq_msg_close(&msg);
}

static void
my_plug_params_flush(const clap_plugin_t *plugin, const clap_input_events_t *in, const clap_output_events_t *out) {
    my_plug_t *plug = plugin->plugin_data;

    for (uint32_t i = 0; i < in->size(in); ++i) {
        const clap_event_header_t *hdr = in->get(in, i);
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t *evt = (const clap_event_param_value_t *) hdr;
            if (evt->param_id < MAX_MACRO_PARAMS) {
                plug->parameters.macro_params[evt->param_id] = evt->value;
                // Send updated parameters via ZeroMQ
                send_all_macro_params(plug);
            }
        }
    }
}

static const clap_plugin_params_t s_my_plug_params = {
        .count = my_plug_params_count,
        .get_info = my_plug_params_get_info,
        .get_value = my_plug_params_get_value,
        .value_to_text = my_plug_params_value_to_text,
        .text_to_value = my_plug_params_text_to_value,
        .flush = my_plug_params_flush,
};

////////////////
// clap_state //
////////////////

bool my_plug_state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
    my_plug_t *plug = plugin->plugin_data;

    int64_t bytes_written = stream->write(stream, &plug->parameters, sizeof(plug->parameters));
    if (bytes_written != sizeof(plug->parameters)) {
        // TODO: Error handling
        return false;
    }
    return true;
}

bool my_plug_state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
    my_plug_t *plug = plugin->plugin_data;

    int64_t bytes_read = stream->read(stream, &plug->parameters, sizeof(plug->parameters));
    if (bytes_read != sizeof(plug->parameters)) {
        // TODO: Error handling
        return false;
    }

    // TODO: update gui

    return true;
}

static const clap_plugin_state_t s_my_plug_state = {
        .save = my_plug_state_save,
        .load = my_plug_state_load,
};

/////////////////
// clap_plugin //
/////////////////

static bool init(const struct clap_plugin *plugin) {
    my_plug_t *plug = plugin->plugin_data;

    // Fetch host's extensions here
    plug->host_log = (const clap_host_log_t *) plug->host->get_extension(plug->host, CLAP_EXT_LOG);
    plug->host_thread_check = (const clap_host_thread_check_t *) plug->host->get_extension(plug->host,
                                                                                           CLAP_EXT_THREAD_CHECK);
    plug->host_state = (const clap_host_state_t *) plug->host->get_extension(plug->host, CLAP_EXT_STATE);

    // Initialize ZeroMQ
    plug->zmq_context = zmq_ctx_new();
    plug->zmq_socket = zmq_socket(plug->zmq_context, ZMQ_PUB);
    zmq_bind(plug->zmq_socket, "tcp://*:5555");

    return true;
}

static void destroy(const struct clap_plugin *plugin) {
    my_plug_t *plug = plugin->plugin_data;
    zmq_close(plug->zmq_socket);
    zmq_ctx_destroy(plug->zmq_context);
    free(plug);
}

static bool activate(const struct clap_plugin *plugin, double sample_rate, uint32_t min_frames_count,
                     uint32_t max_frames_count) {
    return true;
}

static void deactivate(const struct clap_plugin *plugin) {}

static bool start_processing(const struct clap_plugin *plugin) { return true; }

static void stop_processing(const struct clap_plugin *plugin) {}

static void reset(const struct clap_plugin *plugin) {}

static void my_plug_process_event(my_plug_t *plug, const clap_event_header_t *hdr) {
    if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
        zmq_msg_t msg;
        zmq_msg_init_size(&msg, sizeof(clap_event_header_t) + hdr->size);
        memcpy(zmq_msg_data(&msg), hdr, sizeof(clap_event_header_t) + hdr->size);
        zmq_msg_send(&msg, plug->zmq_socket, 0);
        zmq_msg_close(&msg);
    }
}

static clap_process_status process(const struct clap_plugin *plugin, const clap_process_t *process) {
    my_plug_t *plug = plugin->plugin_data;
    const uint32_t nframes = process->frames_count;
    const uint32_t nev = process->in_events->size(process->in_events);
    uint32_t ev_index = 0;
    uint32_t next_ev_frame = nev > 0 ? 0 : nframes;

    for (uint32_t i = 0; i < nframes;) {
        // Handle every event that happens at frame "i"
        while (ev_index < nev && next_ev_frame == i) {
            const clap_event_header_t *hdr = process->in_events->get(process->in_events, ev_index);
            if (hdr->time != i) {
                next_ev_frame = hdr->time;
                break;
            }

            my_plug_process_event(plug, hdr);
            ++ev_index;

            if (ev_index == nev) {
                next_ev_frame = nframes;
                break;
            }
        }

        // Pass-through audio processing (no modification)
        for (; i < next_ev_frame; ++i) {
            process->audio_outputs[0].data32[0][i] = process->audio_inputs[0].data32[0][i];
            process->audio_outputs[0].data32[1][i] = process->audio_inputs[0].data32[1][i];
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void *get_extension(const struct clap_plugin *plugin, const char *id) {
    if (!strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &s_my_plug_note_ports;
    if (!strcmp(id, CLAP_EXT_STATE))
        return &s_my_plug_state;
    if (!strcmp(id, CLAP_EXT_PARAMS))
        return &s_my_plug_params;
    return NULL;
}

static void on_main_thread(const struct clap_plugin *plugin) {}

// Function to change a macro parameter
void set_macro_param(my_plug_t *plug, uint32_t index, float value) {
    if (index < MAX_MACRO_PARAMS) {
        plug->parameters.macro_params[index] = value;
        send_all_macro_params(plug);
    }
}

// Function to read a macro parameter
float get_macro_param(my_plug_t *plug, uint32_t index) {
    if (index < MAX_MACRO_PARAMS) {
        return plug->parameters.macro_params[index];
    }
    return 0.0f; // Return a default value if index is out of range
}

clap_plugin_t *my_plug_create(const clap_host_t *host) {
    my_plug_t *p = calloc(1, sizeof(*p));
    p->host = host;
    p->plugin.desc = &s_my_plug_desc;
    p->plugin.plugin_data = p;
    p->plugin.init = init;
    p->plugin.destroy = destroy;
    p->plugin.activate = activate;
    p->plugin.deactivate = deactivate;
    p->plugin.start_processing = start_processing;
    p->plugin.stop_processing = stop_processing;
    p->plugin.reset = reset;
    p->plugin.process = process;
    p->plugin.get_extension = get_extension;
    p->plugin.on_main_thread = on_main_thread;

    return &p->plugin;
}

/////////////////////////
// clap_plugin_factory //
/////////////////////////

static struct {
    const clap_plugin_descriptor_t *desc;

    clap_plugin_t *(CLAP_ABI *create)(const clap_host_t *host);
} s_plugins[] = {
        {
                .desc = &s_my_plug_desc,
                .create = my_plug_create,
        },
};

static uint32_t plugin_factory_get_plugin_count(const struct clap_plugin_factory *factory) {
    return sizeof(s_plugins) / sizeof(s_plugins[0]);
}

static const clap_plugin_descriptor_t *
plugin_factory_get_plugin_descriptor(const struct clap_plugin_factory *factory, uint32_t index) {
    return s_plugins[index].desc;
}

static const clap_plugin_t *
plugin_factory_create_plugin(const struct clap_plugin_factory *factory, const clap_host_t *host,
                             const char *plugin_id) {
    if (!clap_version_is_compatible(host->clap_version)) {
        return NULL;
    }

    const int N = sizeof(s_plugins) / sizeof(s_plugins[0]);
    for (int i = 0; i < N; ++i)
        if (!strcmp(plugin_id, s_plugins[i].desc->id))
            return s_plugins[i].create(host);

    return NULL;
}

static const clap_plugin_factory_t s_plugin_factory = {
        .get_plugin_count = plugin_factory_get_plugin_count,
        .get_plugin_descriptor = plugin_factory_get_plugin_descriptor,
        .create_plugin = plugin_factory_create_plugin,
};

////////////////
// clap_entry //
////////////////

static bool entry_init(const char *plugin_path) {
    return true;
}

static void entry_deinit(void) {}

#ifdef CLAP_HAS_THREAD
static mtx_t g_entry_lock;
static once_flag g_entry_once = ONCE_FLAG_INIT;
#endif

static int g_entry_init_counter = 0;

#ifdef CLAP_HAS_THREAD
static void entry_init_guard_init(void) {
    mtx_init(&g_entry_lock, mtx_plain);
}
#endif

static bool entry_init_guard(const char *plugin_path) {
#ifdef CLAP_HAS_THREAD
    call_once(&g_entry_once, entry_init_guard_init);
    mtx_lock(&g_entry_lock);
#endif

    const int cnt = ++g_entry_init_counter;
    assert(cnt > 0);

    bool succeed = true;
    if (cnt == 1) {
        succeed = entry_init(plugin_path);
        if (!succeed)
            g_entry_init_counter = 0;
    }

#ifdef CLAP_HAS_THREAD
    mtx_unlock(&g_entry_lock);
#endif

    return succeed;
}

static void entry_deinit_guard(void) {
#ifdef CLAP_HAS_THREAD
    call_once(&g_entry_once, entry_init_guard_init);
    mtx_lock(&g_entry_lock);
#endif

    const int cnt = --g_entry_init_counter;
    assert(cnt >= 0);

    if (cnt == 0)
        entry_deinit();

#ifdef CLAP_HAS_THREAD
    mtx_unlock(&g_entry_lock);
#endif
}

static const void *entry_get_factory(const char *factory_id) {
#ifdef CLAP_HAS_THREAD
    call_once(&g_entry_once, entry_init_guard_init);
#endif

    assert(g_entry_init_counter > 0);
    if (g_entry_init_counter <= 0)
        return NULL;

    if (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
        return &s_plugin_factory;
    return NULL;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
        .clap_version = CLAP_VERSION_INIT,
        .init = entry_init_guard,
        .deinit = entry_deinit_guard,
        .get_factory = entry_get_factory,
};
