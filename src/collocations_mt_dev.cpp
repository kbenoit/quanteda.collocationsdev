#include "quanteda.h"
#include <bitset>
#include <string>
using namespace quanteda;

extern "C" {
#include "loglin.h"
}

// return the matching pattern between two words at each position, 0 for matching, 1 for not matching.
// for example, for 3-gram, bit = 000, 001, 010 ... 111 eg. 0-7
int match_bit(const std::vector<unsigned int> &tokens1, 
              const std::vector<unsigned int> &tokens2){
    
    std::size_t len1 = tokens1.size();
    std::size_t len2 = tokens2.size();
    int bit = 0;
    for (std::size_t i = 0; i < len1 && i < len2; i++) {
        if (tokens1[i] == tokens2[i]) bit += std::pow(2, i); // position dependent, bit=0:(2^n-1)
    }
    return bit;
}

// unigram subtuples from B&J algorithm -- lambda1
double sigma_uni(const std::vector<double> &counts, const std::size_t ntokens){
    double s = 0.0;
    s += std::pow(ntokens - 1, 2) / counts[0];
    for (std::size_t b = 0; b < ntokens; b++) {
        s += 1.0 / counts[std::pow(2, b)];
    }
    s += 1.0 / counts[std::pow(2, ntokens) - 1];
    return std::sqrt(s);
}

double lambda_uni(const std::vector<double> &counts, const std::size_t ntokens){
    double l = 0.0;
    l += std::log(counts[0]) * (ntokens - 1); // c0
    for (std::size_t b = 0; b < ntokens; b++) {  //c(b), #(b)=1
        l -= std::log(counts[std::pow(2, b)]);
    }
    l += std::log(counts[std::pow(2, ntokens) - 1]); //c(2^n-1)
    return l;
}

// all subtuples from B&J algorithm
double sigma_all(const std::vector<double> &counts){
    const std::size_t n = counts.size();
    double s = 0.0;
    
    for (std::size_t b = 0; b < n; b++) {
        s += 1.0 / counts[b];
    }
    
    return std::sqrt(s);
}

double lambda_all(const std::vector<double> &counts, const std::size_t ntokens){
    const std::size_t n = counts.size();
    
    double l = 0.0;
    
    for (std::size_t b = 0; b < n; b++) {  //c(b), #(b)=1
        std::bitset<8> bitb(b);
        l += std::pow(-1, ntokens - bitb.count()) * std::log(counts[b]);
    }
    
    return l;
}

//calculate dice coefficients
// dice = 2*C(2^n-1)/sum(i=1:2^n-1)(#(i)*C(i)): #(i) counts number of digit'1'
double compute_dice(const std::vector<double> &counts){
    double dice = 0.0;
    const std::size_t n = counts.size();
    for (std::size_t b = 1; b < n; b++) {  //c(b), #(b)=1
        std::bitset<8> bitb(b);
        dice += bitb.count() * counts[b]; 
    }
    //Rcout<<"counts[n-1]="<<counts[n-1]<<"dice="<<dice<<std::endl;
    dice = counts[n-1]/(dice);  // smoothing has been applied when declaring counts_bit[]
    return dice;
}


int loglin_api(std::vector<double> &table, std::vector<double> &fit, const std::size_t ntokens, const int iter = 20, const double eps = 0.1){
    int nvar = ntokens;
    int ncon = ntokens;
    int ntab = table.size();
    int nmar = std::pow(2, ntokens - 1) * ntokens;
    int nlast, ifault;
    
    std::vector<int> locmar(ncon);
    std::vector<double> marg(nmar);
    std::vector<double> u(ntab);
    std::vector<double> dev(iter);
    std::vector<int> dtab(ntokens, 2);
    std::vector<int> conf(ntokens * ntokens);
    if (ntokens == 3){
        static const int arr[] = {1, 2, 0, 1, 3, 0, 2, 3, 0};
        conf.assign(arr, arr + sizeof(arr) / sizeof(int));
    } else if (ntokens == 4){
        static const int arr4[] = {1, 2, 3, 0, 1, 2, 4, 0, 1, 3, 4, 0, 2, 3, 4, 0};
        conf.assign(arr4, arr4 + sizeof(arr4) / sizeof(int));
    } else if (ntokens == 5){
        static const int arr5[] = {1, 2, 3, 4, 0, 1, 2, 3, 5, 0, 1, 3, 4, 5, 0, 2, 3, 4, 5, 0};
        conf.assign(arr5, arr5 + sizeof(arr5) / sizeof(int));
    } else {
        throw "ntokens is out of range ";   
    }
    
    
    loglin_local(nvar, &dtab[0], ncon, &conf[0], ntab,
                 &table[0], &fit[0], &locmar[0], nmar, &marg[0],
                 ntab, &u[0], eps, iter, &dev[0], &nlast, &ifault);
    
    return ifault;
    
}
//************************//
void counts(Text text,
            MapNgrams &counts_seq,
            const unsigned int &size){
    
    
    if (text.size() == 0) return; // do nothing with empty text
    text.push_back(0); // add padding to include last words
    
    std::size_t len_text = text.size();
    for (std::size_t i = 0; i <= len_text; i++) {
        //Rcout << "Size" << size << "\n";
        if (i + size < len_text) {
            //if (std::find(text.begin() + i, text.begin() + i + size, 0) == text.begin() + i + size) {
            // dev::print_ngram(text_sub);
            Text text_sub(text.begin() + i, text.begin() + i + size);
            counts_seq[text_sub]++;
            // }
        }
    }
}

