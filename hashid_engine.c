/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hashid_engine.h"
#include <ctype.h>
#include <string.h>

#define HID_MAX_DEPTH 96U
#define HID_STEP_LIMIT 500000U

typedef enum { HidContSequence, HidContRepeat } HidContType;
typedef struct HidContinuation HidContinuation;
struct HidContinuation {
    HidContType type;
    const HidContinuation* parent;
    uint16_t sequence, index, repeat_node, count;
    size_t previous;
};
typedef struct { const uint8_t* input; size_t length; uint32_t steps; } HidMatch;

static uint8_t hid_lower(uint8_t c) {
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c + ('a' - 'A')) : c;
}

static bool hid_class_match(uint16_t class_id, uint8_t value) {
    const HidClass* definition = &hid_classes[class_id];
    uint8_t lower = hid_lower(value);
    bool matched = false;
    for(uint8_t i = 0; i < definition->count; i++) {
        const HidClassAtom* atom = &hid_class_atoms[definition->first + i];
        uint8_t a = hid_lower(atom->a), b = hid_lower(atom->b);
        if((atom->type == HidClassLiteral && lower == a) ||
           (atom->type == HidClassRange && lower >= a && lower <= b) ||
           (atom->type == HidClassDigit && value >= '0' && value <= '9') ||
           (atom->type == HidClassSpace && isspace(value)) ||
           (atom->type == HidClassWord && (isalnum(value) || value == '_'))) {
            matched = true;
            break;
        }
    }
    return definition->negate ? !matched : matched;
}

static bool hid_atom(const HidNode* node, const HidMatch* match, size_t position, size_t* next) {
    if(node->type == HidNodeBegin) { *next = position; return position == 0U; }
    if(node->type == HidNodeEnd) { *next = position; return position == match->length; }
    if(position >= match->length) return false;
    uint8_t value = match->input[position];
    bool okay = (node->type == HidNodeLiteral && hid_lower(value) == hid_lower((uint8_t)node->a)) ||
                (node->type == HidNodeNotLiteral && hid_lower(value) != hid_lower((uint8_t)node->a)) ||
                (node->type == HidNodeAny && value != '\n') ||
                (node->type == HidNodeClass && hid_class_match(node->a, value));
    if(okay) *next = position + 1U;
    return okay;
}

static bool hid_match_sequence(HidMatch*, uint16_t, uint16_t, size_t, const HidContinuation*, uint8_t);
static bool hid_resume(HidMatch*, const HidContinuation*, size_t, uint8_t);

static bool hid_match_repeat(
    HidMatch* match,
    uint16_t node_index,
    uint16_t count,
    size_t position,
    const HidContinuation* after,
    uint8_t depth) {
    if(depth > HID_MAX_DEPTH || ++match->steps > HID_STEP_LIMIT) return false;
    const HidNode* repeat = &hid_nodes[node_index];
    const HidSequence* child = &hid_sequences[repeat->child];
    uint32_t maximum = repeat->b == 0xFFFFU ? (uint32_t)match->length + 1U : repeat->b;
    if(child->count == 1U) {
        const HidNode* atom = &hid_nodes[child->first];
        if(atom->type <= HidNodeClass || atom->type == HidNodeBegin || atom->type == HidNodeEnd) {
            size_t positions_end = position;
            uint32_t consumed = count;
            while(consumed < maximum) {
                size_t next;
                if(!hid_atom(atom, match, positions_end, &next) || next == positions_end) break;
                positions_end = next;
                consumed++;
            }
            while(consumed >= repeat->a) {
                if(hid_resume(match, after, positions_end, (uint8_t)(depth + 1U))) return true;
                if(consumed == count) break;
                positions_end--;
                consumed--;
            }
            return false;
        }
    }
    if(count < maximum) {
        HidContinuation continuation = {
            .type = HidContRepeat,
            .parent = after,
            .repeat_node = node_index,
            .count = (uint16_t)(count + 1U),
            .previous = position,
        };
        if(hid_match_sequence(match, repeat->child, 0U, position, &continuation, (uint8_t)(depth + 1U)))
            return true;
    }
    return count >= repeat->a && hid_resume(match, after, position, (uint8_t)(depth + 1U));
}

