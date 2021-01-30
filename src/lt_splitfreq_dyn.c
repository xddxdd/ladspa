#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ladspa.h"
#include "utils.h"

typedef enum {
    PORT_CONF_SPLIT_FREQUENCY = 0,
    PORT_CONF_BASS_MAX_BOOST,
    PORT_CONF_BASS_THRESHOLD_DECIBEL,
    PORT_CONF_TREBLE_MAX_BOOST,
    PORT_CONF_TREBLE_THRESHOLD_DECIBEL,
    PORT_INPUT_LEFT,
    PORT_INPUT_RIGHT,
    PORT_OUTPUT_LEFT,
    PORT_OUTPUT_RIGHT,
    PORT_COUNT
} ladspa_port_number_t;

typedef struct {
    LADSPA_Data* conf_split_frequency;
    LADSPA_Data* conf_bass_max_boost;
    LADSPA_Data* conf_bass_threshold_decibel;
    LADSPA_Data* conf_treble_max_boost;
    LADSPA_Data* conf_treble_threshold_decibel;
    LADSPA_Data* port_input_left;
    LADSPA_Data* port_input_right;
    LADSPA_Data* port_output_left;
    LADSPA_Data* port_output_right;

    // Intermediate values
    LADSPA_Data tmp_amount_previous;
    LADSPA_Data tmp_amount_current;

    unsigned long tmp_sample_rate;
    LADSPA_Data   tmp_2pi_over_sample_rate;
    float         tmp_boost_gain;

    LADSPA_Data tmp_last_output;
    LADSPA_Data tmp_last_split_frequency;

    float tmp_bass_boost_current;
    float tmp_treble_boost_current;

    unsigned long tmp_wear_level_counter;
    unsigned long tmp_wear_level_inverse_left_right;
} ladspa_plugin_instance_t;

static LADSPA_Handle ladspa_plugin_init(const LADSPA_Descriptor* Descriptor, unsigned long SampleRate) {
    ladspa_plugin_instance_t* instance = (ladspa_plugin_instance_t*) calloc(1, sizeof(ladspa_plugin_instance_t));
    instance->tmp_sample_rate          = SampleRate;
    instance->tmp_2pi_over_sample_rate = 2.0 * M_PI / SampleRate;
    instance->tmp_wear_level_counter   = 0;
    instance->tmp_bass_boost_current   = 1;
    instance->tmp_treble_boost_current = 1;

    // Set gain to increase 100x (40 dB) per sec
    instance->tmp_boost_gain = powf(100.0f, 1.0f / SampleRate);

    // Randomly select bass channel, to avoid damage to one speaker
    srand(time(NULL));
    instance->tmp_wear_level_inverse_left_right = rand() % 2;
    return instance;
}

static void ladspa_connect_port(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data* DataLocation) {
    ladspa_plugin_instance_t* instance = (ladspa_plugin_instance_t*) Instance;
    switch ((ladspa_port_number_t) Port) {
        case PORT_CONF_SPLIT_FREQUENCY: instance->conf_split_frequency = DataLocation; break;
        case PORT_CONF_BASS_MAX_BOOST: instance->conf_bass_max_boost = DataLocation; break;
        case PORT_CONF_BASS_THRESHOLD_DECIBEL: instance->conf_bass_threshold_decibel = DataLocation; break;
        case PORT_CONF_TREBLE_MAX_BOOST: instance->conf_treble_max_boost = DataLocation; break;
        case PORT_CONF_TREBLE_THRESHOLD_DECIBEL: instance->conf_treble_threshold_decibel = DataLocation; break;
        case PORT_INPUT_LEFT: instance->port_input_left = DataLocation; break;
        case PORT_INPUT_RIGHT: instance->port_input_right = DataLocation; break;
        case PORT_OUTPUT_LEFT: instance->port_output_left = DataLocation; break;
        case PORT_OUTPUT_RIGHT: instance->port_output_right = DataLocation; break;
        default: break;
    }
}

static __attribute__((const)) float decibel_to_float(float decibel) { return powf(10.0f, decibel / 20.0f); }