struct counts_mt : public Worker{
    
    Texts texts;
    MapNgrams &counts_seq;
    const unsigned int &len;
    
    counts_mt(Texts texts_, MapNgrams &counts_seq_, const unsigned int &len_):
        texts(texts_), counts_seq(counts_seq_), len(len_){}
    
    void operator()(std::size_t begin, std::size_t end){
        for (std::size_t h = begin; h < end; h++){
            counts(texts[h], counts_seq, len);
        }
    }
};

void estimates(std::size_t i,
               VecNgrams &seqs_np,  // seqs without padding
               IntParams &cs_np,
               VecNgrams &seqs,
               IntParams &cs, 
               DoubleParams &sgma, 
               DoubleParams &lmda, 
               DoubleParams &dice,
               DoubleParams &pmi,
               DoubleParams &logratio,
               DoubleParams &chi2,
               DoubleParams &gensim,
               DoubleParams &lfmd,
               IntParams &ifault,
               const std::string &method,
               const int &count_min,
               const double nseqs,
               const double smoothing,
               StringParams &ob_n,
               StringParams &exp_n){
    
    std::size_t n = seqs_np[i].size(); //n=2:5, seqs
    if (n == 1) return; // ignore single words
    if (cs_np[i] < count_min) return;
    //output counts
    std::vector<double> counts_bit(std::pow(2, n), smoothing);// use 1/2 as smoothing
    for (std::size_t j = 0; j < seqs.size(); j++) {
        //if (i == j) continue; // do not compare with itself
        
        int bit;
        bit = match_bit(seqs_np[i], seqs[j]);
        counts_bit[bit] += cs[j];
    }
    //counts_bit[std::pow(2, n)-1]  += cs_np[i];//  c(2^n-1) += number of itself  
    
    //B-J algorithm    
    if (method == "lambda1"){
        sgma[i] = sigma_uni(counts_bit, n);
        lmda[i] = lambda_uni(counts_bit, n);
    } else if (method == "lambda" || method == "all") {
        sgma[i] = sigma_all(counts_bit);
        lmda[i] = lambda_all(counts_bit, n);
    }
    
    if (method != "lambda" || method != "lambda1"){
        // Dice coefficient
        dice[i] = n * compute_dice(counts_bit);
        
        // expected counts: used in pmi, chi-sqaure, G2, gensim, LFMD
        std::size_t csize = pow(2, n);  
        std::vector<double> ec(csize, 1.0);
        if ( n == 2){
            double row_sum = counts_bit[0] + counts_bit[1] + counts_bit[2] + counts_bit[3];
            ec[0] = (counts_bit[0] + counts_bit[1]) * (counts_bit[0] + counts_bit[2]) / row_sum;
            ec[1] = (counts_bit[0] + counts_bit[1]) * (counts_bit[1] + counts_bit[3]) / row_sum;
            ec[2] = (counts_bit[2] + counts_bit[3]) * (counts_bit[0] + counts_bit[2]) / row_sum;
            ec[3] = (counts_bit[2] + counts_bit[3]) * (counts_bit[1] + counts_bit[3]) / row_sum;
            ifault[i] = 0;
        } else {
            ifault[i] = loglin_api(counts_bit, ec, n);
        }
        
        // calculate gensim score
        // https://radimrehurek.com/gensim/models/phrases.html#gensim.models.phrases.Phrases
        // gensim = (cnt(a, b) - min_count) * N / (cnt(a) * cnt(b))
        //gensim[i] = (counts_bit[std::pow(2, n) - 1] - count_min) * nseqs/mc_product;
        
        //LFMD
        //see http://www.lrec-conf.org/proceedings/lrec2002/pdf/128.pdf for details about LFMD
        //LFMD = log2(P(w1,w2)^2/P(w1)P(w2)) + log2(P(w1,w2))
        lfmd[i] = log2( pow(counts_bit[csize - 1], 2)/ ec[csize -1] ) + log2(counts_bit[csize - 1]);
        
        //pmi
        pmi[i] = log2(counts_bit[csize - 1]/ ec[csize -1]);
        
        
        //logratio
        logratio[i] = 0.0;
        double epsilon = 0.000000001; // to offset zero cell counts
        for (std::size_t k = 0; k < csize; k++){
            logratio[i] += counts_bit[k] * log(counts_bit[k]/ec[k] + epsilon);
        }
        logratio[i] *= 2;
        
        //chi2
        chi2[i] = 0.0;
        for (std::size_t k = 0; k < csize; k++){
            chi2[i] += std::pow((counts_bit[k] - ec[k]), 2)/ec[k];
        }
        
        //output counts
        //Convert sequences from integer to character
        std::ostringstream out;
        out<<std::setprecision(1)<<std::fixed<<std::showpoint<< counts_bit[0];
        std::string this_count = out.str();
        for (std::size_t j = 1; j < csize; j++) {
            std::ostringstream out;
            out<<std::setprecision(1)<<std::fixed<<std::showpoint<< counts_bit[j];
            this_count = this_count + '_' + out.str();
        }
        ob_n[i] = this_count;
        
        std::ostringstream out_ec;
        out_ec<<std::setprecision(1)<<std::fixed<<std::showpoint<< ec[0];
        std::string this_count_ec = out_ec.str();
        for (std::size_t j = 1; j < csize; j++) {
            std::ostringstream out_ec;
            out_ec<<std::setprecision(1)<<std::fixed<<std::showpoint<< ec[j];
            this_count_ec = this_count_ec + '_' + out_ec.str();
        }
        exp_n[i] = this_count_ec;
        ///end of out
    }
}

