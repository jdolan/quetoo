#pragma once

/**
 * @brief
 */
typedef uint16_t ai_node_id_t;

/**
 * @brief
 */
#define NODE_INVALID	((ai_node_id_t)-1)

/**
 * @brief
 */
ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible);

/**
 * @brief
 */
guint Ai_Node_Count(void);

/**
 * @brief
 */
void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd);

/**
 * @brief
 */
void Ai_Node_Render(const g_entity_t *player);

/**
 * @brief
 */
void Ai_InitNodes(const char *mapname);

/**
 * @brief
 */
void Ai_SaveNodes(void);

/**
 * @brief
 */
void Ai_ShutdownNodes(void);

/**
 * @brief
 */
vec3_t Ai_Node_GetPosition(const ai_node_id_t node);

/**
 * @brief
 */
typedef float (*Ai_NodeCost_Func) (const ai_node_id_t a, const ai_node_id_t b);

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
static inline float Ai_Node_DefaultHeuristic(const ai_node_id_t a, const ai_node_id_t b) {
	const vec3_t av = Ai_Node_GetPosition(a);
	const vec3_t bv = Ai_Node_GetPosition(b);

	return fabs(av.x - bv.x) + fabs(av.y - bv.y) + fabs(av.z - bv.z);
}

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
static inline float Ai_Node_DefaultCost(const ai_node_id_t a, const ai_node_id_t b) {
	const vec3_t av = Ai_Node_GetPosition(a);
	const vec3_t bv = Ai_Node_GetPosition(b);

	return Vec3_Distance(av, bv);
}

/**
 * @brief
 */
GArray *Ai_Node_FindPath(const ai_node_id_t start, const ai_node_id_t end, const Ai_NodeCost_Func heuristic, const Ai_NodeCost_Func cost);
