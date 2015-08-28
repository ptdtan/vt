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

#ifndef BCF_GENOTYPING_BUFFERED_READER_H
#define BCF_GENOTYPING_BUFFERED_READER_H

#include <string>
#include "htslib/kseq.h"
#include "htslib/vcf.h"
#include "hts_utils.h"
#include <list>
#include "program.h"
#include "genotyping_record.h"
#include "bcf_ordered_reader.h"
#include "variant.h"

/**
 * Wrapper for BCFOrderedReader.
 *
 * VCF records are wrapped in GenotyingRecord and are
 * maintained in a buffer.
 *
 */
class BCFGenotypingBufferedReader
{
    public:

    ///////
    //i/o//
    ///////
    BCFOrderedReader* odr;

    //////////////////
    //buffer related//
    //////////////////
    std::list<GenotypingRecord*> buffer;
    std::string chrom;
    int32_t rid;
    int32_t start, end;

    /**
     * Constructor.
     */
    BCFGenotypingBufferedReader(std::string filename, std::vector<GenomeInterval>& intervals);

    /**
     * Collects sufficient statistics from read for variants to be genotyped.
     */
    void process_read(bam_hdr_t *h, bam1_t *s);
    
    /**
     * Collects sufficient statistics from read for variants to be genotyped.
     */
    void collect_sufficient_statistics(GenotypingRecord *g, bam1_t *s);

    /**
     * Flush records.
     */
    void flush(BCFOrderedWriter* odw, bam_hdr_t *h, bam1_t *s, bool flush_all=false);

//    /**
//     * Print out all the records.
//     */
//    void flush()
//    {
//        std::list<GenotypingRecord*>::iterator i = buffer.begin();
//        while (i!=buffer.end())
//        {
//            GenotypingRecord* vx = *i;
//
//            vx->print(odw);
//
//            vx->clear();
//            pool.push_front(vx);
//            i = buffer.erase(i);
//        }
//    }
//
//    /**
//     * Adds pileup records till the first record after epos1.
//     */
//    bool add_rec(int32_t epos1)
//    {
//        //add records only if the new record overlaps with read
//        if (buffer.size()!=0)
//        {
//            bcf1_t *v = buffer.back()->v;
//            int32_t vpos1 = bcf_get_pos1(v);
//
//            if (vpos1>epos1)
//            {
//                return false;
//            }
//        }
//
//        bool added_record = false;
//        bcf1_t *v = odw->get_bcf1_from_pool();
//        while (odr->read(v))
//        {
//            GenotypingRecord *p = NULL;
//            if (pool.size()!=0)
//            {
//                p = pool.front();
//                pool.pop_front();
//            }
//            else
//            {
//               // p = new GenotypingRecord(odr->hdr, v);
//            }
//
//            p->clear();
//
//            buffer.push_back(p);
//            added_record = true;
//
//            if ((bcf_get_pos1(p->v))>epos1)
//                break;
//        }
//
//        return added_record;
//    }
//
//    int32_t read_is_before_first_vcf_record(bam1_t *s)
//    {
//        bcf1_t *v = (*(buffer.begin()))->v;
//        int32_t vpos1 = bcf_get_pos1(v);
//        int32_t epos1 = bam_get_end_pos1(s);
//
//        return (epos1+100000)<vpos1 ? vpos1 : 0;
//    }
};

#endif