struct estimates_mt : public Worker{
    VecNgrams &seqs_np;
    IntParams &cs_np;
    VecNgrams &seqs;
    IntParams &cs;
    DoubleParams &sgma;
    DoubleParams &lmda;
    DoubleParams &dice;
    DoubleParams &pmi;
    DoubleParams &logratio;
    DoubleParams &chi2;
    DoubleParams &gensim;
    DoubleParams &lfmd;
    IntParams &ifault;
    const std::string &method;
    const unsigned int &count_min;
    const double nseqs;
    const double smoothing;
    StringParams &ob_n;
    StringParams &exp_n;
    
    // Constructor
    estimates_mt(VecNgrams &seqs_np_, IntParams &cs_np_, VecNgrams &seqs_, IntParams &cs_, DoubleParams &ss_, DoubleParams &ls_, DoubleParams &dice_,
                 DoubleParams &pmi_, DoubleParams &logratio_, DoubleParams &chi2_, DoubleParams &gensim_, DoubleParams &lfmd_, IntParams &ifault, const std::string &method,
                 const unsigned int &count_min_, const double nseqs_, const double smoothing_, StringParams &ob_n_, StringParams &exp_n_):
        seqs_np(seqs_np_), cs_np(cs_np_), seqs(seqs_), cs(cs_), sgma(ss_), lmda(ls_), dice(dice_), pmi(pmi_), logratio(logratio_), chi2(chi2_),
        gensim(gensim_), lfmd(lfmd_), ifault(ifault), method(method), count_min(count_min_), nseqs(nseqs_), smoothing(smoothing_), ob_n(ob_n_), exp_n(exp_n_){}
    
    void operator()(std::size_t begin, std::size_t end){
        for (std::size_t i = begin; i < end; i++) {
            estimates(i, seqs_np, cs_np, seqs, cs, sgma, lmda, dice, pmi, logratio, chi2, gensim, lfmd, ifault, method, count_min, nseqs, smoothing, ob_n, exp_n);
        }
    }
};

Function warningR("warning");

/* 
 * This funciton estimate the strength of association between specified words 
 * that appear in sequences. 
 * @used sequences()
 * @param texts_ tokens object
 * @param count_min sequences appear less than this are ignored
 * @param method 
 * @param smoothing
 */

