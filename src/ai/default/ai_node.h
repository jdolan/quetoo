#pragma once

/**
 * @brief
 */
guint Ai_Node_Count(void);

/**
 * @brief
 */
ai_node_id_t Ai_Node_CreateNode(const vec3_t position);

/**
 * @brief
 */
_Bool Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b);

/**
 * @brief
 */
GArray *Ai_Node_GetLinks(const ai_node_id_t a);

/**
 * @brief
 */
vec3_t Ai_Node_GetPosition(const ai_node_id_t node);

/**
 * @brief
 */
ai_node_id_t Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible, const bool prefer_level);

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
_Bool Ai_Node_CanPathTo(const vec3_t position);

/**
 * @brief Check if the node we want to move towards along the path is currently pathable.
 */
_Bool Ai_Path_CanPathTo(const GArray *path, const guint index);

/**
 * @brief
 */
void Ai_Node_CreateLink(const ai_node_id_t a, const ai_node_id_t b, const float cost);

/**
 * @brief
 */
void Ai_Node_PlayerRoam(const g_entity_t *player, const pm_cmd_t *cmd);

/**
 * @brief
 */
void Ai_Node_Render(void);

/**
 * @brief
 */
void Ai_InitNodes(const char *mapname);

/**
 * @brief
 */
void Ai_NodesReady(void);

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
typedef float (*Ai_NodeCost_Func)(const ai_node_id_t a, const ai_node_id_t b);

/**
 * @brief
 */
static inline float Ai_Node_DefaultHeuristic(const ai_node_id_t link, const ai_node_id_t end) {
	const vec3_t av = Ai_Node_GetPosition(link);
	const vec3_t bv = Ai_Node_GetPosition(end);
	float cost = fabsf(av.x - bv.x) + fabsf(av.y - bv.y) + fabsf(av.z - bv.z);

	if (!Ai_Node_CanPathTo(av)) {
		cost *= 8.f;
	}

	return cost;
}

/**
 * @brief
 */
static inline float Ai_Node_DefaultCost(const ai_node_id_t a, const ai_node_id_t b) {
	const vec3_t av = Ai_Node_GetPosition(a);
	const vec3_t bv = Ai_Node_GetPosition(b);

	return Vec3_Distance(av, bv);
}

/**
 * @brief
 */
GArray *Ai_Node_FindPath(const ai_node_id_t start, const ai_node_id_t end, const Ai_NodeCost_Func heuristic, float *length);

/**
 * @brief
 */
GArray *Ai_Node_TestPath(void);