static bool hid_resume(HidMatch* match, const HidContinuation* continuation, size_t position, uint8_t depth) {
    if(!continuation) return position == match->length;
    if(continuation->type == HidContSequence)
        return hid_match_sequence(match, continuation->sequence, continuation->index, position, continuation->parent, depth);
    if(position == continuation->previous) return false;
    return hid_match_repeat(match, continuation->repeat_node, continuation->count, position, continuation->parent, depth);
}

static bool hid_match_sequence(
    HidMatch* match,
    uint16_t sequence_id,
    uint16_t index,
    size_t position,
    const HidContinuation* continuation,
    uint8_t depth) {
    if(depth > HID_MAX_DEPTH || ++match->steps > HID_STEP_LIMIT) return false;
    const HidSequence* sequence = &hid_sequences[sequence_id];
    if(index >= sequence->count) return hid_resume(match, continuation, position, depth);
    uint16_t node_index = (uint16_t)(sequence->first + index);
    const HidNode* node = &hid_nodes[node_index];
    if(node->type <= HidNodeClass || node->type == HidNodeBegin || node->type == HidNodeEnd) {
        size_t next;
        return hid_atom(node, match, position, &next) &&
               hid_match_sequence(match, sequence_id, (uint16_t)(index + 1U), next, continuation, depth);
    }
    HidContinuation after = {
        .type = HidContSequence,
        .parent = continuation,
        .sequence = sequence_id,
        .index = (uint16_t)(index + 1U),
    };
    if(node->type == HidNodeSubsequence)
        return hid_match_sequence(match, node->a, 0U, position, &after, (uint8_t)(depth + 1U));
    if(node->type == HidNodeRepeat)
        return hid_match_repeat(match, node_index, 0U, position, &after, (uint8_t)(depth + 1U));
    if(node->type == HidNodeBranch) {
        for(uint16_t i = 0; i < node->b; i++)
            if(hid_match_sequence(match, hid_branch_sequences[node->a + i], 0U, position, &after, (uint8_t)(depth + 1U)))
                return true;
    }
    return false;
}

bool hid_identify(const char* input, size_t length, bool extended, HidResults* results) {
    if(!input || !results) return false;
    memset(results, 0, sizeof(*results));
    if(!length || length > HID_MAX_INPUT) return false;
    HidMatch match = {.input = (const uint8_t*)input, .length = length};
    for(size_t i = 0; i < hid_prototype_count; i++) {
        const HidPrototype* prototype = &hid_prototypes[i];
        if(hid_match_sequence(&match, prototype->sequence, 0U, 0U, NULL, 0U)) {
            results->prototype_matches++;
            for(uint8_t j = 0; j < prototype->candidate_count; j++) {
                uint16_t candidate = (uint16_t)(prototype->first_candidate + j);
                if(!extended && hid_candidates[candidate].extended) continue;
                if(results->count < HID_MAX_RESULTS)
                    results->candidate_indices[results->count++] = candidate;
                else results->truncated = true;
            }
        }
        if(match.steps > HID_STEP_LIMIT) { results->truncated = true; break; }
    }
    results->steps = match.steps;
    return true;
}

void hid_characteristics(const char* input, size_t length, HidCharacteristics* value) {
    memset(value, 0, sizeof(*value));
    if(!input) return;
    value->length = length;
    value->hexadecimal = value->length > 0U;
    value->decimal = value->length > 0U;
    value->base64ish = value->length > 0U;
    for(size_t i = 0; i < value->length; i++) {
        uint8_t c = (uint8_t)input[i];
        if(!isxdigit(c)) value->hexadecimal = false;
        if(!isdigit(c)) value->decimal = false;
        if(!(isalnum(c) || c == '+' || c == '/' || c == '=' || c == '.' || c == '-')) value->base64ish = false;
        if(c == ':' || c == '$' || c == '*' || c == '#') value->has_separator = true;
    }
    value->has_prefix = input[0] == '$' || input[0] == '{' || input[0] == '*' ||
                        (value->length > 1U && input[0] == '0' && (input[1] == 'x' || input[1] == 'X'));
}
