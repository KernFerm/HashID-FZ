/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HID_EXTERNAL_PROTOCOL_VERSION 1U
#define HID_EXTERNAL_LINE_MAX 256U

typedef enum { HidExternalNone, HidExternalInfo, HidExternalStatus, HidExternalError } HidExternalType;
typedef struct {
    HidExternalType type;
    uint32_t protocol;
    uint64_t bytes;
    uint32_t files;
    uint32_t matched;
    uint32_t changed;
    uint32_t missing;
    uint32_t new_files;
    int32_t exit_code;
    char state[16];
    char version[32];
    char target[64];
    char error[64];
} HidExternalMessage;
typedef struct { char line[HID_EXTERNAL_LINE_MAX]; size_t length; bool overflow; } HidExternalDecoder;
typedef void (*HidExternalCallback)(const HidExternalMessage*, void*);

void hid_external_decoder_reset(HidExternalDecoder* decoder);
bool hid_external_decoder_feed(HidExternalDecoder* decoder, const uint8_t* data, size_t length, HidExternalCallback callback, void* context);
bool hid_external_parse_line(const char* line, HidExternalMessage* message);