static void ladspa_execute(LADSPA_Handle Instance, unsigned long SampleCount) {
    ladspa_plugin_instance_t* instance = (ladspa_plugin_instance_t*) Instance;

    if ((*instance->conf_split_frequency) != instance->tmp_last_split_frequency) {
        instance->tmp_last_split_frequency = *instance->conf_split_frequency;

        // Recalculate channel component
        if ((*instance->conf_split_frequency) <= 0) {
            // Reject everything for bass channel
            instance->tmp_amount_current  = 0;
            instance->tmp_amount_previous = 0;
        } else if ((*instance->conf_split_frequency) > instance->tmp_sample_rate / 2) {
            // Above Nyquist frequency, accept everything
            instance->tmp_amount_current  = 1;
            instance->tmp_amount_previous = 0;
        } else {
            LADSPA_Data comp = 2 - cos(instance->tmp_2pi_over_sample_rate * (*instance->conf_split_frequency));
            instance->tmp_amount_previous = comp - sqrt(comp * comp - 1);
            instance->tmp_amount_current  = 1 - instance->tmp_amount_previous;
        }
    }

    for (unsigned long i = 0; i < SampleCount; i++) {
        LADSPA_Data input = 0.5 * (instance->port_input_left[i] + instance->port_input_right[i]);
        instance->tmp_last_output
            = instance->tmp_amount_current * input + instance->tmp_amount_previous * instance->tmp_last_output;

        float bass   = instance->tmp_last_output;
        float treble = input - instance->tmp_last_output;

        // Bass dynamic processing
        instance->tmp_bass_boost_current = fminf(decibel_to_float(*instance->conf_bass_max_boost),
                                                 instance->tmp_bass_boost_current * instance->tmp_boost_gain);

        if (fabs(bass) > decibel_to_float(-80)) {
            // Input is audible, limit on input
            instance->tmp_bass_boost_current
                = fminf(instance->tmp_bass_boost_current,
                        decibel_to_float(*instance->conf_bass_threshold_decibel) / fabs(bass));
        }

        bass *= instance->tmp_bass_boost_current;

        // Treble dynamic processing
        instance->tmp_treble_boost_current = fminf(decibel_to_float(*instance->conf_treble_max_boost),
                                                   instance->tmp_treble_boost_current * instance->tmp_boost_gain);

        if (fabs(treble) > decibel_to_float(-80)) {
            // Input is audible, limit on input
            instance->tmp_treble_boost_current
                = fminf(instance->tmp_treble_boost_current,
                        decibel_to_float(*instance->conf_treble_threshold_decibel) / fabs(treble));
        }

        treble *= instance->tmp_treble_boost_current;

        // Wear leveling between two channels
        if (fabs(input) < decibel_to_float(-80)) {
            // Silent now, decrease counter
            if (instance->tmp_wear_level_counter == 0) {
                // Already switched, do nothing
            } else if (--instance->tmp_wear_level_counter == 0) {
                // Silent going on for 1 sec, perform switching
                instance->tmp_wear_level_inverse_left_right = 1 - instance->tmp_wear_level_inverse_left_right;
            }
        } else {
            // Music is loud now, do not swap
            instance->tmp_wear_level_counter = instance->tmp_sample_rate / 10;
        }

        if (instance->tmp_wear_level_inverse_left_right) {
            instance->port_output_right[i] = bass;
            instance->port_output_left[i]  = treble;
        } else {
            instance->port_output_left[i]  = treble;
            instance->port_output_right[i] = bass;
        }
    }
}

static void ladspa_cleanup(LADSPA_Handle Instance) { free(Instance); }

LADSPA_Descriptor* ladspa_plugin_info = NULL;

