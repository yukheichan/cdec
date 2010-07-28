#ifndef FF_LM_FSA_H
#define FF_LM_FSA_H

//FIXME: when FSA_LM_PHRASE 1, 3gram has differences in 4th decimal digit, compared to regular ff_lm.  this is USUALLY a bug (there's way more actual precision in there).  this was with #define LM_FSA_SHORTEN_CONTEXT 1 and 0 (so it's not that).  also, LM_FSA_SHORTEN_CONTEXT gives identical scores with FSA_LM_PHRASE 0

#define FSA_LM_PHRASE 0

#define FSA_LM_DEBUG 0
#if FSA_LM_DEBUG
# define FSALMDBG(e,x) FSADBGif(debug(),e,x)
# define FSALMDBGnl(e) FSADBGif_nl(debug(),e)
#else
# define FSALMDBG(e,x)
# define FSALMDBGnl(e)
#endif

#include "ff_lm.h"
#include "ff_from_fsa.h"

namespace {
WordID empty_context=TD::none;
}

struct LanguageModelFsa : public FsaFeatureFunctionBase<LanguageModelFsa> {
  typedef WordID * W;
  typedef WordID const* WP;

  // overrides; implementations in ff_lm.cc
  typedef SingleFeatureAccumulator Accum;
  static std::string usage(bool,bool);
  LanguageModelFsa(std::string const& param);
  int markov_order() const { return ctxlen_; }
  void print_state(std::ostream &,void const *) const;
  inline Featval floored(Featval p) const {
    return p<floor_?floor_:p;
  }
  static inline WordID const* left_end(WordID const* left, WordID const* e) {
    for (;e>left;--e)
      if (e[-1]!=TD::none) break;
    //post: [left,e] are the seen left words
    return e;
  }

  template <class Accum>
  void ScanAccum(SentenceMetadata const& /* smeta */,Hypergraph::Edge const& /* edge */,WordID w,void const* old_st,void *new_st,Accum *a) const {
    if (!ctxlen_) {
      Add(floored(pimpl_->WordProb(w,&empty_context)),a);
      return;
    }
    //variable length array is in C99, msvc++, if it doesn't support it, #ifdef it or use a stackalloc call (forget the name)
    if (ctxlen_) {
      WordID ctx[ngram_order_];
      state_copy(ctx,old_st);
      ctx[ctxlen_]=TD::none; // make this part of state?  wastes space but saves copies.
      Featval p=floored(pimpl_->WordProb(w,ctx));
// states are sri contexts so are in reverse order (most recent word is first, then 1-back comes next, etc.).
      WordID *nst=(WordID *)new_st;
      nst[0]=w; // new most recent word
      to_state(nst+1,ctx,ctxlen_-1); // rotate old words right
#if LM_FSA_SHORTEN_CONTEXT
      p+=pimpl_->ShortenContext(nst,ctxlen_);
#endif
      Add(p,a);
    }
  }

#if FSA_LM_PHRASE
  //FIXME: there is a bug in here somewhere, or else the 3gram LM we use gives different scores for phrases (impossible? BOW nonzero when shortening context past what LM has?)
  template <class Accum>
  void ScanPhraseAccum(SentenceMetadata const& /* smeta */,const Hypergraph::Edge&edge,WordID const* begin,WordID const* end,void const* old_st,void *new_st,Accum *a) const {
    if (begin==end) return; // otherwise w/ shortening it's possible to end up with no words at all.
    /* // this is forcing unigram prob always.  we will instead build the phrase
    if (!ctxlen_) {
      Featval p=0;
      for (;i<end;++i)
        p+=floored(pimpl_->WordProb(*i,e&mpty_context));
      Add(p,a);
      return;
      } */
    int nw=end-begin;
    WP st=(WP)old_st;
    WP st_end=st+ctxlen_; // may include some null already (or none if full)
    int nboth=nw+ctxlen_;
    WordID ctx[nboth+1];
    ctx[nboth]=TD::none;
    // reverse order - state at very end of context, then [i,end) in rev order ending at ctx[0]
    W ctx_score_end=wordcpy_reverse(ctx,begin,end);
    assert(ctx_score_end==ctx+nw);
    wordcpy(ctx_score_end,st,st_end); // st already reversed.
    // we could just copy the filled state words, but it probably doesn't save much time (and might cost some to scan to find the nones.  most contexts are full except for the shortest source spans.
//    FSALMDBG(edge," Scan("<<TD::GetString(ctx,ctx+nboth)<<')');
    Featval p=0;
    FSALMDBGnl(edge);
    for(;ctx_score_end>ctx;--ctx_score_end)
      p+=floored(pimpl_->WordProb(ctx_score_end[-1],ctx_score_end));
    //TODO: look for score discrepancy -
    // i had some idea that maybe shortencontext would return a different prob if the length provided was > ctxlen_; however, since the same 4th digit disagreement happens with LM_FSA_SHORTEN_CONTEXT 0 anyway, it's not that.  perhaps look to SCAN_PHRASE_ACCUM_OVERRIDE - make sure they do the right thing.
#if LM_FSA_SHORTEN_CONTEXT
    p+=pimpl_->ShortenContext(ctx,nboth<ctxlen_?nboth:ctxlen_);
#endif
    state_copy(new_st,ctx);
    FSALMDBG(edge," lm.Scan("<<TD::GetString(begin,end)<<"|"<<describe_state(old_st)<<")"<<"="<<p<<","<<describe_state(new_st));
    FSALMDBGnl(edge);
    Add(p,a);
  }

  SCAN_PHRASE_ACCUM_OVERRIDE
#endif
  // impl details:
  void set_ngram_order(int i); // if you build ff_from_fsa first, then increase this, you will get memory overflows.  otherwise, it's the same as a "-o i" argument to constructor
  double floor_; // log10prob minimum used (e.g. unk words)

private:
  int ngram_order_;
  int ctxlen_; // 1 less than above
  LanguageModelInterface *pimpl_;

};

typedef FeatureFunctionFromFsa<LanguageModelFsa> LanguageModelFromFsa;

#endif
