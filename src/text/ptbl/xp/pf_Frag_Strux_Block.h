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


#ifndef PF_FRAG_STRUX_BLOCK_H
#define PF_FRAG_STRUX_BLOCK_H

#include "ut_types.h"
#include "pf_Frag.h"
#include "pf_Frag_Strux.h"

// pf_Frag_Strux_Block represents structure information for a 
// block (paragraph) in the document.

class pf_Frag_Strux_Block : public pf_Frag_Strux
{
public:
	pf_Frag_Strux_Block(pt_PieceTable * pPT,
						PT_AttrPropIndex indexAP);
	virtual ~pf_Frag_Strux_Block();

	void					setPreferredSpanFmt(PT_AttrPropIndex api);
	PT_AttrPropIndex		getPreferredSpanFmt(void) const;
	
#ifdef PT_TEST
	virtual void			__dump(FILE * fp) const;
#endif

protected:
	PT_AttrPropIndex		m_preferredSpanAPI;
};

#endif /* PF_FRAG_STRUX_BLOCK_H */
