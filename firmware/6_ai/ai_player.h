/* Module 6: AI opponent glue around the header-only wallship_ai.h core.
 * The AI's WallshipAgent is both its own fleet and its shooting brain.
 */
#ifndef AI_PLAYER_H
#define AI_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "wallship_ai.h"

/* Init the agent with a profile and place its fleet randomly. */
bool AiPlayer_Setup(WallshipAgent *ai, uint32_t seed, AIProfile profile);

/* Pick a target, fire it through the shared match rules, then feed the result
 * (and SUNK cell list) back to the AI. Returns SHOT_INVALID only if the game
 * state is inconsistent.
 */
ShotResult AiPlayer_TakeShot(WallshipAgent *ai, WallshipTwoPlayerGame *match,
                             uint8_t ai_index, Position *shot,
                             uint8_t *sunk_length);

#endif
