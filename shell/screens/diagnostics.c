#include "diagnostics.h"

#include <stdio.h>
#include <string.h>
#include "components/header.h"
#include "components/stat_card.h"
#include "sysinfo.h"
#include "theme/theme.h"

#define REFRESH_MS 2000
#define CARD_W 596
#define CARD_H 272
#define LABEL 32

typedef struct {
    lv_obj_t *memory;
    lv_obj_t *cpu;
    lv_obj_t *network;
    lv_obj_t *storage;
    lv_obj_t *uptime;
    lv_timer_t *timer;
    sysinfo_sample_t previous;
} diagnostics_t;

static diagnostics_t view;

static const char *const memory_keys[] = { "Available", "Swap (zram)", "Cache" };
static const char *const cpu_keys[] = { "Temperature", "Frequency", "Load (1 min)" };
static const char *const network_keys[] = { "Download", "Upload", "Interface" };
static const char *const storage_keys[] = { "Free", "Total", "Logs" };

static void update_memory(const sysinfo_sample_t *now)
{
    const sysinfo_memory_t *m = &now->memory;
    uint64_t used = m->total_kib - m->available_kib;
    char value[LABEL], unit[2 * LABEL], total[LABEL], available[LABEL], swap[LABEL], cache[LABEL];

    sysinfo_format_size(used, value, sizeof(value));
    sysinfo_format_size(m->total_kib, total, sizeof(total));
    snprintf(unit, sizeof(unit), "of %s used", total);
    sysinfo_format_size(m->available_kib, available, sizeof(available));
    sysinfo_format_size(m->swap_total_kib - m->swap_free_kib, swap, sizeof(swap));
    sysinfo_format_size(m->cached_kib, cache, sizeof(cache));
    const char *const values[] = { available, swap, cache };
    sui_stat_card_set(view.memory, value, unit, m->total_kib ? (int32_t)(used * 100 / m->total_kib) : 0, values);
}

static void update_cpu(const sysinfo_sample_t *now)
{
    unsigned percent = sysinfo_cpu_percent(view.previous.cpu, now->cpu);
    char value[LABEL], unit[LABEL], temp[LABEL], freq[LABEL], load[LABEL];

    snprintf(value, sizeof(value), "%u%%", percent);
    snprintf(unit, sizeof(unit), "%u cores", now->cpu_count);
    if (now->temp_c >= 0)
        snprintf(temp, sizeof(temp), "%.1f °C", now->temp_c);
    else
        snprintf(temp, sizeof(temp), "—");
    if (now->freq_mhz >= 1000)
        snprintf(freq, sizeof(freq), "%.1f GHz", now->freq_mhz / 1000.0);
    else if (now->freq_mhz)
        snprintf(freq, sizeof(freq), "%u MHz", now->freq_mhz);
    else
        snprintf(freq, sizeof(freq), "—");
    snprintf(load, sizeof(load), "%.2f", now->load1);
    const char *const values[] = { temp, freq, load };
    sui_stat_card_set(view.cpu, value, unit, (int32_t)percent, values);
}

static void update_network(const sysinfo_sample_t *now)
{
    uint64_t seconds = REFRESH_MS / 1000;
    bool same = view.previous.iface[0] && strcmp(view.previous.iface, now->iface) == 0;
    uint64_t rx = same && now->net.rx_bytes >= view.previous.net.rx_bytes ? now->net.rx_bytes - view.previous.net.rx_bytes : 0;
    uint64_t tx = same && now->net.tx_bytes >= view.previous.net.tx_bytes ? now->net.tx_bytes - view.previous.net.tx_bytes : 0;
    char down[LABEL], up[LABEL];

    sysinfo_format_rate(rx / seconds, down, sizeof(down));
    sysinfo_format_rate(tx / seconds, up, sizeof(up));
    const char *const values[] = { down, up, now->iface[0] ? now->iface : "—" };
    sui_stat_card_set(view.network, now->ip, now->iface[0] ? "connected" : "offline", 0, values);
}

static void update_storage(const sysinfo_sample_t *now)
{
    uint64_t used = now->disk_total_kib - now->disk_free_kib;
    char value[LABEL], unit[2 * LABEL], total[LABEL], free_space[LABEL];

    sysinfo_format_size(used, value, sizeof(value));
    sysinfo_format_size(now->disk_total_kib, total, sizeof(total));
    snprintf(unit, sizeof(unit), "of %s used", total);
    sysinfo_format_size(now->disk_free_kib, free_space, sizeof(free_space));
    const char *const values[] = { free_space, total, "In memory" };
    sui_stat_card_set(view.storage, value, unit, now->disk_total_kib ? (int32_t)(used * 100 / now->disk_total_kib) : 0,
                      values);
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    sysinfo_sample_t now;
    char uptime[LABEL], text[2 * LABEL];

    sysinfo_sample(&now);
    update_memory(&now);
    update_cpu(&now);
    update_network(&now);
    update_storage(&now);
    sysinfo_format_uptime(now.uptime_s, uptime, sizeof(uptime));
    snprintf(text, sizeof(text), "Updates every 2 s · up %s", uptime);
    lv_label_set_text(view.uptime, text);
    view.previous = now;
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "System diagnostics");
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(page, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_row(page, SUI_SPACE_L, 0);

    view.memory = sui_stat_card_create(page, "Memory", memory_keys, SUI_COLOR_CAT_SYSTEM);
    view.cpu = sui_stat_card_create(page, "CPU", cpu_keys, SUI_COLOR_CAT_SYSTEM);
    view.network = sui_stat_card_create(page, "Network", network_keys, SUI_COLOR_CAT_SYSTEM);
    view.storage = sui_stat_card_create(page, "Storage", storage_keys, SUI_COLOR_CAT_SYSTEM);
    lv_obj_t *cards[] = { view.memory, view.cpu, view.network, view.storage };
    for (size_t i = 0; i < sizeof(cards) / sizeof(cards[0]); i++)
        lv_obj_set_size(cards[i], CARD_W, CARD_H);

    view.uptime = lv_label_create(screen);
    lv_obj_set_style_text_font(view.uptime, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(view.uptime, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_align(view.uptime, LV_ALIGN_TOP_RIGHT, -SUI_SPACE_XL, SUI_STATUS_BAR_H + SUI_HEADER_H / 2 - 8);

    sysinfo_sample(&view.previous);
    refresh(NULL);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    view.timer = lv_timer_create(refresh, REFRESH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t diagnostics_screen = {
    .name = "diagnostics",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
