#ifndef TC_RECV_H
#define TC_RECV_H
#include <stdint.h>
#include "tc_ui.h"
#include "tc_imgdec.h"

/* Thumbnail pool: TC_UI_LOG_MAX slots of TC_THUMB_W x TC_THUMB_H RGBA.
 * Slot i belongs to ui->log[i], so shifting the list shifts the pixels too. */
#define TC_POOL_BYTES ((size_t)TC_UI_LOG_MAX * TC_THUMB_W * TC_THUMB_H * 4)

/* Add one entry object (as sent in `entry.entry`, or an element of
 * `joined.log`). Entries without a `png` - system lines - are ignored. */
void tc_recv_push(tc_ui *ui, uint8_t *pool, const char *entry_obj);

/* Replace the list from a `joined` message's log array, oldest first. */
void tc_recv_replay(tc_ui *ui, uint8_t *pool, const char *joined_msg);
#endif
