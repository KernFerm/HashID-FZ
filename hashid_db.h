/* Generated from upstream HashID; do not edit manually. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { HidNodeLiteral, HidNodeNotLiteral, HidNodeAny, HidNodeClass,
    HidNodeSubsequence, HidNodeRepeat, HidNodeBranch, HidNodeBegin, HidNodeEnd } HidNodeType;
typedef enum { HidClassLiteral, HidClassRange, HidClassDigit, HidClassSpace, HidClassWord } HidClassType;
typedef struct { uint8_t type; uint16_t a, b, child; } HidNode;
typedef struct { uint16_t first, count; } HidSequence;
typedef struct { uint8_t type; uint8_t a, b; } HidClassAtom;
typedef struct { uint16_t first; uint8_t count, negate; } HidClass;
typedef struct { uint16_t sequence, first_candidate; uint8_t candidate_count; } HidPrototype;
typedef struct { const char* name; int32_t hashcat; const char* john; bool extended; } HidCandidate;

extern const HidNode hid_nodes[];
extern const HidSequence hid_sequences[];
extern const HidClassAtom hid_class_atoms[];
extern const HidClass hid_classes[];
extern const uint16_t hid_branch_sequences[];
extern const HidPrototype hid_prototypes[];
extern const HidCandidate hid_candidates[];
extern const size_t hid_prototype_count, hid_candidate_count;
