// $Id: Phrase.cpp 3620 2010-10-12 16:17:41Z hieuhoang1972 $
/***********************************************************************
 Moses - factored phrase-based, hierarchical and syntactic language decoder
 Copyright (C) 2009 Hieu Hoang
 
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
#include <iostream>
#include <cassert>
#include "../../moses/src/Util.h"
#include "Phrase.h"

using namespace std;

namespace OnDiskPt
{

Phrase::Phrase(const Phrase &copy)
:m_words(copy.GetSize())
{
	for (size_t pos = 0; pos < copy.GetSize(); ++pos)
	{
		const Word &oldWord = copy.GetWord(pos);
		Word *newWord = new Word(oldWord);
		m_words[pos] = newWord;
	}
}

Phrase::~Phrase()
{
	Moses::RemoveAllInColl(m_words);
}

void Phrase::AddWord(Word *word)
{
	m_words.push_back(word);
}

void Phrase::AddWord(Word *word, size_t pos)
{
	assert(pos < m_words.size());
	m_words.insert(m_words.begin() + pos + 1, word);
}

int Phrase::Compare(const Phrase &compare) const
{
	int ret = 0;
	for (size_t pos = 0; pos < GetSize(); ++pos)
	{
		if (pos >= compare.GetSize())
		{ // we're bigger than the other. Put 1st
			ret = -1;
			break;
		}
		
		const Word &thisWord = GetWord(pos)
							,&compareWord = compare.GetWord(pos);
		int wordRet = thisWord.Compare(compareWord);
		if (wordRet != 0)
		{ 
			ret = wordRet;
			break;
		}
	}

	if (ret == 0)
	{
		assert(compare.GetSize() >= GetSize());
		ret = (compare.GetSize() > GetSize()) ? 1 : 0;
	}
	return ret;		
}

//! transitive comparison
bool Phrase::operator<(const Phrase &compare) const
{	
	int ret = Compare(compare);
	return ret < 0;
}

bool Phrase::operator>(const Phrase &compare) const
{	
	int ret = Compare(compare);
	return ret > 0;
}

bool Phrase::operator==(const Phrase &compare) const
{	
	int ret = Compare(compare);
	return ret == 0;
}

std::ostream& operator<<(std::ostream &out, const Phrase &phrase)
{
	for (size_t pos = 0; pos < phrase.GetSize(); ++pos)
	{
		const Word &word = phrase.GetWord(pos);
		out << word << " ";
	}
	
	return out;
}
	
}
