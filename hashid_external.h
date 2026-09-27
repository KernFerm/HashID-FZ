/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct HashidExternal HashidExternal;
typedef struct {
    bool active, connected, running;
    uint32_t protocol, serial_errors, files, matched, changed, missing, new_files;
    uint64_t bytes;
    int32_t exit_code;
    char state[16], version[32], target[64], error[64];
} HidExternalSnapshot;

HashidExternal* hid_external_alloc(void);
void hid_external_free(HashidExternal* external);
bool hid_external_start(HashidExternal* external, uint32_t baudrate);
void hid_external_stop(HashidExternal* external);
bool hid_external_run(HashidExternal* external, const char* operation);
bool hid_external_cancel(HashidExternal* external);
void hid_external_snapshot(HashidExternal* external, HidExternalSnapshot* snapshot);
