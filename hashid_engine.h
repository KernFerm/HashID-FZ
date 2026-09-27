/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "hashid_db.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HID_MAX_INPUT 4096U
#define HID_MAX_RESULTS 272U

typedef struct {
    uint16_t candidate_indices[HID_MAX_RESULTS];
    uint16_t count;
    uint16_t prototype_matches;
    uint32_t steps;
    bool truncated;
} HidResults;

typedef struct {
    size_t length;
    bool hexadecimal, decimal, base64ish, has_prefix, has_separator;
} HidCharacteristics;

bool hid_identify(const char* input, size_t length, bool extended, HidResults* results);
void hid_characteristics(const char* input, size_t length, HidCharacteristics* characteristics);
