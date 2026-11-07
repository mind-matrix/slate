#ifndef SLATE_NOTIFICATION_HUB_H
#define SLATE_NOTIFICATION_HUB_H

#include "lvgl.h"
#include "notify.h"

typedef void (*notification_open_cb_t)(notify_owner_t owner);

lv_obj_t *notification_hub_create(lv_obj_t *parent, notification_open_cb_t open);
void notification_hub_refresh(lv_obj_t *hub);

#endif
