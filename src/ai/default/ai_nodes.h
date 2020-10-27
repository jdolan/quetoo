#pragma once

typedef uint16_t ai_node_id_t;

#define NODE_INVALID	((ai_node_id_t)-1)

ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance);

void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd);

void Ai_Node_Render(const g_entity_t *player);