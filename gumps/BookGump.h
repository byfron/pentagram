/*
 *  Copyright (C) 2004-2005  The Pentagram Team
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

#ifndef BOOKGUMP_H
#define BOOKGUMP_H

#include "ModalGump.h"
#include "intrinsics.h"

class BookGump : public ModalGump
{
	std::string	text;
	ObjId textwidgetL;
	ObjId textwidgetR;
public:
	ENABLE_RUNTIME_CLASSTYPE();

	BookGump();
	BookGump(ObjId owner, std::string msg);
	virtual ~BookGump();

	// Go to the next page on mouse click
	virtual void OnMouseClick(int button, int mx, int my);

	// Close on double click
	virtual void OnMouseDouble(int button, int mx, int my);

	// Init the gump, call after construction
	virtual void InitGump(Gump* newparent, bool take_focus=true);

	INTRINSIC(I_readBook);

protected:
	void NextText(); 

public:
	bool loadData(IDataSource* ids, uint32 version);
protected:
	virtual void saveData(ODataSource* ods);
};

#endif
