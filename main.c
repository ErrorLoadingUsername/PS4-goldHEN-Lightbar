// temp_lightbar 2.0 - GoldHEN-Plugin
// Zeigt die CPU-Temperatur der PS4 als Farbe der DualShock-4-Lightbar.
//
// Aufbau:
//  - Keine Hooks. Hooks auf die kurzen scePad-Funktionen haben Aufrufe
//    verschluckt (SetLightBar) bzw. das Spiel aufgehaengt (Read). Die Farbe
//    wird stattdessen direkt mit scePadSetLightBar gesetzt.
//  - Controller-Handles kommen ueber scePadGetHandle je angemeldetem Benutzer.
//  - Temperatur: sceKernelGetCpuTemperature ist intern ein ioctl auf /dev/sbi.
//    Das Geraet ist in der Spiel-Sandbox unsichtbar und der Treiber prueft die
//    Rechte bei jedem Aufruf. Deshalb wird /dev/sbi einmal per GoldHEN-
//    Jailbreak geoeffnet und fuer jede Abfrage nur fuer die Dauer des einen
//    ioctl-Aufrufs gejailbreakt.

#include "plugin_common.h"
#include "Common.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <Syscall.h>
#include <GoldHEN.h>

int *__error(void); // errno der PS4-libkernel

attr_public const char *g_pluginName = "temp_lightbar";
attr_public const char *g_pluginDesc = "CPU-Temperatur als Lightbar-Farbe";
attr_public const char *g_pluginAuth = "Domi";
attr_public u32 g_pluginVersion = 0x00000200; // 2.00

// ---------------------------------------------------------------- Einstellungen

#define START_DELAY_US    (8 * 1000 * 1000)  // Spiel erst hochfahren lassen
#define TICK_US           (2 * 1000 * 1000)  // Farbe alle 2 s auffrischen
#define TEMP_EVERY_TICKS  5                  // Temperatur alle 5 Ticks = 10 s lesen
#define MAX_PADS          8

typedef struct {
    int temp;
    uint8_t r, g, b;
} ColorStop;

static const ColorStop g_stops[] = {
    {45,   0,   0, 255}, // bis 45: blau
    {58,   0, 255,   0}, // gruen
    {68, 255, 200,   0}, // gelb
    {78, 255,   0,   0}, // ab 78: rot
};
#define NUM_STOPS (sizeof(g_stops) / sizeof(g_stops[0]))

// ---------------------------------------------------------------- Zustand

static volatile bool g_run = false;
static OrbisPthread g_thread;
static int g_sbiFd = -1;

typedef int (*GetLoginUsersFn)(OrbisUserServiceLoginUserIdList *list);
typedef int (*PadGetHandleFn)(int userId, unsigned int type, unsigned int index);
static GetLoginUsersFn p_getLoginUsers = NULL;
static PadGetHandleFn p_padGetHandle = NULL;

#define SBI_IOCTL_CPU_TEMP 0x4004A501UL

// ---------------------------------------------------------------- Hilfen

static void notify(const char *text) {
    NotifyStatic(TEX_ICON_SYSTEM, text);
}

