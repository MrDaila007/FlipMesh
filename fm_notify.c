#include "fm_notify.h"
#include <furi_hal_speaker.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "flipmesh"

/* ── Tone metadata ───────────────────────────────────────────────────────── */

static const char* tone_labels[FM_TONE_COUNT] = {
    "Off",       /* FMToneOff        */
    "Ping",      /* FMTonePing       */
    "Chime",     /* FMToneChime      */
    "Ascend",    /* FMToneAscend     */
    "Pulse",     /* FMTonePulse      */
    "Bell",      /* FMToneBell       */
    "Morse",     /* FMToneMorse      */
    "Sweep",     /* FMToneSweep      */
    "Bounce",    /* FMToneBounce     */
    "Alert",     /* FMToneAlert      */
    "Two",       /* FMToneTwo        */
    "Three",     /* FMToneThree      */
    "Deep",      /* FMToneDeep       */
    "Criss",     /* FMToneCrisscross */
    "Ramp",      /* FMToneRamp       */
    "Click",     /* FMToneClick      */
    "Warp",      /* FMToneWarp       */
    "Chord",     /* FMToneChord      */
    "Blip",      /* FMToneBlip       */
};

const char* fm_tone_name(FMTone t) {
    if((unsigned)t >= FM_TONE_COUNT) return "?";
    return tone_labels[t];
}

/* ── Speaker helper ──────────────────────────────────────────────────────── */

static void beep(float hz, uint32_t ms, float vol) {
    furi_hal_speaker_start(hz, vol);
    furi_delay_ms(ms);
    furi_hal_speaker_stop();
}

static void gap(uint32_t ms) {
    furi_delay_ms(ms);
}

/* ── Tone implementations ────────────────────────────────────────────────── */

void fm_play_tone(FMTone t) {
    if(t == FMToneOff) return;
    if(!furi_hal_speaker_acquire(800)) return;

    switch(t) {
    case FMTonePing:
        /* Single crisp high ping */
        beep(1760, 60, 0.6f);
        break;

    case FMToneChime:
        /* Three descending notes: E5 C5 G4 */
        beep(659, 80, 0.5f); gap(30);
        beep(523, 80, 0.5f); gap(30);
        beep(392, 120, 0.4f);
        break;

    case FMToneAscend:
        /* Four-note rising arpeggio */
        beep(440, 60, 0.4f); gap(20);
        beep(554, 60, 0.5f); gap(20);
        beep(659, 60, 0.5f); gap(20);
        beep(880, 100, 0.6f);
        break;

    case FMTonePulse:
        /* Two identical pulses with gap */
        beep(880, 70, 0.5f); gap(80);
        beep(880, 70, 0.5f);
        break;

    case FMToneBell:
        /* Simulated bell: sharp attack, decay */
        beep(1047, 30, 0.8f); gap(10);
        beep(1047, 80, 0.3f); gap(10);
        beep(1047, 120, 0.1f);
        break;

    case FMToneMorse:
        /* SOS: · · · — — — · · · */
        for(int i = 0; i < 3; i++) { beep(700, 60, 0.5f);  gap(60); }
        for(int i = 0; i < 3; i++) { beep(700, 180, 0.5f); gap(60); }
        for(int i = 0; i < 3; i++) { beep(700, 60, 0.5f);  gap(60); }
        break;

    case FMToneSweep:
        /* Frequency sweep up 300 → 1200 Hz */
        for(float f = 300.0f; f <= 1200.0f; f += 45.0f) {
            beep(f, 12, 0.4f);
        }
        break;

    case FMToneBounce:
        /* Alternating low-high like a rubber ball */
        beep(440, 80, 0.5f); gap(40);
        beep(880, 60, 0.5f); gap(40);
        beep(440, 60, 0.4f); gap(30);
        beep(880, 50, 0.4f);
        break;

    case FMToneAlert:
        /* Urgent two-frequency alternation × 4 */
        for(int i = 0; i < 4; i++) {
            beep(1200, 80, 0.6f);
            beep(600,  80, 0.4f);
        }
        break;

    case FMToneTwo:
        /* Two distinct tones: G4 then B4 */
        beep(392, 100, 0.5f); gap(50);
        beep(494, 150, 0.5f);
        break;

    case FMToneThree:
        /* Three quick rising tones */
        beep(523, 70, 0.5f); gap(30);
        beep(659, 70, 0.5f); gap(30);
        beep(784, 100, 0.5f);
        break;

    case FMToneDeep:
        /* Single low resonant tone */
        beep(196, 350, 0.5f);
        break;

    case FMToneCrisscross:
        /* High-low-high-low crossing pattern */
        beep(1046, 70, 0.5f);
        beep(330,  70, 0.5f);
        beep(1046, 70, 0.5f);
        beep(330,  70, 0.5f);
        break;

    case FMToneRamp:
        /* Smooth exponential ramp up */
        for(int step = 0; step < 12; step++) {
            float f = 200.0f + (float)(step * step) * 5.0f;
            beep(f, 18, 0.4f);
        }
        break;

    case FMToneClick:
        /* Very brief mechanical click */
        beep(2000, 20, 0.7f);
        break;

    case FMToneWarp:
        /* Warp-drive style sweep down then up */
        for(float f = 1200.0f; f >= 300.0f; f -= 60.0f)
            beep(f, 10, 0.4f);
        for(float f = 300.0f; f <= 1200.0f; f += 60.0f)
            beep(f, 10, 0.4f);
        break;

    case FMToneChord:
        /* Simulated chord: rapid arpeggiation of C major */
        beep(523, 40, 0.5f);
        beep(659, 40, 0.5f);
        beep(784, 40, 0.5f);
        beep(1046, 80, 0.5f);
        break;

    case FMToneBlip:
        /* Tiny electronic blip */
        beep(1320, 30, 0.5f); gap(20);
        beep(1760, 30, 0.5f);
        break;

    default:
        break;
    }

    furi_hal_speaker_release();
}

/* ── LED / vibro sequences ───────────────────────────────────────────────── */

static const NotificationSequence seq_vib = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence seq_led = {
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    NULL,
};

static const NotificationSequence seq_vib_led = {
    &message_vibro_on,
    &message_green_255,
    &message_delay_100,
    &message_vibro_off,
    &message_green_0,
    NULL,
};

void fm_notify_message(FlipMeshApp* app) {
    if(!app) return;

    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);

    if(app->vib_on && app->led_on) {
        notification_message(notif, &seq_vib_led);
    } else if(app->vib_on) {
        notification_message(notif, &seq_vib);
    } else if(app->led_on) {
        notification_message(notif, &seq_led);
    }

    furi_record_close(RECORD_NOTIFICATION);

    if(app->tone != FMToneOff) {
        fm_play_tone(app->tone);
    }
}
