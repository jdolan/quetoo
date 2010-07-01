/**
 * @file m_nodes.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_NODES_H
#define CLIENT_MENU_M_NODES_H

/* prototype */
struct menuIcon_s;
struct menuCondition_s;
struct value_s;

/* extradata struct */
#include "node/m_node_abstractscrollbar.h"
#include "node/m_node_abstractvalue.h"
#include "node/m_node_text.h"
#include "node/m_node_textentry.h"
#include "node/m_node_window.h"

/* exclude rect */
#define MAX_EXLUDERECTS	32

typedef struct excludeRect_s {
	vec2_t pos, size;
} excludeRect_t;

/**
 * @brief menu node
 */
typedef struct menuNode_s {
	/* common identification */
	char name[MAX_VAR];			/**< name from the script files */
	struct nodeBehaviour_s *behaviour;

	struct menuNode_s *super;	/**< Node inherited, else NULL */

	/* common navigation */
	struct menuNode_s *firstChild;	/**< first element of linked list of child */
	struct menuNode_s *lastChild;	/**< last element of linked list of child */
	struct menuNode_s *next;		/**< Next element into linked list */
	struct menuNode_s *parent;		/**< Parent menu, else NULL */
	struct menuNode_s *root;		/**< Shortcut to the root node */

	/* common pos */
	vec2_t pos;
	vec2_t size;

	/* common attributes */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	int num;					/**< used to identify child into a parent; can be used for layout */
	byte textalign;
	qboolean invis;				/**< true if the node is invisible */
	qboolean disabled;			/**< true if the node is inactive */
	qboolean invalidated;		/**< true if we need to update the layout */
	qboolean ghost;				/**< true if the node is not tangible */
	char* text;
	const char* font;			/**< Font to draw text */
	const char* tooltip;		/**< holds the tooltip */
	const char *key;			/**< key bindings - used as tooltip */
	struct menuIcon_s *icon;	/**< Link to an icon */

	/** @todo use a linked list of excluderect? */
	excludeRect_t *excludeRect;	/**< exclude this for hover or click functions */
	int excludeRectNum;			/**< how many consecutive exclude rects defined? */

	struct menuCondition_s* visibilityCondition;	/**< cvar condition to display/hide the node */

	/** @todo needs cleanup */
	void* image;
	void* cvar;
	qboolean state;				/**< is node hovered */
	int mousefx;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */

	/** @todo move it into window node, or think about a node way (i.e. Doom3 onTime) */
	int timeOut;				/**< ms value until calling onTimeOut (see cl.time) */
	int lastTime;				/**< when a menu was pushed this value is set to cl.time */

	/* common color */
	vec4_t color;				/**< rgba */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */

	/* common events */
	struct menuAction_s *onClick;
	struct menuAction_s *onRightClick;
	struct menuAction_s *onMiddleClick;
	struct menuAction_s *onWheel;
	struct menuAction_s *onMouseEnter;
	struct menuAction_s *onMouseLeave;
	struct menuAction_s *onWheelUp;
	struct menuAction_s *onWheelDown;
	struct menuAction_s *onChange;	/**< called when the widget change from an user action */

	/* temporary, and/or for testing */
	float extraData1;			/**< allow behaviour to use it, how it need (before creating a real extradata structure) */

	/* image, and more */
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
	qboolean preventRatio;

	/* text */
	/** @todo remove it  from 'string node', need to full implement MN_DrawStringInBox */
	byte longlines;				/**< what to do with long lines */

	/* zone */
	qboolean repeat;			/**< repeat action when "click" is held */
	int clickDelay;				/**< for nodes that have repeat set, this is the delay for the next click */

	/** union will contain all extradata for a node */
	union {
		abstractValueExtraData_t abstractvalue;
		abstractScrollbarExtraData_t abstractscrollbar;
		textEntryExtraData_t textentry;
		textExtraData_t text;
		windowExtraData_t window;
	} u;

} menuNode_t;

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct nodeBehaviour_s {
	/* behaviour attributes */
	const char* name;				/**< name of the behaviour: string type of a node */
	const char* extends;			/**< name of the extends behaviour */
	qboolean isVirtual;				/**< true, if the node doesn't have any position on the screen */
	qboolean isFunction;			/**< true, if the node is a function */
	qboolean isAbstract;			/**< true, if we can't instantiate the behaviour */
	qboolean isInitialized;			/**< cache if we already have initialized the node behaviour */
	qboolean focusEnabled;			/**< true if the node can win the focus (should be use when type TAB) */
	const value_t* properties;		/**< list of properties of the node */
	int propertyCount;				/**< number of the properties into the propertiesList. Cache value to speedup search */
	struct nodeBehaviour_s *super;	/**< link to the extended node */

	/* draw callback */
	void (*draw)(menuNode_t *node);							/**< how to draw a node */
	void (*drawOverMenu)(menuNode_t *node);					/**< callback to draw content over the menu @sa MN_CaptureDrawOver */

	/* mouse events */
	void (*leftClick)(menuNode_t *node, int x, int y);		/**< left mouse click event in the node */
	void (*rightClick)(menuNode_t *node, int x, int y);		/**< right mouse button click event in the node */
	void (*middleClick)(menuNode_t *node, int x, int y);	/**< middle mouse button click event in the node */
	void (*mouseWheel)(menuNode_t *node, qboolean down, int x, int y);	/**< mouse wheel event in the node */
	void (*mouseMove)(menuNode_t *node, int x, int y);
	void (*mouseDown)(menuNode_t *node, int x, int y, int button);	/**< mouse button down event in the node */
	void (*mouseUp)(menuNode_t *node, int x, int y, int button);	/**< mouse button up event in the node */
	void (*capturedMouseMove)(menuNode_t *node, int x, int y);
	void (*capturedMouseLost)(menuNode_t *node);

	/* system callback */
	void (*loading)(menuNode_t *node);		/**< called before script initialization, initialized default values */
	void (*loaded)(menuNode_t *node);		/**< only called one time, when node parsing was finished */
	void (*init)(menuNode_t *node);			/**< call every time we push the parent menu */
	void (*clone)(const menuNode_t *source, menuNode_t *clone);			/**< call to initialize a cloned node */
	void (*doLayout)(menuNode_t *node);		/**< call to update node layout */

	/* focus and keyboard events */
	void (*focusGained)(menuNode_t *node);
	void (*focusLost)(menuNode_t *node);
	qboolean (*keyPressed)(menuNode_t *node, unsigned int key, unsigned short unicode);

	/* Planned */
#if 0
	/* mouse move event */
	void (*mouseEnter)(menuNode_t *node);
	void (*mouseLeave)(menuNode_t *node);
#endif
} nodeBehaviour_t;

/* module initialization */
void MN_InitNodes(void);

/* nodes */
menuNode_t* MN_AllocNode(const char* type) __attribute__ ((warn_unused_result));
menuNode_t* MN_GetNodeByPath(const char* path);
void MN_ReadNodePath(const char* path, const menuNode_t *relativeNode, menuNode_t** resultNode, const value_t **resultProperty);
struct menuNode_s *MN_GetNodeAtPosition(int x, int y);
const char* MN_GetPath(const menuNode_t* node);
menuNode_t* MN_CloneNode(const menuNode_t* node, struct menuNode_s *targetMenu, qboolean recursive);
qboolean MN_CheckVisibility(menuNode_t *node);

/* behaviours */
nodeBehaviour_t* MN_GetNodeBehaviour(const char* name);
const struct value_s *MN_GetPropertyFromBehaviour(const nodeBehaviour_t *behaviour, const char* name);

#endif
