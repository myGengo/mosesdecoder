#include "Gibbler.h"

#include "Hypothesis.h"
#include "TranslationOptionCollection.h"
#include "GibblerMaxTransDecoder.h"
#include "StaticData.h"

using namespace std;

namespace Moses {

Sample::Sample(Hypothesis* target_head) : feature_values(target_head->GetScoreBreakdown()){
  
  source_size = target_head->GetWordsBitmap().GetSize();
  
  std::map<int, Hypothesis*> source_order;
  this->target_head = target_head;
  Hypothesis* next = NULL;

  for (Hypothesis* h = target_head; h; h = const_cast<Hypothesis*>(h->GetPrevHypo())) {
    size_t startPos = h->GetCurrSourceWordsRange().GetStartPos();
    SetSourceIndexedHyps(h); 
    if (h->GetPrevHypo()){
      source_order[startPos] = h;  
    }
    else {
      source_order[-1] = h;  
    }
    this->target_tail = h;
    h->m_nextHypo = next;
    next = h;
  }
  
  std::map<int, Hypothesis*>::const_iterator source_it = source_order.begin();
  Hypothesis* prev = NULL;
  this->source_tail = source_it->second;
  
  
  for (; source_it != source_order.end(); source_it++) {
    Hypothesis *h = source_it->second;  
    h->m_sourcePrevHypo = prev;
    if (prev != NULL) 
      prev->m_sourceNextHypo = h;
    this->source_head = h;
    prev = h;
  }
  
  this->source_head->m_sourceNextHypo = NULL;
  this->target_head->m_nextHypo = NULL;
  
  this->source_tail->m_sourcePrevHypo = NULL;
  this->target_tail->m_prevHypo = NULL;
}
 
Sample::~Sample() {
  RemoveAllInColl(cachedSampledHyps);
}


Hypothesis* Sample::CreateHypothesis(Hypothesis& prevTarget, const TranslationOption& option) {
  UpdateCoverageVector(prevTarget, option);
  Hypothesis* hypo = new Hypothesis(prevTarget,option);
  cachedSampledHyps.push_back(hypo);
  return hypo;
}


void Sample::GetTargetWords(vector<Word>& words) {
  const Hypothesis* currHypo = GetTargetTail(); //target tail
  
  //we're now at the dummy hypo at the start of the sentence
  while ((currHypo = (currHypo->GetNextHypo()))) {
    TargetPhrase targetPhrase = currHypo->GetTargetPhrase();
    for (size_t i = 0; i < targetPhrase.GetSize(); ++i) {
      words.push_back(targetPhrase.GetWord(i));
    }
  }
  
  IFVERBOSE(2) {
    VERBOSE(2,"Sentence: ");
    for (size_t i = 0; i < words.size(); ++i) {
      VERBOSE(2,words[i] << " ");
    }
    VERBOSE(2,endl);
  }
}

  
Hypothesis* Sample::GetHypAtSourceIndex(size_t i) {
  std::map<size_t, Hypothesis*>::iterator it = sourceIndexedHyps.find(i);
  if (it == sourceIndexedHyps.end())
    return NULL;
  return it->second;
}

void Sample::SetSourceIndexedHyps(Hypothesis* h) {
  //if (!h->m_prevHypo)
//    return;
  
  size_t startPos = h->GetCurrSourceWordsRange().GetStartPos();
  size_t endPos = h->GetCurrSourceWordsRange().GetEndPos();
  
  if (startPos + 1 == 0 ) {
    sourceIndexedHyps[startPos] = h; 
    return;
  }
    
  
  for (size_t i = startPos; i <= endPos; i++) {
    sourceIndexedHyps[i] = h; 
  } 
  
}
  
//x and y are source side positions  
void Sample::FlipNodes(const TranslationOption& leftTgtOption, const TranslationOption& rightTgtOption, Hypothesis* m_prevTgtHypo, Hypothesis* m_nextTgtHypo, const ScoreComponentCollection& deltaFV) {
  bool tgtSideContiguous = false; 
  Hypothesis *oldLeftHypo = GetHypAtSourceIndex(leftTgtOption.GetSourceWordsRange().GetStartPos());
  Hypothesis *oldRightHypo = GetHypAtSourceIndex(rightTgtOption.GetSourceWordsRange().GetStartPos());
  
  //created the new left most tgt
  Hypothesis *newLeftHypo = CreateHypothesis(*m_prevTgtHypo, leftTgtOption);
  SetSourceIndexedHyps(newLeftHypo);
  m_prevTgtHypo->m_nextHypo = newLeftHypo;
  
  //creating the one that goes to the right
  //find its target side predecessor
  Hypothesis *tgtSidePredecessor = const_cast<Hypothesis*>(oldLeftHypo->GetPrevHypo());
   //If the flip is contiguous on the target side, then the predecessor is the flipped one 
  if (tgtSidePredecessor->GetCurrSourceWordsRange() == rightTgtOption.GetSourceWordsRange()) {
   tgtSidePredecessor = newLeftHypo;
   tgtSideContiguous = true;
  }

  //update the target word ranges of the ones in between
  if (!tgtSideContiguous) {
    size_t startTgtPos = newLeftHypo->GetCurrTargetWordsRange().GetEndPos();
    for (Hypothesis *h = const_cast<Hypothesis*>(oldRightHypo->GetNextHypo()); h != oldLeftHypo ; h = const_cast<Hypothesis*>(h->GetNextHypo())) {
     WordsRange& range = h->GetCurrTargetWordsRange();
     size_t size = range.GetNumWordsCovered();
     range.SetStartPos(startTgtPos+1);
     range.SetEndPos(startTgtPos+size);
     startTgtPos += size;
    }
  }

  //now create the one that goes on the right 
  //clear the bitmap of the predecessor
  //WordsBitmap & predWordBitmap = tgtSidePredecessor->GetWordsBitmap();
  //predWordBitmap.SetValue(rightTgtOption.GetSourceWordsRange().GetStartPos(), rightTgtOption.GetSourceWordsRange().GetEndPos(), false);
  Hypothesis *newRightHypo = CreateHypothesis(*tgtSidePredecessor, rightTgtOption);
  SetSourceIndexedHyps(newRightHypo);
  newRightHypo->m_nextHypo = m_nextTgtHypo;
  if (m_nextTgtHypo)
    m_nextTgtHypo->m_prevHypo = newRightHypo;
  if(tgtSidePredecessor) {
    tgtSidePredecessor->m_nextHypo = newRightHypo;   
  }

  //update the target side sample pointers now 
  if  (tgtSideContiguous) {
    newLeftHypo->m_nextHypo = newRightHypo;
  } 
  else {
    Hypothesis *leftHypoTgtSideSuccessor = const_cast<Hypothesis*>(oldRightHypo->GetNextHypo());
    newLeftHypo->m_nextHypo = leftHypoTgtSideSuccessor;
    if (leftHypoTgtSideSuccessor) {
      leftHypoTgtSideSuccessor->m_prevHypo = newLeftHypo;
    }
  }
  
  //update the source side sample pointers now 
  Hypothesis* newLeftSourcePrevHypo = GetHypAtSourceIndex(newLeftHypo->GetCurrSourceWordsRange().GetStartPos() - 1 );
  newLeftHypo->m_sourcePrevHypo = newLeftSourcePrevHypo;
  if (newLeftSourcePrevHypo) {
    newLeftSourcePrevHypo->m_sourceNextHypo = newLeftHypo;
  }
  
  Hypothesis* newLeftSourceNextHypo = GetHypAtSourceIndex(newLeftHypo->GetCurrSourceWordsRange().GetEndPos() + 1 );
  newLeftHypo->m_sourceNextHypo = newLeftSourceNextHypo;
  if (newLeftSourceNextHypo) {
    newLeftSourceNextHypo->m_sourcePrevHypo = newLeftHypo;
  }
  
  Hypothesis* newRightSourcePrevHypo = GetHypAtSourceIndex(newRightHypo->GetCurrSourceWordsRange().GetStartPos() - 1 );
  newRightHypo->m_sourcePrevHypo = newRightSourcePrevHypo;
  if (newRightSourcePrevHypo) {
    newRightSourcePrevHypo->m_sourceNextHypo = newRightHypo;
  }
  
  Hypothesis* newRightSourceNextHypo = GetHypAtSourceIndex(newRightHypo->GetCurrSourceWordsRange().GetEndPos() + 1 );
  newRightHypo->m_sourceNextHypo = newRightSourceNextHypo;
  if (newRightSourceNextHypo) {
    newRightSourceNextHypo->m_sourcePrevHypo = newRightHypo;
  }
  
  UpdateHead(oldLeftHypo, newLeftHypo, source_head);
  UpdateHead(oldRightHypo, newRightHypo, source_head);
  
  UpdateHead(oldLeftHypo, newRightHypo, target_head);
  UpdateHead(oldRightHypo, newRightHypo, target_head);
  
  
  UpdateFeatureValues(deltaFV);
  
}
  
  
  
void Sample::UpdateAdjacentTgtWordRanges(Hypothesis *prevHyp, Hypothesis *nextTgtHyp, Hypothesis *adjTgtHyp) {
    
  const WordsRange & prevHypoTgtRange = prevHyp->GetCurrTargetWordsRange();
  WordsRange & nextTgtHypRange = nextTgtHyp->GetCurrTargetWordsRange();
  WordsRange & adjTgtHypRange = adjTgtHyp->GetCurrTargetWordsRange();
  
  nextTgtHypRange.SetStartPos(prevHypoTgtRange.GetEndPos() + 1);
  nextTgtHypRange.SetEndPos(prevHypoTgtRange.GetEndPos() +  nextTgtHyp->GetCurrTargetPhrase().GetSize());

  adjTgtHypRange.SetStartPos(nextTgtHypRange.GetEndPos() + 1);
  adjTgtHypRange.SetEndPos(nextTgtHypRange.GetEndPos() + adjTgtHyp->GetCurrTargetPhrase().GetSize());
}  
  
void Sample::ChangeTarget(const TranslationOption& option, const ScoreComponentCollection& deltaFV)  {
  size_t optionStartPos = option.GetSourceWordsRange().GetStartPos();
  Hypothesis *currHyp = GetHypAtSourceIndex(optionStartPos);
  //cout << "Start pos " << optionStartPos << " hypo " << *currHyp <<  " option: " << option <<  endl;
  Hypothesis& prevHyp = *(const_cast<Hypothesis*>(currHyp->m_prevHypo));
  Hypothesis *newHyp = CreateHypothesis(prevHyp, option);
  
  //cout << "Target head " << target_head << endl;
  CopyTgtSidePtrs(currHyp, newHyp);
  CopySrcSidePtrs(currHyp, newHyp);
  //cout << "Target head " << target_head << endl;

  //Update source side map
  SetSourceIndexedHyps(newHyp); 
  
  //Update target word ranges
  int tgtSizeChange = static_cast<int> (option.GetTargetPhrase().GetSize()) - static_cast<int> (currHyp->GetTargetPhrase().GetSize());
  if (tgtSizeChange != 0) {
    VERBOSE(2,"Updating tgt word range downstream" << endl);
    UpdateTargetWordRange(newHyp, tgtSizeChange);  
  }
  
  UpdateFeatureValues(deltaFV);
}  

void Sample::MergeTarget(const TranslationOption& option, const ScoreComponentCollection& deltaFV)  {
  size_t optionStartPos = option.GetSourceWordsRange().GetStartPos();
  size_t optionEndPos = option.GetSourceWordsRange().GetEndPos();
  
  Hypothesis *currStartHyp = GetHypAtSourceIndex(optionStartPos);
  Hypothesis *currEndHyp = GetHypAtSourceIndex(optionEndPos);
  
  assert(currStartHyp != currEndHyp);
  
  //cout << "Start pos " << optionStartPos << " hypo " << *currHyp <<  " option: " << option <<  endl;
  Hypothesis* prevHyp = NULL;
  Hypothesis* newHyp = NULL;
  
  if (currStartHyp->GetCurrTargetWordsRange() < currEndHyp->GetCurrTargetWordsRange()) {
    prevHyp = const_cast<Hypothesis*> (currStartHyp->m_prevHypo);
    newHyp = CreateHypothesis(*prevHyp, option);
    
    //Set the target ptrs
    newHyp->m_prevHypo = prevHyp;
    newHyp->m_nextHypo = currEndHyp->m_nextHypo;
    if (prevHyp) {
      prevHyp->m_nextHypo = newHyp;
    }
    Hypothesis* currHypNext = newHyp->m_nextHypo;
    if (currHypNext) {
      currHypNext->m_prevHypo = newHyp;  
    }
    UpdateHead(currStartHyp, newHyp, target_head);
  } 
  else {
    prevHyp = const_cast<Hypothesis*> (currEndHyp->m_prevHypo);
    newHyp = CreateHypothesis(*prevHyp, option);
    
    //Set the target ptrs
    newHyp->m_prevHypo = prevHyp;
    newHyp->m_nextHypo = currStartHyp->m_nextHypo;
    if (prevHyp) {
      prevHyp->m_nextHypo = newHyp;
    }
    Hypothesis* currHypNext = newHyp->m_nextHypo;
    if (currHypNext) {
      currHypNext->m_prevHypo = newHyp;  
    }
    UpdateHead(currEndHyp, newHyp, target_head);
  }
  
  //Set the source ptrs
  newHyp->m_sourcePrevHypo = currStartHyp->m_sourcePrevHypo;
  newHyp->m_sourceNextHypo = currEndHyp->m_sourceNextHypo;
  
  Hypothesis* newHypSourcePrev = newHyp->m_sourcePrevHypo;
  if (newHypSourcePrev) {
    newHypSourcePrev->m_sourceNextHypo = newHyp;
  }
  
  Hypothesis* newHypSourceNext = newHyp->m_sourceNextHypo;
  if (newHypSourceNext) {
    newHypSourceNext->m_sourcePrevHypo = newHyp; 
  }
  
  UpdateHead(currEndHyp, newHyp, source_head);
                    
  //Update source side map
  SetSourceIndexedHyps(newHyp); 
  
  //Update target word ranges
  int newTgtSize = option.GetTargetPhrase().GetSize();
  int prevTgtSize = currStartHyp->GetTargetPhrase().GetSize() + currEndHyp->GetTargetPhrase().GetSize();
  int tgtSizeChange = newTgtSize - prevTgtSize;
  if (tgtSizeChange != 0) {
    VERBOSE(2,"Updating tgt word range downstream" << endl);
    UpdateTargetWordRange(newHyp, tgtSizeChange);  
  }
    
  UpdateFeatureValues(deltaFV);
}
  
  
void Sample::SplitTarget(const TranslationOption& leftTgtOption, const TranslationOption& rightTgtOption,  const ScoreComponentCollection& deltaFV) {
    
  size_t optionStartPos = leftTgtOption.GetSourceWordsRange().GetStartPos();
  Hypothesis *currHyp = GetHypAtSourceIndex(optionStartPos);
  
  //cout << "Start pos " << optionStartPos << " hypo " << *currHyp <<  " option: " << option <<  endl;
  Hypothesis& prevHyp = *(const_cast<Hypothesis*>(currHyp->m_prevHypo));
  Hypothesis *newLeftHyp = CreateHypothesis(prevHyp, leftTgtOption);
  
  Hypothesis *newRightHyp = CreateHypothesis(*newLeftHyp, rightTgtOption);
  
  //cout << "Target head " << target_head << endl;
  
  //Update tgt ptrs
  newLeftHyp->m_prevHypo = currHyp->m_prevHypo;
  Hypothesis* currHypPrev = const_cast<Hypothesis*>(currHyp->m_prevHypo);
  if (currHypPrev) {
    currHypPrev->m_nextHypo = newLeftHyp;
  }
  newLeftHyp->m_nextHypo = newRightHyp;
  newRightHyp->m_nextHypo = currHyp->m_nextHypo;
  if (currHyp->m_nextHypo) {
    currHyp->m_nextHypo->m_prevHypo = newRightHyp;  
  }
  
  UpdateHead(currHyp, newRightHyp, target_head);
  
  
  //Update src ptrs
  if (newLeftHyp->GetCurrSourceWordsRange() < newRightHyp->GetCurrSourceWordsRange()) { //monotone
    newLeftHyp->m_sourcePrevHypo = currHyp->m_sourcePrevHypo;
    if (currHyp->m_sourcePrevHypo) {
      currHyp->m_sourcePrevHypo->m_sourceNextHypo = newLeftHyp;
    }
    newLeftHyp->m_sourceNextHypo = newRightHyp;
    
    newRightHyp->m_sourcePrevHypo = newLeftHyp;
    newRightHyp->m_sourceNextHypo = currHyp->m_sourceNextHypo;
    if (currHyp->m_sourceNextHypo) {
      currHyp->m_sourceNextHypo->m_sourcePrevHypo = newRightHyp;
    }
    
    UpdateHead(currHyp, newRightHyp, source_head);
    
  }
  else {
    newRightHyp->m_sourcePrevHypo = currHyp->m_sourcePrevHypo;
    if (currHyp->m_sourcePrevHypo) {
      currHyp->m_sourcePrevHypo->m_sourceNextHypo = newRightHyp;
    }
    newRightHyp->m_sourceNextHypo = newLeftHyp;
    
    newLeftHyp->m_sourcePrevHypo = newRightHyp;
    newLeftHyp->m_sourceNextHypo = currHyp->m_sourceNextHypo;
    if (currHyp->m_sourceNextHypo) {
      currHyp->m_sourceNextHypo->m_sourcePrevHypo = newLeftHyp;
    }
    
    UpdateHead(currHyp, newLeftHyp, source_head);
  }
  
  
  //cout << "Target head " << target_head << endl;
  
  //Update source side map
  SetSourceIndexedHyps(newLeftHyp); 
  SetSourceIndexedHyps(newRightHyp); 
  
  //Update target word ranges
  int prevTgtSize = currHyp->GetTargetPhrase().GetSize();
  int newTgtSize = newLeftHyp->GetTargetPhrase().GetSize() + newRightHyp->GetTargetPhrase().GetSize();
  int tgtSizeChange = newTgtSize - prevTgtSize;
  if (tgtSizeChange != 0) {
    VERBOSE(2,"Updating tgt word range downstream" << endl);
    UpdateTargetWordRange(newRightHyp, tgtSizeChange);  
  }
  
  UpdateFeatureValues(deltaFV);
}  
  
  
  
void Sample::CopyTgtSidePtrs(Hypothesis* currHyp, Hypothesis* newHyp){
  newHyp->m_prevHypo = currHyp->m_prevHypo;
  newHyp->m_nextHypo = currHyp->m_nextHypo;
  Hypothesis* currHypPrev = const_cast<Hypothesis*>(currHyp->m_prevHypo);
  if (currHypPrev) {
    currHypPrev->m_nextHypo = newHyp;
  }
  Hypothesis* currHypNext = currHyp->m_nextHypo;
  if (currHypNext) {
    currHypNext->m_prevHypo = newHyp;  
  }
  
  UpdateHead(currHyp, newHyp, target_head);
}

  
void Sample::CopySrcSidePtrs(Hypothesis* currHyp, Hypothesis* newHyp){
  newHyp->m_sourcePrevHypo = currHyp->m_sourcePrevHypo;
  newHyp->m_sourceNextHypo = currHyp->m_sourceNextHypo;
  Hypothesis* newHypSourcePrev = newHyp->m_sourcePrevHypo;
  if (newHypSourcePrev) {
    newHypSourcePrev->m_sourceNextHypo = newHyp;
  }
  Hypothesis* newHypSourceNext = newHyp->m_sourceNextHypo;
  if (newHypSourceNext) {
    newHypSourceNext->m_sourcePrevHypo = newHyp; 
  }

  UpdateHead(currHyp, newHyp, source_head);
}

void Sample::UpdateHead(Hypothesis* currHyp, Hypothesis* newHyp, Hypothesis *&head) {
  if (head == currHyp)
    head = newHyp;
}
  
void Sample::UpdateTargetWordRange(Hypothesis* hyp, int tgtSizeChange) {
  Hypothesis* nextHyp = const_cast<Hypothesis*>(hyp->GetNextHypo());
  if (!nextHyp)
    return;
    
  for (Hypothesis* h = nextHyp; h; h = const_cast<Hypothesis*>(h->GetNextHypo())){
    WordsRange& range = h->GetCurrTargetWordsRange();
    range.SetStartPos(range.GetStartPos()+tgtSizeChange);
    range.SetEndPos(range.GetEndPos()+tgtSizeChange);
  }
}  
  
void Sample::UpdateFeatureValues(const ScoreComponentCollection& deltaFV) {
  //cout << "Delta: " << deltaFV << endl;
  feature_values.PlusEquals(deltaFV);
  //cout << "New FV " << feature_values << endl;
}  

//update  the bitmap of the predecessor
void Sample::UpdateCoverageVector(Hypothesis& hyp, const TranslationOption& option) {
 size_t startPos = option.GetSourceWordsRange().GetStartPos();
 size_t endPos = option.GetSourceWordsRange().GetEndPos();

 WordsBitmap & wordBitmap = hyp.GetWordsBitmap();
 wordBitmap.SetValue(startPos, endPos, false);
} 
  
void Sampler::init() {
  m_collectors.push_back(new PrintSampleCollector());
  m_operators.push_back(new MergeSplitOperator());
  m_operators.push_back(new TranslationSwapOperator());
  m_operators.push_back(new FlipOperator());
  m_iterations = StaticData::Instance().GetNumSamplingIterations();
}
  
void Sampler::Run(Hypothesis* starting, const TranslationOptionCollection* options) {
  
  Sample sample(starting);
  
  for (size_t i = 0; i < m_iterations; ++i) {
    VERBOSE(1,"Gibbs sampling iteration: " << i << endl);
    for (size_t j = 0; j < m_operators.size(); ++j) {
      VERBOSE(1,"Sampling with operator " << m_operators[j]->name() << endl);
      m_operators[j]->doIteration(sample,*options);
    }
    for (size_t k = 0; k < m_collectors.size(); ++k) {
      m_collectors[k]->collect(sample);
    }
  }
}

void PrintSampleCollector::collect(Sample& sample)  {
      //cout << *(sample.GetSampleHypothesis()) /*->GetTargetPhraseStringRep()*/ << endl;
      cout << "Sampled hypothesis: \"";
      sample.GetSampleHypothesis()->ToStream(cout);
      cout << "\"" << "  " << "Feature values: " << sample.GetFeatureValues() << endl;
    }
    
    

}