ON_LOAD_ROUTINE {
    ladspa_plugin_info = (LADSPA_Descriptor*) malloc(sizeof(LADSPA_Descriptor));
    if (ladspa_plugin_info) {
        ladspa_plugin_info->UniqueID   = 2548;
        ladspa_plugin_info->Label      = strdup("splitfreq");
        ladspa_plugin_info->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
        ladspa_plugin_info->Name       = strdup("LT Stereo Frequency Splitter (w/ Dynamic Boost)");
        ladspa_plugin_info->Maker      = strdup("Lan Tian (based on Richard Furse's LADSPA example plugins)");
        ladspa_plugin_info->Copyright  = strdup("None");
        ladspa_plugin_info->PortCount  = PORT_COUNT;

        LADSPA_PortDescriptor* ladspa_port_desc
            = (LADSPA_PortDescriptor*) calloc(PORT_COUNT, sizeof(LADSPA_PortDescriptor));
        ladspa_port_desc[PORT_CONF_SPLIT_FREQUENCY]          = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
        ladspa_port_desc[PORT_CONF_BASS_MAX_BOOST]           = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
        ladspa_port_desc[PORT_CONF_BASS_THRESHOLD_DECIBEL]   = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
        ladspa_port_desc[PORT_CONF_TREBLE_MAX_BOOST]         = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
        ladspa_port_desc[PORT_CONF_TREBLE_THRESHOLD_DECIBEL] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
        ladspa_port_desc[PORT_INPUT_LEFT]                    = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
        ladspa_port_desc[PORT_INPUT_RIGHT]                   = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
        ladspa_port_desc[PORT_OUTPUT_LEFT]                   = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
        ladspa_port_desc[PORT_OUTPUT_RIGHT]                  = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
        ladspa_plugin_info->PortDescriptors                  = (const LADSPA_PortDescriptor*) ladspa_port_desc;

        char** ladspa_port_names                              = (char**) calloc(PORT_COUNT, sizeof(char*));
        ladspa_port_names[PORT_CONF_SPLIT_FREQUENCY]          = strdup("Split Frequency");
        ladspa_port_names[PORT_CONF_BASS_MAX_BOOST]           = strdup("Bass Max Boost (dB)");
        ladspa_port_names[PORT_CONF_BASS_THRESHOLD_DECIBEL]   = strdup("Bass Threshold (dB)");
        ladspa_port_names[PORT_CONF_TREBLE_MAX_BOOST]         = strdup("Treble Max Boost (dB)");
        ladspa_port_names[PORT_CONF_TREBLE_THRESHOLD_DECIBEL] = strdup("Treble Threshold (dB)");
        ladspa_port_names[PORT_INPUT_LEFT]                    = strdup("Input (Left)");
        ladspa_port_names[PORT_INPUT_RIGHT]                   = strdup("Input (Right)");
        ladspa_port_names[PORT_OUTPUT_LEFT]                   = strdup("Output (Left)");
        ladspa_port_names[PORT_OUTPUT_RIGHT]                  = strdup("Output (Right)");
        ladspa_plugin_info->PortNames                         = (const char**) ladspa_port_names;

        LADSPA_PortRangeHint* ladspa_port_range_hint
            = (LADSPA_PortRangeHint*) calloc(PORT_COUNT, sizeof(LADSPA_PortRangeHint));
        ladspa_port_range_hint[PORT_CONF_SPLIT_FREQUENCY].HintDescriptor
            = (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_SAMPLE_RATE | LADSPA_HINT_LOGARITHMIC
               | LADSPA_HINT_DEFAULT_100);
        ladspa_port_range_hint[PORT_CONF_SPLIT_FREQUENCY].LowerBound = 0;
        ladspa_port_range_hint[PORT_CONF_SPLIT_FREQUENCY].UpperBound
            = 0.5; /* Nyquist frequency (half the sample rate) */
        ladspa_port_range_hint[PORT_CONF_BASS_MAX_BOOST].HintDescriptor
            = (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_100);
        ladspa_port_range_hint[PORT_CONF_BASS_MAX_BOOST].LowerBound = 0;
        ladspa_port_range_hint[PORT_CONF_BASS_MAX_BOOST].UpperBound = 100;
        ladspa_port_range_hint[PORT_CONF_BASS_THRESHOLD_DECIBEL].HintDescriptor
            = (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_0);
        ladspa_port_range_hint[PORT_CONF_BASS_THRESHOLD_DECIBEL].LowerBound = -100;
        ladspa_port_range_hint[PORT_CONF_BASS_THRESHOLD_DECIBEL].UpperBound = 0;
        ladspa_port_range_hint[PORT_CONF_TREBLE_MAX_BOOST].HintDescriptor
            = (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_100);
        ladspa_port_range_hint[PORT_CONF_TREBLE_MAX_BOOST].LowerBound = 0;
        ladspa_port_range_hint[PORT_CONF_TREBLE_MAX_BOOST].UpperBound = 100;
        ladspa_port_range_hint[PORT_CONF_TREBLE_THRESHOLD_DECIBEL].HintDescriptor
            = (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_0);
        ladspa_port_range_hint[PORT_CONF_TREBLE_THRESHOLD_DECIBEL].LowerBound = -100;
        ladspa_port_range_hint[PORT_CONF_TREBLE_THRESHOLD_DECIBEL].UpperBound = 0;
        ladspa_port_range_hint[PORT_INPUT_LEFT].HintDescriptor                = 0;
        ladspa_port_range_hint[PORT_INPUT_RIGHT].HintDescriptor               = 0;
        ladspa_port_range_hint[PORT_OUTPUT_LEFT].HintDescriptor               = 0;
        ladspa_port_range_hint[PORT_OUTPUT_RIGHT].HintDescriptor              = 0;
        ladspa_plugin_info->PortRangeHints = (const LADSPA_PortRangeHint*) ladspa_port_range_hint;

        ladspa_plugin_info->instantiate         = ladspa_plugin_init;
        ladspa_plugin_info->connect_port        = ladspa_connect_port;
        ladspa_plugin_info->activate            = NULL;
        ladspa_plugin_info->run                 = ladspa_execute;
        ladspa_plugin_info->run_adding          = NULL;
        ladspa_plugin_info->set_run_adding_gain = NULL;
        ladspa_plugin_info->deactivate          = NULL;
        ladspa_plugin_info->cleanup             = ladspa_cleanup;
    }
}

/*****************************************************************************/

static void deleteDescriptor(LADSPA_Descriptor* psDescriptor) {
    unsigned long lIndex;
    if (psDescriptor) {
        free((char*) psDescriptor->Label);
        free((char*) psDescriptor->Name);
        free((char*) psDescriptor->Maker);
        free((char*) psDescriptor->Copyright);
        free((LADSPA_PortDescriptor*) psDescriptor->PortDescriptors);
        for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++) free((char*) (psDescriptor->PortNames[lIndex]));
        free((char**) psDescriptor->PortNames);
        free((LADSPA_PortRangeHint*) psDescriptor->PortRangeHints);
        free(psDescriptor);
    }
}

/*****************************************************************************/

/* Called automatically when the library is unloaded. */
ON_UNLOAD_ROUTINE { deleteDescriptor(ladspa_plugin_info); }

/*****************************************************************************/

/* Return a descriptor of the requested plugin type. There are two
   plugin types available in this library (mono and stereo). */
const LADSPA_Descriptor* ladspa_descriptor(unsigned long Index) {
    /* Return the requested descriptor or null if the index is out of
        range. */
    switch (Index) {
        case 0: return ladspa_plugin_info;
        default: return NULL;
    }
}
