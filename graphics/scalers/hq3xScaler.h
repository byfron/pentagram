/*
Copyright (C) 2005 The Pentagram Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef HQ3XSCALER_H_INCLUDED
#define HQ3XSCALER_H_INCLUDED

#ifdef USE_HQ3X_SCALER

#include "Scaler.h"

namespace Pentagram {

class hq3xScaler : public Scaler
{
public:
	hq3xScaler();

	virtual const uint32	ScaleBits() const;			//< bits for supported integer scaling
	virtual const bool		ScaleArbitrary() const;		//< supports arbitrary scaling of any degree 

	virtual const char *	ScalerName() const;			//< Name Of the Scaler (1 word)
	virtual const char *	ScalerDesc() const;			//< Desciption of the Scaler
	virtual const char *	ScalerCopyright() const;	//< Scaler Copyright info
private:
	template<class uintX, class Manip, class uintS> ScalerFunc GetScaler();
};

extern const hq3xScaler hq3x_scaler;

};

#endif

#endif
