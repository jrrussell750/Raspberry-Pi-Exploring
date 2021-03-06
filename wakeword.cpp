/*  This file has been modified from its original form to allow it to be run as a background process.  The changes include */
/*  internalizing the file assignments and opening a python program when the wake word is recognized     */
/*  April 22, 2021  jrr  */

#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>


#include "pv_porcupine.h"

static volatile bool is_interrupted = false;

void interrupt_handler(int _) {
    (void) _;
    is_interrupted = true;
}

int main() {  /* file assignments are now internal  */
    
    signal(SIGINT, interrupt_handler);

    const char *library_path = "/home/pi/porcupine/lib/raspberry-pi/cortex-a72/libpv_porcupine.so";
    const char *model_path = "/home/pi/porcupine/lib/common/porcupine_params.pv";
    const char *keyword_path = "/home/pi/porcupine/resources/keyword_files/raspberry-pi/porcupine_raspberry-pi.ppn";
    const float sensitivity = 0.5;
    const char *input_audio_device = "plughw:CARD=Device,DEV=0";

    void *porcupine_library = dlopen(library_path, RTLD_NOW);
    if (!porcupine_library) {
        fprintf(stderr, "failed to open library.\n");
        exit(1);
    }

    char *error = NULL;

    const char *(*pv_status_to_string_func)(pv_status_t) = dlsym(porcupine_library, "pv_status_to_string");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_status_to_string' with '%s'.\n", error);
        exit(1);
    }

    int32_t (*pv_sample_rate_func)() = dlsym(porcupine_library, "pv_sample_rate");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_sample_rate' with '%s'.\n", error);
        exit(1);
    }

    pv_status_t (*pv_porcupine_init_func)(const char *, int32_t, const char *const *, const float *, pv_porcupine_t **) =
            dlsym(porcupine_library, "pv_porcupine_init");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_porcupine_init' with '%s'.\n", error);
        exit(1);
    }

    void (*pv_porcupine_delete_func)(pv_porcupine_t *) = dlsym(porcupine_library, "pv_porcupine_delete");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_porcupine_delete' with '%s'.\n", error);
        exit(1);
    }

    pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int32_t *) =
            dlsym(porcupine_library, "pv_porcupine_process");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_porcupine_process' with '%s'.\n", error);
        exit(1);
    }

    int32_t (*pv_porcupine_frame_length_func)() = dlsym(porcupine_library, "pv_porcupine_frame_length");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "failed to load 'pv_porcupine_frame_length' with '%s'.\n", error);
        exit(1);
    }

    pv_porcupine_t *porcupine = NULL;
    pv_status_t status = pv_porcupine_init_func(model_path, 1, &keyword_path, &sensitivity, &porcupine);
    if (status != PV_STATUS_SUCCESS) {
        fprintf(stderr, "'pv_porcupine_init' failed with '%s'\n", pv_status_to_string_func(status));
        exit(1);
    }

    snd_pcm_t *alsa_handle = NULL;
    int error_code = snd_pcm_open(&alsa_handle, input_audio_device, SND_PCM_STREAM_CAPTURE, 0);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_open' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    snd_pcm_hw_params_t *hardware_params = NULL;
    error_code = snd_pcm_hw_params_malloc(&hardware_params);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_malloc' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params_any(alsa_handle, hardware_params);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_any' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params_set_access(alsa_handle, hardware_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_set_access' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params_set_format(alsa_handle, hardware_params, SND_PCM_FORMAT_S16_LE);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_set_format' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params_set_rate(alsa_handle, hardware_params, pv_sample_rate_func(), 0);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_set_rate' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params_set_channels(alsa_handle, hardware_params, 1);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params_set_channels' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    error_code = snd_pcm_hw_params(alsa_handle, hardware_params);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_hw_params' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    snd_pcm_hw_params_free(hardware_params);

    error_code = snd_pcm_prepare(alsa_handle);
    if (error_code != 0) {
        fprintf(stderr, "'snd_pcm_prepare' failed with '%s'\n", snd_strerror(error_code));
        exit(1);
    }

    const int32_t frame_length = pv_porcupine_frame_length_func();

    int16_t *pcm = malloc(frame_length * sizeof(int16_t));
    if (!pcm) {
        fprintf(stderr, "failed to allocate memory for audio buffer\n");
        exit(1);
    }

    while (!is_interrupted) {
        const int count = snd_pcm_readi(alsa_handle, pcm, frame_length);
        if (count < 0) {
            fprintf(stderr, "'snd_pcm_readi' failed with '%s'\n", snd_strerror(count));
            exit(1);
        } else if (count != frame_length) {
            fprintf(stderr, "read %d frames instead of %d\n", count, frame_length);
            exit(1);
        }

        int32_t keyword_index = -1;
        status = pv_porcupine_process_func(porcupine, pcm, &keyword_index);
        if (status != PV_STATUS_SUCCESS) {
            fprintf(stderr, "'pv_porcupine_process' failed with '%s'\n", pv_status_to_string_func(status));
            exit(1);
        }
        if (keyword_index != -1) {
            system("python3 /home/pi/hello.py");

        }
    }

    free(pcm);
    snd_pcm_close(alsa_handle);
    pv_porcupine_delete_func(porcupine);
    dlclose(porcupine_library);

    return 0;
}
