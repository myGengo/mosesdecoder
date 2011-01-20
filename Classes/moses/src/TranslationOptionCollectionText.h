// $Id: TranslationOptionCollectionText.h 3394 2010-08-10 13:12:00Z bhaddow $

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#ifndef moses_TranslationOptionCollectionText_h
#define moses_TranslationOptionCollectionText_h

#include "TranslationOptionCollection.h"

namespace Moses
{

class Sentence;
class LMList;

class TranslationOptionCollectionText : public TranslationOptionCollection {
 public:
	void ProcessUnknownWord(size_t sourcePos);
	
    TranslationOptionCollectionText(const TranslationSystem* system, Sentence const& inputSentence, size_t maxNoTransOptPerCoverage, float translationOptionThreshold);
	
	bool HasXmlOptionsOverlappingRange(size_t startPosition, size_t endPosition) const;
	
	void CreateXmlOptionsForRange(size_t startPosition, size_t endPosition);
	
	
};

}

#endif
