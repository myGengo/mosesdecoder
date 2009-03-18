/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2009 University of Edinburgh

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

#include "Derivation.h"

using namespace std;
using namespace Moses;

namespace Josiah {

  bool Derivation::PhraseAlignment::operator<(const PhraseAlignment& other) const {
  if (_sourceSegment < other._sourceSegment) return true;
  if (other._sourceSegment < _sourceSegment) return false;
  return _target < other._target;
  }

  Derivation::Derivation(const Sample& sample) {
   
    m_featureValues = sample.GetFeatureValues();
  
    const Hypothesis* currHypo = sample.GetTargetTail();
    while ((currHypo = (currHypo->GetNextHypo()))) {
      TargetPhrase targetPhrase = currHypo->GetTargetPhrase();
      m_alignments.push_back(
        PhraseAlignment(currHypo->GetCurrSourceWordsRange(), Phrase(targetPhrase)));
      for (size_t i = 0; i < targetPhrase.GetSize(); ++i) {
        m_targetWords.push_back(targetPhrase.GetWord(i).GetFactor(0)->GetString());
      }
    }
    
    const vector<float> & weights = StaticData::Instance().GetAllWeights();
    m_score = m_featureValues.InnerProduct(weights);
  }
  

  //FIXME: This may not be the most efficient way of mapping derivations, but will do for now
  bool Derivation::operator <(const Derivation & other) const {
    bool result = m_alignments < other.m_alignments;
    return result;
  }
  
  void Derivation::getTargetFactors(std::vector<const Factor*>& sentence) const {
    for (vector<PhraseAlignment>::const_iterator i = m_alignments.begin(); i != m_alignments.end(); ++i) {
      const Phrase& targetPhrase = i->_target;
      for (size_t j = 0; j < targetPhrase.GetSize(); ++j) {
        sentence.push_back(targetPhrase.GetFactor(j,0));
      }
    }
  }

  ostream& operator<<(ostream& out, const Derivation& d) {
    out << "Target: << ";
    for (size_t i = 0; i < d.m_alignments.size(); ++i) {
      out << d.m_alignments[i]._target;
      out << d.m_alignments[i]._sourceSegment << " ";
    }
    out << ">> Feature values: ";
    out << d.m_featureValues;
    out << " Score: ";
    out << d.m_score;
    return out;
  }
  
  void DerivationCollector::outputDerivationProbability(const DerivationProbability& dp, std::ostream& out) {
    out << std::setprecision(8) << dp.second << " " << dp.second*m_n <<" " << *(dp.first);
  }
  
  
  
  void DerivationCollector::collect(Sample& sample) {
    ++m_counts[Derivation(sample)];
    IFVERBOSE(1) {
      VERBOSE(1,"Collected: " << Derivation(sample) << endl);
    }
    if (m_collectDerivByTrans) {
      //derivations per translation
      Derivation d(sample);
      ostringstream os;
      const vector<string>& sentence = d.getTargetSentence();
      copy(sentence.begin(),sentence.end(),ostream_iterator<string>(os," "));
      m_derivByTrans[os.str()].insert(d);
    }
    ++m_n;
    if (m_pd > 0 && m_n > 0 && m_n%m_pd == 0) {
      vector<DerivationProbability> derivations;
      getTopN(1,derivations);
      if (derivations.size()) {
        cerr << "MaxDecode(" << m_n << "): ";
        outputDerivationProbability(derivations[0],cerr);
        cerr << endl;
      }
    }
    
    if (m_outputMaxChange) {
      vector<DerivationProbability> derivations;
      getTopN(1,derivations);
      const Derivation* newmax = derivations[0].first;
      if (m_maxDerivation == NULL || *m_maxDerivation < *newmax || *newmax < *m_maxDerivation) {
        cerr << "NewMaxDeriv(" << m_n << ")";
        outputDerivationProbability(derivations[0],cerr);
        cerr << endl;
        m_maxDerivation = newmax;
      } 
    }
    
  }
  
  void DerivationCollector::getTopN(size_t n, vector<DerivationProbability>& derivations) {
    derivations.clear();
    for (map<Derivation,size_t>::iterator i = m_counts.begin(); i != m_counts.end(); ++i) {
      float probability = (float)i->second/m_n;
      derivations.push_back(DerivationProbability(&(i->first),probability));
    }
    DerivationProbGreaterThan comparator;
    /*for (size_t i = 0; i < derivations.size(); ++i) {
      const Derivation* d = derivations[i].first;
      float probability = derivations[i].second;
      cout << *d << endl;
      cout << probability << endl;
  }*/
    sort(derivations.begin(),derivations.end(),comparator);
    while (derivations.size() > n) {
      derivations.pop_back();
    }
    //cout << derivations.size() << endl;
  }
  
  void DerivationCollector::Max(std::vector<const Factor*>& translation, size_t& count) {
    vector<DerivationProbability> derivations;
    getTopN(1,derivations);
    count = m_counts[*(derivations[0].first)];
    derivations[0].first->getTargetFactors(translation);
  }
  
  void DerivationCollector::outputDerivationsByTranslation(ostream& out) {
    out << "Derivations per translation" << endl;
    multimap<size_t,string,greater<size_t> > sortedCounts;
    for (map<string, set<Derivation> >::const_iterator i = m_derivByTrans.begin();
         i != m_derivByTrans.end(); ++i) {
           sortedCounts.insert(pair<size_t,string>(i->second.size(),i->first));
    }
    
    for (multimap<size_t,string>::const_iterator i = sortedCounts.begin(); i != sortedCounts.end(); ++i) {
      out << "COUNT: " <<  i->first << " TRANS:" << i->second << endl;
      if (i->first > 1) {
        for (set<Derivation>::const_iterator j = m_derivByTrans[i->second].begin(); 
            j != m_derivByTrans[i->second].end(); ++j) {
              out << *j << endl;
        }
      }
    }
    
  }

}//namespace