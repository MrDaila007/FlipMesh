#include "fm_settings.h"
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SETTINGS_VERSION 2

static void write_line(File* f, const char* fmt, ...) {
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    storage_file_write(f, buf, strlen(buf));
}

void settings_save(FlipMeshApp* app) {
    if(!app) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/flipmesh");

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FM_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        write_line(file, "version=%d\n", SETTINGS_VERSION);
        write_line(file, "uart_id=%d\n",    (int)app->uart_id);
        write_line(file, "baud=%lu\n",      (unsigned long)app->baud);
        write_line(file, "vibro=%d\n",      app->vib_on ? 1 : 0);
        write_line(file, "led=%d\n",        app->led_on   ? 1 : 0);
        write_line(file, "ringtone=%d\n",   (int)app->tone);
        write_line(file, "scroll_speed=%d\n",app->scroll_spd);
        write_line(file, "scroll_fps=%d\n", app->framerate);
        write_line(file, "lmh_mode=%d\n",   (int)app->long_msg);
        write_line(file, "hb_idx=%d\n",     (int)app->hb_idx);
        write_line(file, "channels=%d\n",   (int)app->num_ch);
        write_line(file, "timestamps=%d\n", app->show_ts ? 1 : 0);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_load(FlipMeshApp* app) {
    if(!app) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, FM_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    char buffer[320];
    uint16_t n = storage_file_read(file, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Check version; silently accept both v1 and v2 */
    int file_ver = 1;
    char* vpos = strstr(buffer, "version=");
    if(vpos) file_ver = atoi(vpos + 8);
    (void)file_ver; /* future migration hook */

    char* pos = buffer;
    while(pos < buffer + n) {
        char* eol = strchr(pos, '\n');
        if(!eol) eol = buffer + n;

        size_t ll = (size_t)(eol - pos);
        if(ll > 0 && ll < 128) {
            char line[128];
            memcpy(line, pos, ll);
            line[ll] = '\0';

            char* eq = strchr(line, '=');
            if(eq) {
                *eq = '\0';
                const char* key = line;
                int val = atoi(eq + 1);

                if(!strcmp(key, "uart_id")) {
                    app->uart_id = (FuriHalSerialId)val;
                } else if(!strcmp(key, "baud")) {
                    app->baud = (uint32_t)val;
                } else if(!strcmp(key, "vibro")) {
                    app->vib_on = (val != 0);
                } else if(!strcmp(key, "led")) {
                    app->led_on = (val != 0);
                } else if(!strcmp(key, "ringtone")) {
                    if(val >= 0 && val < FM_TONE_COUNT)
                        app->tone = (FMTone)val;
                } else if(!strcmp(key, "scroll_speed")) {
                    if(val >= 1 && val <= 10) app->scroll_spd = (uint8_t)val;
                } else if(!strcmp(key, "scroll_fps")) {
                    if(val >= 1 && val <= 10) app->framerate = (uint8_t)val;
                } else if(!strcmp(key, "lmh_mode")) {
                    if(val >= 0 && val < FM_LMH_COUNT) app->long_msg = (FMLongMsg)val;
                } else if(!strcmp(key, "hb_idx")) {
                    if(val >= 0 && val <= 2) app->hb_idx = (uint8_t)val;
                } else if(!strcmp(key, "channels")) {
                    if(val >= 1 && val <= FM_MAX_CHANNELS) app->num_ch = (uint8_t)val;
                } else if(!strcmp(key, "timestamps")) {
                    app->show_ts = (val != 0);
                }
            }
        }
        pos = eol + 1;
    }
}
