/*
 *  Copyright (C) 2005  The Pentagram Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "pent_include.h"
#include "U8SaveGump.h"

#include "RenderSurface.h"
#include "DesktopGump.h"
#include "EditWidget.h"
#include "TextWidget.h"
#include "GUIApp.h"
#include "GameData.h"
#include "Shape.h"
#include "ShapeFrame.h"
#include "FileSystem.h"
#include "Savegame.h"
#include "PagedGump.h"
#include "getObject.h"
#include "MainActor.h"

#include "IDataSource.h"
#include "ODataSource.h"

static const int entryfont = 4;

DEFINE_RUNTIME_CLASSTYPE_CODE(U8SaveGump,Gump);

U8SaveGump::U8SaveGump(bool save_, int page_)
	: Gump(0, 0, 5, 5), save(save_), page(page_)
{

}

U8SaveGump::~U8SaveGump()
{

}


// gumps: 36/0-11: number 1-12
//        46/0: "Entry"

void U8SaveGump::InitGump(Gump* newparent, bool take_focus)
{
	Gump::InitGump(newparent, take_focus);

	dims.w = 220;
	dims.h = 170;

	FrameID entry_id(GameData::GUMPS, 46, 0);
	entry_id = _TL_SHP_(entry_id);

	Shape* entryShape;
	entryShape = GameData::get_instance()->getShape(entry_id);
	ShapeFrame* sf = entryShape->getFrame(entry_id.framenum);
	int entrywidth = sf->width;
	int entryheight = sf->height;

	if (save)
		editwidgets.resize(6); // constant!

	loadDescriptions();

	for (int i = 0; i < 6; ++i) {
		int index = page * 6 + i;


		int xbase = 3;
		int yi = i;
		if (i >= 3) {
			xbase += dims.w/2 + 9;
			yi -= 3;
		}

		Gump* gump = new Gump(xbase, 3+40*yi, 1, 1);
		gump->SetShape(entry_id, true);
		gump->InitGump(this, false);

		int x = xbase+2+entrywidth;

		if (index >= 9) { // index 9 is labelled "10"
			FrameID entrynum1_id(GameData::GUMPS, 36, (index+1)/10 - 1);
			entrynum1_id = _TL_SHP_(entrynum1_id);
			entryShape = GameData::get_instance()->getShape(entrynum1_id);
			sf = entryShape->getFrame(entrynum1_id.framenum);
			x += 1 + sf->width;

			gump = new Gump(xbase+2+entrywidth, 3+40*yi, 1, 1);
			gump->SetShape(entrynum1_id, true);
			gump->InitGump(this, false);
		}

		FrameID entrynum_id(GameData::GUMPS, 36, index % 10);
		entrynum_id = _TL_SHP_(entrynum_id);

		gump = new Gump(x, 3+40*yi, 1, 1);
		gump->SetShape(entrynum_id, true);

		if (index % 10 == 9) {
			// HACK: There is no frame for '0', so we re-use part of the
			// frame for '10', cutting off the first 6 pixels.
			Pentagram::Rect dims;
			gump->GetDims(dims);
			dims.x += 6;
			gump->SetDims(dims);
		}
		gump->InitGump(this, false);

		if (index == 0) {
			// special case for 'The Beginning...' save
			Gump* widget = new TextWidget(xbase, 12+entryheight,
										  _TL_("The Beginning..."),
										  true, entryfont, 95);
			widget->InitGump(this, false);

		} else {

			if (save) {
				EditWidget* ew = new EditWidget(xbase, entryheight+4+40*yi,
												descriptions[i],
												true, entryfont,
												95, 38-entryheight, 0, true);
				ew->SetIndex(i+1);
				ew->InitGump(this, false);
				editwidgets[i] = ew;
			} else {
				// load
				Gump* widget = new TextWidget(xbase, entryheight+4+40*yi,
											  descriptions[i], true, entryfont,
											  95);
				widget->InitGump(this, false);
			}
		}

	}

	// remove focus from children (just in case)
	if (focus_child) focus_child->OnFocus(false);
	focus_child = 0;
}

void U8SaveGump::Close(bool no_del)
{
	Gump::Close(no_del);
}

void U8SaveGump::OnFocus(bool gain)
{
	if (gain)
	{
		if (save)
			GUIApp::get_instance()->setMouseCursor(GUIApp::MOUSE_QUILL);
		else
			GUIApp::get_instance()->setMouseCursor(GUIApp::MOUSE_MAGGLASS);
	}
}

Gump* U8SaveGump::OnMouseDown(int button, int mx, int my)
{
	// take all clicks
	return this;
}


void U8SaveGump::OnMouseClick(int button, int mx, int my)
{
	if (button != BUTTON_LEFT) return;

	ParentToGump(mx,my);

	int x;
	if (mx >= 3 && mx <= 100)
		x = 0;
	else if (mx >= dims.w/2 + 10)
		x = 1;
	else
		return;

	int y;
	if (my >= 3 && my <= 40)
		y = 0;
	else if (my >= 43 && my <= 80)
		y = 1;
	else if (my >= 83 && my <= 120)
		y = 2;
	else
		return;

	int i = 3*x + y;
	int index = 6*page + i + 1;

	if (save && !focus_child && editwidgets[i]) {
		editwidgets[i]->MakeFocus();
		PagedGump* p = p_dynamic_cast<PagedGump*>(parent);
		if (p) p->enableButtons(false);
	}

	if (!save) {

		// If our parent has a notifiy process, we'll put our result in it and wont actually load the game
		GumpNotifyProcess* p = parent->GetNotifyProcess();
		if (p) { 
			// Do nothing in this case
			if (index != 1 && descriptions[i].empty()) return;

			parent->SetResult(index);
			parent->Close(); // close PagedGump (and us)
			return;
		}

		loadgame(index); // 'this' will be deleted here!
	}
}

void U8SaveGump::ChildNotify(Gump *child, uint32 message)
{
	if (child->IsOfType<EditWidget>() && message == EditWidget::EDIT_ENTER)
	{
		// save
		assert(save);

		EditWidget* widget = p_dynamic_cast<EditWidget*>(child);
		assert(widget);

		std::string name = widget->getText();
		if (name.empty()) return;

		if (savegame(widget->GetIndex() + 6*page, name))
			parent->Close(); // close PagedGump (and us)

		return;
	}

	if (child->IsOfType<EditWidget>() && message == EditWidget::EDIT_ESCAPE)
	{
		// cancel edit
		assert(save);

		// remove focus
		if (focus_child) focus_child->OnFocus(false);
		focus_child = 0;

		PagedGump* p = p_dynamic_cast<PagedGump*>(parent);
		if (p) p->enableButtons(true);

		EditWidget* widget = p_dynamic_cast<EditWidget*>(child);
		assert(widget);
		widget->setText(descriptions[widget->GetIndex()-1]);
		
		return;
	}

}

bool U8SaveGump::OnKeyDown(int key, int mod)
{
	if (Gump::OnKeyDown(key, mod)) return true;

	return false;
}

std::string U8SaveGump::getFilename(int index) {
	char buf[32];
	sprintf(buf, "%02d", index);

	std::string filename = "@save/pent";
	filename += buf;
	filename += ".sav";
	return filename;
}

bool U8SaveGump::loadgame(int index)
{
	if (index == 1) {
		GUIApp::get_instance()->newGame(std::string());
		return true;
	}

	pout << "Load " << index << std::endl;

	std::string filename = getFilename(index);
	GUIApp::get_instance()->loadGame(filename);

	return true;
}

bool U8SaveGump::savegame(int index, const std::string& name)
{
	pout << "Save " << index << ": \"" << name << "\"" << std::endl;

	if (name.empty()) return false;

	std::string filename = getFilename(index);
	GUIApp::get_instance()->saveGame(filename, name, true);
	return true;
}

void U8SaveGump::loadDescriptions()
{
	descriptions.resize(6);

	for (int i = 0; i < 6; ++i) {
		int index = 6*page + i + 1;

		std::string filename = getFilename(index);
		IDataSource* ids = FileSystem::get_instance()->ReadFile(filename);
		if (!ids) continue;

		Savegame* sg = new Savegame(ids);
		uint32 version = sg->getVersion();
		descriptions[i] = "";

		// FIXME: move version checks elsewhere!!
		if (version == 0) {
			descriptions[i] = "[corrupt] ";
		} else if (version == 1) {
			descriptions[i] = "[outdated] ";
		} else if (version > Pentagram::savegame_version) {
			descriptions[i] = "[too modern] ";
		}

		descriptions[i] += sg->getDescription();
		delete sg;
	}
}

//static
Gump *U8SaveGump::showLoadSaveGump(Gump *parent, bool save)
{
	if (save) {
		// can't save if game over
		// FIXME: this check should probably be in Game or GUIApp
		MainActor* av = getMainActor();
		if (!av || (av->getActorFlags() & Actor::ACT_DEAD)) return 0;
	}

	PagedGump * gump = new PagedGump(34, -38, 3, 35);
	gump->InitGump(parent);

	U8SaveGump* s;

	for (int page = 0; page < 16; ++page) {
		s = new U8SaveGump(save, page);
		s->InitGump(gump, false);
		gump->addPage(s);
	}


	gump->setRelativePosition(CENTER);

	return gump;
}