static void *resolve(const char *lib, const char *sym) {
    char path[256];
    int handle = 0;
    void *addr = NULL;

    snprintf(path, sizeof(path), "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), lib);
    if (sys_dynlib_load_prx(path, &handle) != 0 || handle <= 0) {
        handle = 0;
        if (sys_dynlib_load_prx(lib, &handle) != 0 || handle <= 0) return NULL;
    }
    if (sys_dynlib_dlsym(handle, sym, &addr) != 0) return NULL;
    return addr;
}

static int get_pads(int *pads) {
    OrbisUserServiceLoginUserIdList list;
    int n = 0;
    if (!p_getLoginUsers || !p_padGetHandle || p_getLoginUsers(&list) != 0) return 0;
    for (int u = 0; u < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; u++) {
        int uid = list.userId[u];
        if (uid == ORBIS_USER_SERVICE_USER_ID_INVALID) continue;
        int h = p_padGetHandle(uid, 0, 0);
        if (h >= 0 && n < MAX_PADS) pads[n++] = h;
    }
    return n;
}

// ---------------------------------------------------------------- Farbverlauf

static uint8_t lerp8(uint8_t a, uint8_t b, int num, int den) {
    return (uint8_t)(a + ((int)b - (int)a) * num / den);
}

static void temp_to_color(int t, OrbisPadColor *c) {
    const ColorStop *lo = &g_stops[0];
    const ColorStop *hi = &g_stops[NUM_STOPS - 1];

    c->a = 255;
    if (t <= lo->temp) { c->r = lo->r; c->g = lo->g; c->b = lo->b; return; }
    if (t >= hi->temp) { c->r = hi->r; c->g = hi->g; c->b = hi->b; return; }

    for (size_t i = 0; i + 1 < NUM_STOPS; i++) {
        const ColorStop *a = &g_stops[i];
        const ColorStop *b = &g_stops[i + 1];
        if (t >= a->temp && t <= b->temp) {
            int num = t - a->temp;
            int den = b->temp - a->temp;
            c->r = lerp8(a->r, b->r, num, den);
            c->g = lerp8(a->g, b->g, num, den);
            c->b = lerp8(a->b, b->b, num, den);
            return;
        }
    }
}

// ---------------------------------------------------------------- Temperatur

static int open_sbi(void) {
    struct jailbreak_backup jb;
    int err = 0;
    if (sys_sdk_jailbreak(&jb) != 0) return -1;
    g_sbiFd = open("/dev/sbi", O_RDWR);
    if (g_sbiFd < 0) g_sbiFd = open("/dev/sbi", O_RDONLY);
    if (g_sbiFd < 0) err = *__error();
    sys_sdk_unjailbreak(&jb);
    return g_sbiFd >= 0 ? 0 : (err ? err : -2);
}

static int read_temp(int *t) {
    struct jailbreak_backup jb;
    int ret;
    if (g_sbiFd < 0) return -1;
    if (sys_sdk_jailbreak(&jb) != 0) return -2;
    ret = ioctl(g_sbiFd, SBI_IOCTL_CPU_TEMP, t) == 0 ? 0 : *__error();
    sys_sdk_unjailbreak(&jb);
    if (ret == 0 && (*t <= 0 || *t > 120)) ret = -3;
    return ret;
}

// ---------------------------------------------------------------- Thread

static void *temp_thread(void *arg) {
    (void)arg;
    int pads[MAX_PADS];
    OrbisPadColor color = {0, 0, 255, 255};
    bool haveColor = false;
    bool errShown = false;
    int tick = 0;

    sceKernelUsleep(START_DELAY_US);

    int openRet = open_sbi();
    if (openRet != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "temp_lightbar: Temperatur-Geraet nicht verfuegbar (%d)", openRet);
        notify(msg);
        return NULL;
    }

    while (g_run) {
        if (tick % TEMP_EVERY_TICKS == 0) {
            int temp = 0;
            int ret = read_temp(&temp);
            if (ret == 0) {
                temp_to_color(temp, &color);
                if (!haveColor) {
                    char msg[96];
                    snprintf(msg, sizeof(msg), "temp_lightbar 2.0 laeuft: %d C", temp);
                    notify(msg);
                }
                haveColor = true;
            } else if (!errShown) {
                char msg[96];
                snprintf(msg, sizeof(msg), "temp_lightbar: Temperatur nicht lesbar (%d)", ret);
                notify(msg);
                errShown = true;
            }
        }

        if (haveColor) {
            int n = get_pads(pads);
            for (int i = 0; i < n; i++) scePadSetLightBar(pads[i], &color);
        }

        tick++;
        sceKernelUsleep(TICK_US);
    }
    return NULL;
}

// ---------------------------------------------------------------- Plugin-Einstieg

s32 attr_public plugin_load(s32 argc, const char *argv[]) {
    (void)argc; (void)argv;
    final_printf("[GoldHEN] %s Plugin gestartet\n", g_pluginName);

    p_getLoginUsers = (GetLoginUsersFn)resolve("libSceUserService.sprx", "sceUserServiceGetLoginUserIdList");
    p_padGetHandle = (PadGetHandleFn)resolve("libScePad.sprx", "scePadGetHandle");
    if (!p_getLoginUsers || !p_padGetHandle) {
        notify("temp_lightbar: Controller-Funktionen nicht gefunden");
        return 0;
    }

    g_run = true;
    scePthreadCreate(&g_thread, NULL, temp_thread, NULL, "temp_lightbar");
    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char *argv[]) {
    (void)argc; (void)argv;
    if (g_run) {
        g_run = false;
        scePthreadJoin(g_thread, NULL);
    }
    if (g_sbiFd >= 0) { close(g_sbiFd); g_sbiFd = -1; }
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args) {
    (void)argc; (void)args;
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args) {
    (void)argc; (void)args;
    return 0;
}
