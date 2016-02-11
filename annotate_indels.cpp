/* The MIT License

   Copyright (c) 2014 Adrian Tan <atks@umich.edu>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "annotate_indels.h"

namespace
{

class Igor : Program
{
    public:

    std::string version;

    ///////////
    //options//
    ///////////
    std::string input_vcf_file;
    std::string ref_fasta_file;
    std::string output_vcf_file;
    std::vector<GenomeInterval> intervals;
    std::string interval_list;
    bool debug;
    std::string method;                //methods of detection
    std::string vntr_annotation_mode;  //modes of annotation
    int32_t vntr_classification;       //vntr classification schemas
    bool override_tag;
    bool add_vntr_record;
    bool add_flank_annotation;     //add flank annotation

    //motif related
    std::string END;
    std::string MOTIF;
    std::string MLEN;
    std::string RU;
    std::string BASIS;
    std::string BLEN;

    //exact alignment related statistics
    std::string EX_REPEAT_TRACT;
    std::string EX_COMP;
    std::string EX_ENTROPY;
    std::string EX_RL;
    std::string EX_LL;
    std::string EX_RU_COUNTS;
    std::string EX_SCORE;
    std::string EX_TRF_SCORE;
    
    //fuzzy alignment related statistics
    std::string FZ_REPEAT_TRACT;
    std::string FZ_COMP;
    std::string FZ_ENTROPY;
    std::string FZ_RL;
    std::string FZ_LL;
    std::string FZ_RU_COUNTS;
    std::string FZ_SCORE;
    std::string FZ_TRF_SCORE;

    std::string FLANKSEQ;

    //////////
    //filter//
    //////////
    std::string fexp;
    Filter filter;
    bool filter_exists;

    ///////
    //i/o//
    ///////
    BCFOrderedReader *odr;
    BCFOrderedWriter *odw;

    /////////
    //stats//
    /////////
    int32_t no_indels_annotated;

    ////////////////
    //common tools//
    ////////////////
    VariantManip* vm;
    VNTRAnnotator* va;
    ReferenceSequence* rs;

    Igor(int argc, char **argv)
    {
        version = "0.5";

        //////////////////////////
        //options initialization//
        //////////////////////////
        try
        {
            std::string desc = "annotates indels with VNTR information and adds a VNTR record.";

            TCLAP::CmdLine cmd(desc, ' ', version);
            VTOutput my; cmd.setOutput(&my);
            TCLAP::ValueArg<std::string> arg_intervals("i", "i", "intervals", false, "", "str", cmd);
            TCLAP::ValueArg<std::string> arg_interval_list("I", "I", "file containing list of intervals []", false, "", "str", cmd);
            TCLAP::ValueArg<std::string> arg_output_vcf_file("o", "o", "output VCF file [-]", false, "-", "str", cmd);
            TCLAP::ValueArg<std::string> arg_ref_fasta_file("r", "r", "reference sequence fasta file []", true, "", "str", cmd);
            TCLAP::SwitchArg arg_debug("d", "d", "debug [false]", cmd, false);
            TCLAP::UnlabeledValueArg<std::string> arg_input_vcf_file("<in.vcf>", "input VCF file", true, "","file", cmd);
            TCLAP::ValueArg<std::string> arg_fexp("f", "f", "filter expression []", false, "", "str", cmd);
            TCLAP::SwitchArg arg_override_tag("x", "x", "override tags [false]", cmd, false);

            cmd.parse(argc, argv);

            input_vcf_file = arg_input_vcf_file.getValue();
            output_vcf_file = arg_output_vcf_file.getValue();
            parse_intervals(intervals, arg_interval_list.getValue(), arg_intervals.getValue());
            fexp = arg_fexp.getValue();
            debug = arg_debug.getValue();
            ref_fasta_file = arg_ref_fasta_file.getValue();
        }
        catch (TCLAP::ArgException &e)
        {
            std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
            abort();
        }
    };

    ~Igor()
    {
    };

    void initialize()
    {
        ///////////
        //options//
        ///////////
        

        /////////////////////////
        //filter initialization//
        /////////////////////////
        filter.parse(fexp.c_str(), false);
        filter_exists = fexp=="" ? false : true;

        //////////////////////
        //i/o initialization//
        //////////////////////
        odr = new BCFOrderedReader(input_vcf_file, intervals);
        odw = new BCFOrderedWriter(output_vcf_file, 10000);
        odw->link_hdr(odr->hdr);

        //////////////////////////////
        //INFO header adding for VCF//
        //////////////////////////////
        bool rename = false;

        //motif related
        END = bcf_hdr_append_info_with_backup_naming(odw->hdr, "END", "1", "Integer", "End position of the variant", rename);
        MOTIF = bcf_hdr_append_info_with_backup_naming(odw->hdr, "MOTIF", "1", "String", "Canonical motif in an VNTR.", rename);
        RU = bcf_hdr_append_info_with_backup_naming(odw->hdr, "RU", "1", "String", "Repeat unit in the reference sequence.", rename);
        BASIS = bcf_hdr_append_info_with_backup_naming(odw->hdr, "BASIS", "1", "String", "Basis nucleotides in the motif.", rename);
        MLEN = bcf_hdr_append_info_with_backup_naming(odw->hdr, "MLEN", "1", "Integer", "Motif length.", rename);
        BLEN = bcf_hdr_append_info_with_backup_naming(odw->hdr, "BLEN", "1", "Integer", "Basis length.", rename);

        //exact alignment related statisitcs
        EX_REPEAT_TRACT = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_REPEAT_TRACT", "2", "Integer", "Left and right flank positions of the Indel, left/right alignment invariant, not necessarily equal to POS.", rename);
        EX_COMP = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_COMP", "4", "Integer", "Composition(%) of bases in repeat tract", rename);
        EX_ENTROPY = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_ENTROPY", "1", "Float", "Entropy measure of repeat tract", rename);
        EX_RL = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_RL", "1", "Integer", "Reference repeat tract length in bases from exact alignment", rename);
        EX_LL = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_LL", "1", "Integer", "Longest repeat tract length in bases from exact alignment", rename);
        EX_RU_COUNTS = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_RU_COUNTS", "2", "Integer", "Number of exact repeat units and total number of repeat units.", rename);
        EX_SCORE = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_SCORE", "1", "Float", "Concordance of repeat unit.", rename);
        EX_TRF_SCORE = bcf_hdr_append_info_with_backup_naming(odw->hdr, "EX_TRF_SCORE", "1", "Float", "TRF Score for M/I/D as 2/-7/-7", rename);
        
        //fuzzy alignment related statisitcs
        FZ_REPEAT_TRACT = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_REPEAT_TRACT", "2", "Integer", "Left and right flank positions of the Indel, left/right alignment invariant, not necessarily equal to POS.", rename);
        FZ_COMP = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_COMP", "4", "Integer", "Composition(%) of bases in repeat tract", rename);
        FZ_ENTROPY = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_ENTROPY", "1", "Float", "Entropy measure of repeat tract", rename);
        FZ_TRF_SCORE = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_TRF_SCORE", "1", "Float", "TRF Score for M/I/D as 2/-7/-7", rename);
        FZ_RL = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_RL", "1", "Integer", "Reference repeat tract length in bases from exact alignment", rename);
        FZ_LL = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_LL", "1", "Integer", "Longest repeat tract length in bases from exact alignment", rename);
        FZ_RU_COUNTS = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_RU_COUNTS", "2", "Integer", "Number of exact repeat units and total number of repeat units.", rename);
        FZ_SCORE = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_SCORE", "1", "Float", "Concordance of repeat unit.", rename);
        FZ_TRF_SCORE = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FZ_TRF_SCORE", "1", "Float", "Concordance of repeat unit.", rename);

        FLANKSEQ = bcf_hdr_append_info_with_backup_naming(odw->hdr, "FLANKSEQ", "1", "String", "Flanking sequence 10bp on either side of REF.", rename);

              ////////////////////////
        //stats initialization//
        ////////////////////////
        no_indels_annotated = 0;

        ////////////////////////
        //tools initialization//
        ////////////////////////
        vm = new VariantManip(ref_fasta_file);
        va = new VNTRAnnotator(ref_fasta_file, debug);
        rs = new ReferenceSequence(ref_fasta_file);
    }

    void print_options()
    {
        std::clog << "annotate_indels v" << version << "\n";
        std::clog << "\n";
        std::clog << "options:     input VCF file(s)        " << input_vcf_file << "\n";
        std::clog << "         [o] output VCF file          " << output_vcf_file << "\n";
        std::clog << "         [v] add VNTR records         " << (add_vntr_record ? "true" : "false") << "\n";
        std::clog << "         [k] add_flank_annotation     " << (add_flank_annotation ? "true" : "false") << "\n";
        print_boo_op("         [d] debug                    ", debug);
        print_ref_op("         [r] ref FASTA file           ", ref_fasta_file);
        print_boo_op("         [x] override tag             ", override_tag);
        print_int_op("         [i] intervals                ", intervals);
        std::clog << "\n";
    }

    void print_stats()
    {
        std::clog << "\n";
        std::cerr << "stats: no. of indels annotated   " << no_indels_annotated << "\n";
        std::clog << "\n";
    }

    /**
     * Updates the FLANKSEQ INFO field.
     */
    void update_flankseq(bcf_hdr_t* h, bcf1_t *v, const char* chrom, int32_t lflank_beg1, int32_t lflank_end1, int32_t rflank_beg1, int32_t rflank_end1)
    {
        std::string flanks;
        char* seq = rs->fetch_seq(chrom, lflank_beg1, lflank_end1);
        flanks.assign(seq);
        if (seq) free(seq);
        flanks.append(1, '[');
        seq = rs->fetch_seq(chrom, lflank_end1+1, rflank_beg1-1);
        flanks.append(seq);
        if (seq) free(seq);
        flanks.append(1, ']');
        seq = rs->fetch_seq(chrom, rflank_beg1, rflank_end1);
        flanks.append(seq);
        if (seq) free(seq);
        bcf_update_info_string(h, v, "FLANKSEQ", flanks.c_str());
    }

    void annotate_indels()
    {
        odw->write_hdr();

        bcf1_t *v = odw->get_bcf1_from_pool();
        bcf_hdr_t *h = odw->hdr;
        Variant variant;
        kstring_t old_alleles = {0,0,0};

        int32_t no_exact = 0;
        int32_t no_inexact = 0;

        while (odr->read(v))
        {
            int32_t vtype = vm->classify_variant(odr->hdr, v, variant);

            if (filter_exists)
            {
                if (!filter.apply(h, v, &variant, false))
                {
                    continue;
                }
            }

            //require normalization
            if (!vm->is_normalized(v))
            {
                std::string var = variant.get_variant_string();
                fprintf(stderr, "[%s:%d %s] Variant is not normalized, annotate_indels requires that variants are normalized: %s\n", __FILE__, __LINE__, __FUNCTION__, var.c_str());
                odw->write(v);
                v = odw->get_bcf1_from_pool();
                continue;
            }
            
            //variants with N crashes the alignment models!!!!!!  :(
            if (vm->contains_N(v))
            {
                std::string var = variant.get_variant_string();
                fprintf(stderr, "[%s:%d %s] Variant contains N bases, skipping annotation: %s\n", __FILE__, __LINE__, __FUNCTION__, var.c_str());
                odw->write(v);
                v = odw->get_bcf1_from_pool();
                continue;
            }

            if (debug)
            {
                bcf_print_liten(h,v);
            }

            if (vtype&VT_INDEL)
            {
                va->annotate(variant, method);

                VNTR& vntr = variant.vntr;

                //shared fields
                bcf_set_rid(v, variant.rid);
                
                bcf_update_info_int32(h, v, END.c_str(), &variant.end1, 1);
                bcf_update_info_string(h, v, MOTIF.c_str(), vntr.motif.c_str());
                bcf_update_info_int32(h, v, MLEN.c_str(), &vntr.mlen, 1);
                bcf_update_info_string(h, v, RU.c_str(), vntr.ru.c_str());
                bcf_update_info_string(h, v, BASIS.c_str(), vntr.basis.c_str());
                bcf_update_info_int32(h, v, BLEN.c_str(), &vntr.blen, 1);
        
                //exact characteristics
                int32_t exact_flank_pos1[2] = {vntr.exact_beg1, vntr.exact_end1};
                bcf_update_info_int32(h, v, EX_REPEAT_TRACT.c_str(), &exact_flank_pos1, 2);
                bcf_update_info_int32(h, v, EX_COMP.c_str(), &vntr.exact_comp[0], 4);
                bcf_update_info_float(h, v, EX_ENTROPY.c_str(), &vntr.exact_entropy, 1);
                bcf_update_info_int32(h, v, EX_RL.c_str(), &vntr.exact_rl, 1);
                bcf_update_info_int32(h, v, EX_LL.c_str(), &vntr.exact_ll, 1);
                int32_t exact_ru_count[2] = {vntr.exact_no_exact_ru, vntr.exact_total_no_ru};
                bcf_update_info_int32(h, v, EX_RU_COUNTS.c_str(), &exact_ru_count, 2);
                bcf_update_info_float(h, v, EX_SCORE.c_str(), &vntr.exact_score, 1);
                bcf_update_info_int32(h, v, EX_TRF_SCORE.c_str(), &vntr.exact_trf_score, 1);
               
                //fuzzy characteristics
                int32_t fuzzy_flank_pos1[2] = {vntr.fuzzy_beg1, vntr.fuzzy_end1};
                bcf_update_info_int32(h, v, FZ_REPEAT_TRACT.c_str(), &fuzzy_flank_pos1, 2);
                bcf_update_info_int32(h, v, FZ_COMP.c_str(), &vntr.fuzzy_comp[0], 4);
                bcf_update_info_float(h, v, FZ_ENTROPY.c_str(), &vntr.fuzzy_entropy, 1);
                bcf_update_info_int32(h, v, FZ_RL.c_str(), &vntr.fuzzy_rl, 1);
                bcf_update_info_int32(h, v, FZ_LL.c_str(), &vntr.fuzzy_ll, 1);
                int32_t fuzzy_ru_count[2] = {vntr.fuzzy_no_exact_ru, vntr.fuzzy_total_no_ru};
                bcf_update_info_int32(h, v, FZ_RU_COUNTS.c_str(), &fuzzy_ru_count, 2);
                bcf_update_info_float(h, v, FZ_SCORE.c_str(), &vntr.fuzzy_score, 1);
                bcf_update_info_int32(h, v, FZ_TRF_SCORE.c_str(), &vntr.fuzzy_trf_score, 1);
             
                update_flankseq(h, v, variant.chrom.c_str(),
                                variant.beg1-10, variant.beg1-1,
                                variant.end1+1, variant.end1+10);
                                
                ++no_indels_annotated;
            }
            else if (vtype==VT_VNTR)
            {
                va->annotate(variant, vntr_annotation_mode);
              
                bcf_update_info_float(h, v, "SCORE", &variant.vntr.exact_score, 1);
                bcf_update_info_float(h, v, "TRF_SCORE", &variant.vntr.exact_score, 1);

                update_flankseq(h, v, variant.chrom.c_str(),
                                variant.beg1-10, variant.beg1-1,
                                variant.end1+1, variant.end1+10);
            }
            else if (vtype==VT_SNP || vtype==VT_MNP)
            {
                update_flankseq(h, v, variant.chrom.c_str(),
                                variant.beg1-10, variant.beg1-1,
                                variant.end1+1, variant.end1+10);
            }
            else //SVs?
            {
                //do nothing
            }
            
            odw->write(v);
            v = odw->get_bcf1_from_pool();
        }

        odw->close();
        odr->close();
    };

    private:
};
}

void annotate_indels(int argc, char ** argv)
{
    Igor igor(argc, argv);
    igor.print_options();
    igor.initialize();
    igor.annotate_indels();
    igor.print_stats();
};