// [[Rcpp::export]]
DataFrame qatd_cpp_collocations_dev(const List &texts_,
                                    const CharacterVector &types_,
                                    const unsigned int count_min,
                                    const IntegerVector sizes_,
                                    const std::string method,
                                    const double smoothing){
    
    Texts texts = as<Texts>(texts_);
    std::vector<unsigned int> sizes = as< std::vector<unsigned int> >(sizes_);
    unsigned int len_coe = sizes.size() * types_.size();

    // Estimate significance of the sequences
    std::vector<double> sgma_all;
    sgma_all.reserve(len_coe);
    
    std::vector<double> lmda_all;
    lmda_all.reserve(len_coe);
    
    std::vector<double> dice_all;
    dice_all.reserve(len_coe);
    
    std::vector<double> pmi_all;
    pmi_all.reserve(len_coe);
    
    std::vector<double> logratio_all;
    logratio_all.reserve(len_coe);
    
    std::vector<double> chi2_all;
    chi2_all.reserve(len_coe);
    
    std::vector<double> gensim_all;
    gensim_all.reserve(len_coe);
    
    std::vector<double> lfmd_all;
    lfmd_all.reserve(len_coe);
    
    std::vector<int> cs_all;   // count of sequence
    cs_all.reserve(len_coe);
    
    std::vector<int> ns_all; //length of sequence
    ns_all.reserve(len_coe);
    
    VecNgrams seqs_all;
    seqs_all.reserve(len_coe);
    
    //output oberved and expected counting
    std::vector<std::string> ob_all;
    ob_all.reserve(len_coe);
    
    std::vector<std::string> exp_all;
    exp_all.reserve(len_coe);
    
    //warning sign
    std::vector<int> iwarning(3, 0);
    
    for(unsigned int m = 0; m < sizes.size(); m++){
        unsigned int mw_len = sizes[m];
        // Collect all sequences of specified words
        MapNgrams counts_seq;
        //dev::Timer timer;
        //dev::start_timer("Count", timer);
#if QUANTEDA_USE_TBB
        counts_mt count_mt(texts, counts_seq, mw_len);
        parallelFor(0, texts.size(), count_mt);
#else
        for (std::size_t h = 0; h < texts.size(); h++) {
            counts(texts[h], counts_seq, mw_len);
        }
#endif
        //dev::stop_timer("Count", timer);
        
        // Separate map keys and values
        std::size_t len = counts_seq.size();
        VecNgrams seqs, seqs_np;   //seqs_np sequences without padding
        IntParams cs, cs_np;    // cs: count of sequences;  
        seqs.reserve(len);
        seqs_np.reserve(len);
        cs_np.reserve(len);
        cs.reserve(len);
        
        
        
        double total_counts = 0.0;
        std::size_t len_noPadding = 0;
        for (auto it = counts_seq.begin(); it != counts_seq.end(); ++it) {
            seqs.push_back(it -> first);
            cs.push_back(it -> second);
            total_counts += it -> second;
            if (std::find(it -> first.begin() , it -> first.begin() + it -> first.size(), 0) == it -> first.begin() + it -> first.size()) 
            {
                seqs_np.push_back(it -> first);
                cs_np.push_back(it -> second);
                seqs_all.push_back(it -> first);
                cs_all.push_back(it -> second);
                ns_all.push_back(it -> first.size());
                len_noPadding ++;
            }
        }
        
        //output counts;
        StringParams ob_n(len_noPadding);
        StringParams exp_n(len_noPadding);
        
        // adjust total_counts of MW 
        total_counts += 4 * smoothing;
        
        // Estimate significance of the sequences
        DoubleParams sgma(len_noPadding);
        DoubleParams lmda(len_noPadding);
        DoubleParams dice(len_noPadding);
        DoubleParams pmi(len_noPadding);
        DoubleParams logratio(len_noPadding);
        DoubleParams chi2(len_noPadding);
        DoubleParams gensim(len_noPadding);
        DoubleParams lfmd(len_noPadding);
        IntParams ifault(len_noPadding, 0);
        //dev::start_timer("Estimate", timer);
#if QUANTEDA_USE_TBB
        estimates_mt estimate_mt(seqs_np, cs_np, seqs, cs, sgma, lmda, dice, pmi, logratio, chi2, gensim, lfmd, ifault, method, count_min, total_counts, smoothing, ob_n, exp_n);
        parallelFor(0, seqs_np.size(), estimate_mt);
#else
        for (std::size_t i = 0; i < seqs_np.size(); i++) {
            //std::vector<double> count_bit(std::pow(2, mw_len), smoothing);
            estimates(i, seqs_np, cs_np, seqs, cs, sgma, lmda, dice, pmi, logratio, chi2, gensim, lfmd, ifault, method, count_min, total_counts, smoothing, ob_n, exp_n);
        }
#endif
        //output warning message
        for (std::size_t i = 0; i < len_noPadding; i++){
            switch(ifault[i]) {
            case 1:
            case 2:
                // if (iwarning[0] == 0){
                //     Rcout << "Warning: this should not happen" << endl; 
                //     iwarning[0] = 1;
                // }
                break;
            case 3:
                if (iwarning[1] == 0){
                    warningR("Warning: ipf algorithm did not converge for at least once"); 
                    iwarning[1] = 1;
                }
                break;
            case 4:
                if (iwarning[2] == 0){
                    warningR("Warning: incorrect specification of 'table' or 'start'"); 
                    iwarning[2] = 1;
                }
                break;
            default:
                break;
            }
        }
        
        
        
        //dev::stop_timer("Estimate", timer);
        sgma_all.insert( sgma_all.end(), sgma.begin(), sgma.end() );
        lmda_all.insert( lmda_all.end(), lmda.begin(), lmda.end() );
        dice_all.insert( dice_all.end(), dice.begin(), dice.end() );
        pmi_all.insert( pmi_all.end(), pmi.begin(), pmi.end() );
        logratio_all.insert( logratio_all.end(), logratio.begin(), logratio.end() );
        chi2_all.insert( chi2_all.end(), chi2.begin(), chi2.end() );
        gensim_all.insert( gensim_all.end(), gensim.begin(), gensim.end() );
        lfmd_all.insert( lfmd_all.end(), lfmd.begin(), lfmd.end() );
        
        //output counts
        ob_all.insert( ob_all.end(), ob_n.begin(), ob_n.end() );
        exp_all.insert( exp_all.end(), exp_n.begin(), exp_n.end() );
        
        
    }
    
    // Convert sequences from integer to character
    CharacterVector seqs_(seqs_all.size());
    for (std::size_t i = 0; i < seqs_all.size(); i++) {
        seqs_[i] = join_strings(seqs_all[i], types_, " ");
    }
    
    DataFrame output_ = DataFrame::create(_["collocation"] = seqs_,
                                          _["count"] = as<IntegerVector>(wrap(cs_all)),
                                          _["length"] = as<NumericVector>(wrap(ns_all)),
                                          _["method"] = as<NumericVector>(wrap(lmda_all)),
                                          _["sigma"] = as<NumericVector>(wrap(sgma_all)),
                                          _["dice"] = as<NumericVector>(wrap(dice_all)),
                                          _["gensim"] = as<NumericVector>(wrap(gensim_all)),
                                          _["pmi"] = as<NumericVector>(wrap(pmi_all)),
                                          _["G2"] = as<NumericVector>(wrap(logratio_all)),
                                          _["chi2"] = as<NumericVector>(wrap(chi2_all)),
                                          _["LFMD"] = as<NumericVector>(wrap(lfmd_all)),
                                          _["observed_counts"] = ob_all,
                                          _["expected_counts"] = exp_all,
                                          _["stringsAsFactors"] = false);
    return output_;
}


/***R
toks <- tokens(data_corpus_inaugural)
toks <- tokens_select(toks, stopwords("english"), "remove", padding = TRUE)
#toks <- tokens_select(toks, "^([A-Z][a-z\\-]{2,})", valuetype="regex", case_insensitive = FALSE, padding = TRUE)
#types <- unique(as.character(toks))
#out2 <- qatd_cpp_sequences(toks, types, 1, 2, "lambda1",0.5)
#out3 <- qatd_cpp_sequences(toks, types, 1, 2, "lambda",0.5)
#out4 <- qatd_cpp_sequences(toks, types, 1, 3, "lambda1",0.5)
# out2$z <- out2$lambda / out2$sigma
# out2$p <- 1 - stats::pnorm(out2$z)
txt <- "A gains capital B C capital gains A B capital C capital gains tax gains tax gains B gains C capital gains tax"
toks <- tokens(txt)
types <- unique(as.character(toks))
out2 <- qatd_cpp_collocations(toks, types, 1, 3, "lambda", 0.0)
*/
