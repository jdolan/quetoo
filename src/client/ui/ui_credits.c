/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief
 */
TwBar *Ui_Credits(void) {

	TwBar *bar = TwNewBar("Credits");


	TwAddButton(bar, "lead", NULL, NULL, " group='Project Lead' label='Jay \"jdolan\" Dolan'");

	TwAddButton(bar, "infra", NULL, NULL, " group='Infrastructure' label='Marcel \"maci\" Wysocki'");


	TwAddButton(bar, "dev0", NULL, NULL, " group='Developer' label='Jay \"jdolan\" Dolan'");
	TwAddButton(bar, "dev1", NULL, NULL, " group='Developer' label='Stijn \"Ingar\" Buys'");
	TwAddButton(bar, "dev2", NULL, NULL, " group='Developer' label='Michael \"WickedShell\" du Breuil'");
	TwAddButton(bar, "dev3", NULL, NULL, " group='Developer' label='Stephan \"stereo84\" Reiter'");
	TwAddButton(bar, "dev4", NULL, NULL, " group='Developer' label='Stanley \"gotnone\" Pinchak'");
	TwAddButton(bar, "dev5", NULL, NULL, " group='Developer' label='Dale \"supa_user\" Blount'");

	TwAddButton(bar, "model0", NULL, NULL, " group='Modeling' label='Antti \"Karvajalka\" Lahti'");
	TwAddButton(bar, "model1", NULL, NULL, " group='Modeling' label='Juha \"jhaa\" Merila'");

	TwAddButton(bar, "tex0", NULL, NULL, " group='Texture Artist' label='Georges \"TRaK\" Grondin'");
	TwAddButton(bar, "tex1", NULL, NULL, " group='Texture Artist' label='Michael \"Thorn\" Rodenhurst'");
	TwAddButton(bar, "tex2", NULL, NULL, " group='Texture Artist' label='Tom \"keres\" Havlik'");

	TwAddButton(bar, "map0", NULL, NULL, " group='Level Design' label='Tim \"spirit\" Schafer'");
	TwAddButton(bar, "map1", NULL, NULL, " group='Level Design' label='Georges \"TRaK\" Grondin'");
	TwAddButton(bar, "map2", NULL, NULL, " group='Level Design' label='Steve \"Jester\" Veihl'");
	TwAddButton(bar, "map3", NULL, NULL, " group='Level Design' label='Tom \"keres\" Havlik'");
	TwAddButton(bar, "map4", NULL, NULL, " group='Level Design' label='Pawel \"ShadoW\" Chrapka'");

	TwAddButton(bar, "sound0", NULL, NULL, " group='Sound Design' label='Roland Shaw'");

	TwAddButton(bar, "thanks0", NULL, NULL, " group='Special Thanks' label='Forest \"LordHavoc\" Hale'");
	TwAddButton(bar, "thanks1", NULL, NULL, " group='Special Thanks' label='Martin \"mattn\" Gerhardy'");
	TwAddButton(bar, "thanks2", NULL, NULL, " group='Special Thanks' label='Chris \"TheBunny\" Dillman'");
	TwAddButton(bar, "thanks3", NULL, NULL, " group='Special Thanks' label='Mads \"Madsy\" Elvheim'");

	TwDefine("Credits size='400 300' alpha=200 iconifiable=false valueswidth=100 visible=false");

	return bar;
}
