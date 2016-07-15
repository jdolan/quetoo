/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "ui_local.h"

/**
 * @brief
 */
TwBar *Ui_Credits(void) {

	TwBar *bar = TwNewBar("Credits");

	TwAddButton(bar, "lead", NULL, NULL, "group='Project Lead' label=\"Jay 'jdolan' Dolan\"");

	TwAddButton(bar, "infra", NULL, NULL, "group='Infrastructure' label=\"Marcel 'maci' Wysocki\"");

	TwAddButton(bar, "dev0", NULL, NULL, "group='Programming' label=\"Jay 'jdolan' Dolan\"");
	TwAddButton(bar, "dev2", NULL, NULL, "group='Programming' label=\"Michael 'WickedShell' du Breuil\"");
	TwAddButton(bar, "dev1", NULL, NULL, "group='Programming' label=\"Stijn 'Ingar' Buys\"");

	TwAddButton(bar, "map0", NULL, NULL, "group='Level Design' label=\"Dan 'CardO' Shannon\"");
	TwAddButton(bar, "map1", NULL, NULL, "group='Level Design' label=\"DJ 'Panjoo' Bloot\"");
	TwAddButton(bar, "map2", NULL, NULL, "group='Level Design' label=\"Georges 'TRaK' Grondin\"");
	TwAddButton(bar, "map3", NULL, NULL, "group='Level Design' label=\"Steve 'Jester' Veihl\"");
	TwAddButton(bar, "map4", NULL, NULL, "group='Level Design' label=\"Tim 'spirit' Schafer\"");
	//TwAddButton(bar, "map5", NULL, NULL, "group='Level Design' label=\"Pawel 'ShadoW' Chrapka\"");

	TwAddButton(bar, "mod0", NULL, NULL, "group='Model Design' label=\"Antti 'Karvajalka' Lahti\"");
	TwAddButton(bar, "mod1", NULL, NULL, "group='Model Design' label=\"Georges 'TRaK' Grondin\"");
	TwAddButton(bar, "mod2", NULL, NULL, "group='Model Design' label=\"Noel 'Nilium' Cower\"");

	TwAddButton(bar, "snd0", NULL, NULL, "group='Sound Design' label=\"Roland Shaw\"");

	TwAddButton(bar, "tex0", NULL, NULL, "group='Textures' label=\"Georges 'TRaK' Grondin\"");
	TwAddButton(bar, "tex1", NULL, NULL, "group='Textures' label=\"Michael 'Thorn' Rodenhurst\"");
	TwAddButton(bar, "tex2", NULL, NULL, "group='Textures' label=\"Noel 'Nilium' Cower\"");

	TwAddButton(bar, "thx0", NULL, NULL, "group='Special Thanks' label=\"Chris 'TheBunny' Dillman\"");
	TwAddButton(bar, "thx1", NULL, NULL, "group='Special Thanks' label=\"Forest 'LordHavoc' Hale\"");
	TwAddButton(bar, "thx2", NULL, NULL, "group='Special Thanks' label=\"Mads 'Madsy' Elvheim\"");
	TwAddButton(bar, "thx3", NULL, NULL, "group='Special Thanks' label=\"Martin 'mattn' Gerhardy\"");
	TwAddButton(bar, "thx4", NULL, NULL, "group='Special Thanks' label=\"Stephan 'stereo84' Reiter\"");

	TwDefine("Credits size='350 500' alpha=200 iconifiable=false valueswidth=100 visible=false");

	return bar;
}
