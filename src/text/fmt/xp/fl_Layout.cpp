/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */ 



#include "ut_types.h"
#include "pt_Types.h"
#include "ut_assert.h"

#include "fl_Layout.h"
#include "pd_Document.h"


fl_Layout::fl_Layout(PTStruxType type, PL_StruxDocHandle sdh)
{
	UT_ASSERT(sdh);

	m_type = type;
	m_sdh = sdh;
	m_apIndex = 0;

	m_pDoc = NULL;		// set by child
}

fl_Layout::~fl_Layout()
{
}

PTStruxType	fl_Layout::getType(void) const
{
	return m_type;
}

PT_AttrPropIndex fl_Layout::getAttrPropIndex(void) const
{
	return m_apIndex;
}

void fl_Layout::setAttrPropIndex(PT_AttrPropIndex apIndex)
{
	m_apIndex = apIndex;
}

UT_Bool fl_Layout::getAttrProp(const PP_AttrProp ** ppAP) const
{
	return m_pDoc->getAttrProp(m_apIndex,ppAP);
}

UT_Bool fl_Layout::getSpanAttrProp(UT_uint32 offset, const PP_AttrProp ** ppAP) const
{
	return m_pDoc->getSpanAttrProp(m_sdh,offset,ppAP);
}

