/*
 * Copyright (c) 2012-2014,2017 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Declaration of a base set associative tag store.
 */

#ifndef __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
#define __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include<string.h>

#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "params/BaseSetAssoc.hh"

namespace gem5
{

/**
 * A basic cache tag store.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 *
 * The BaseSetAssoc placement policy divides the cache into s sets of w
 * cache lines (ways).
 */
class BaseSetAssoc : public BaseTags
{
  protected:
    /** The allocatable associativity of the cache (alloc mask). */
    unsigned allocAssoc;

    /** The cache blocks. */
    std::vector<CacheBlk> blks;

    /** Whether tags and data are accessed sequentially. */
    const bool sequentialAccess;

    /** Replacement policy */
    replacement_policy::Base *replacementPolicy;

  public:
    //yongjun : fast write
    int is_invalid_victim;
    int getIsInvalid(){
        return is_invalid_victim;
    }
    //end
    /** Convenience typedef. */
     typedef BaseSetAssocParams Params;

    /**
     * Construct and initialize this tag store.
     */
    BaseSetAssoc(const Params &p);
    //yongjun : base_set_assoc params name
    std::string params_name;
    /**
     * Destructor
     */
    virtual ~BaseSetAssoc() {};

    /**
     * Initialize blocks as CacheBlk instances.
     */
    void tagsInit() override;

    /**
     * This function updates the tags when a block is invalidated. It also
     * updates the replacement data.
     *
     * @param blk The block to invalidate.
     */
    void invalidate(CacheBlk *blk) override;

    /**
     * Access block and update replacement data. May not succeed, in which case
     * nullptr is returned. This has all the implications of a cache access and
     * should only be used as such. Returns the tag lookup latency as a side
     * effect.
     *
     * @param pkt The packet holding the address to find.
     * @param lat The latency of the tag lookup.
     * @return Pointer to the cache block if found.
     */
    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat) override
    {
        CacheBlk *blk = findBlock(pkt->getAddr(), pkt->isSecure());

        // Access all tags in parallel, hence one in each way.  The data side
        // either accesses all blocks in parallel, or one block sequentially on
        // a hit.  Sequential access with a miss doesn't access data.
        stats.tagAccesses += allocAssoc;
        if (sequentialAccess) {
            if (blk != nullptr) {
                stats.dataAccesses += 1;
            }
        } else {
            stats.dataAccesses += allocAssoc;
        }

        // If a cache hit
        if (blk != nullptr) {
            // Update number of references to accessed block
            blk->increaseRefCount();

            // Update replacement data of accessed block
            replacementPolicy->touch(blk->replacementData, pkt);
        }

        // The tag lookup latency is the same for a hit or a miss
        lat = lookupLatency;

        return blk;
    }
    // yongjun : update local counter
    void updataLocalCounterToTags(Addr addr, int is_hit){
        indexingPolicy->updateLocalCounter(addr, is_hit);
    }
    // yongjun : writeback PROI, Write hit
    void writeHitL2_PROI(Addr addr, std::vector<CacheBlk*>& evict_blks, int flag)
    {
        int local_cnt_value = 0;
        int thres = 10;
        const std::vector<ReplaceableEntry*> entries =
                indexingPolicy->getPossibleEntries(addr);
        if((params_name == "system.l2.tags") && (flag==0)) {
            int is_invalid = 0;
            for (const auto &candidate: entries) {
                CacheBlk* valid_test = static_cast<CacheBlk *>(candidate);
                bool valid_bit = valid_test->isValid();
                if(!valid_bit){ // if has invalid entry
                    is_invalid = 1;
                }
            }
            //int setIdx = indexingPolicy->getSetIdx(addr);
            local_cnt_value = indexingPolicy->getLocalCounter(addr);
            if((local_cnt_value <= thres) && (!is_invalid)) {
                CacheBlk *victim_dead = static_cast<CacheBlk *>(replacementPolicy->getVictim(
                        entries));
                // IF NULL DON'T INSERT
                if (victim_dead != NULL) {
                    stats.deadblock++;
                    evict_blks.push_back(victim_dead);
                }
            }
            else if((local_cnt_value > thres) && (!is_invalid)){
                stats.Nondeadblock++;
            }
        }
        else if((params_name == "system.l2.tags") && (flag==1)) {
            int is_invalid = 0;
            for (const auto &candidate: entries) {
                CacheBlk* valid_test = static_cast<CacheBlk *>(candidate);
                bool valid_bit = valid_test->isValid();
                if(!valid_bit){ // if has invalid entry
                    is_invalid = 1;
                }
            }
            //int setIdx = indexingPolicy->getSetIdx(addr);
            local_cnt_value = indexingPolicy->getLocalCounter(addr);
            if((local_cnt_value <= thres) && (!is_invalid)) {
                CacheBlk *victim_dead = static_cast<CacheBlk *>(replacementPolicy->getVictim(
                        entries, 1));
                // IF NULL DON'T INSERT
                if (victim_dead != NULL) {
                    stats.deadblock++;
                    evict_blks.push_back(victim_dead);
                }
            }
            else if((local_cnt_value > thres) && (!is_invalid)){
                stats.Nondeadblock++;
            }
        }
    }
    //end


    /**
     * Find replacement victim based on address. The list of evicted blocks
     * only contains the victim.
     *
     * @param addr Address to find a victim for.
     * @param is_secure True if the target memory space is secure.
     * @param size Size, in bits, of new block to allocate.
     * @param evict_blks Cache blocks to be evicted.
     * @return Cache block to be replaced.
     */
     // yongjun : base.cc 1811 line findVictim
     //yongjun : findvictim, invalidate
    CacheBlk* findVictim(Addr addr, const bool is_secure,
                         const std::size_t size,
                         std::vector<CacheBlk*>& evict_blks) override
    {
        // Get possible entries to be victimized
        // yongjun : set blocks
        int local_cnt_value = 0;
        int thres = 10;
        const std::vector<ReplaceableEntry*> entries =
            indexingPolicy->getPossibleEntries(addr);

        //std::cout << "base_set_accos addr = " << addr << '\n';

        // Choose replacement victim from replacement candidates
        CacheBlk* victim = static_cast<CacheBlk*>(replacementPolicy->getVictim(
                                entries));
        if((params_name == "system.l2.tags")) {
            if (victim->isValid() == 0) {
                is_invalid_victim = 1;
            } else if (victim->isValid() == 1) {
                is_invalid_victim = 0;
            }
        }
        // There is only one eviction for this replacement
        evict_blks.push_back(victim);
        //eivct_blks + data to All 0
        //std::cout << params_name << '\n';
        //yongjun : change getVictim to dead block
        //std::tuple<void*, void*> tmp = (replacementPolicy->getVictim_deadblock(entries));
        //CacheBlk* victim = static_cast<CacheBlk*>(std::get<0>(tmp));
        //CacheBlk* victim_deadblock = static_cast<CacheBlk*>(std::get<1>(tmp));
        //yongjun : 2개 evict 필요, push.back ?

        //uint8_t tmp = 0;
        //baseline
        //begin
        if((params_name == "system.l2.tags")){
            int is_invalid = 0;
            for (const auto &candidate: entries) {
                CacheBlk* valid_test = static_cast<CacheBlk *>(candidate);
                bool valid_bit = valid_test->isValid();
                if(!valid_bit){ // if has invalid entry
                    //if(valid_test != victim) {
                    is_invalid = 1;
                    //}
                }
            }
            int setIdx = indexingPolicy->getSetIdx(addr);
            local_cnt_value = indexingPolicy->getLocalCounter(addr);
            if((local_cnt_value <= thres) && (!is_invalid)) {
                CacheBlk *victim_dead = static_cast<CacheBlk *>(replacementPolicy->getVictim(
                        entries, 1));
                // IF NULL DON'T INSERT
                //blk->isValid()

                if (victim_dead != NULL) {

                    stats.deadblock++;
                    evict_blks.push_back(victim_dead);
                    //uint8_t* data = victim_dead->data;
                    //std::cout << "data = " <<*data << '\n';
                    //for(int i =0; i < 64; i++){
                    //data[i] = tmp;
                    //}
                    //std::memset(victim_dead->data, 0, 64);
                    //std::memcpy(victim_dead->data, all_zero_p, 64);
                    //std::memcpy(p, getConstPtr<uint8_t>(), getSize());
                    //for(int i = 0 ; i< 64;i++){
                    //    victim_dead->data[i] = 0;
                    //}
                    //std::cout<<"no preset !!"<< '\n';
                }
            }
            else if((local_cnt_value > thres) && (!is_invalid)){
                stats.Nondeadblock++;
            }
        }




        //end

        //return victim;
        return victim;
    }

    /**
     * Insert the new block into the cache and update replacement data.
     *
     * @param pkt Packet holding the address to update
     * @param blk The block to update.
     */
    void insertBlock(const PacketPtr pkt, CacheBlk *blk) override
    {
        // Insert block
        BaseTags::insertBlock(pkt, blk);

        // Increment tag counter
        stats.tagsInUse++;

        // Update replacement policy
        replacementPolicy->reset(blk->replacementData, pkt);
    }

    void moveBlock(CacheBlk *src_blk, CacheBlk *dest_blk) override;

    /**
     * Limit the allocation for the cache ways.
     * @param ways The maximum number of ways available for replacement.
     */
    virtual void setWayAllocationMax(int ways) override
    {
        fatal_if(ways < 1, "Allocation limit must be greater than zero");
        allocAssoc = ways;
    }

    /**
     * Get the way allocation mask limit.
     * @return The maximum number of ways available for replacement.
     */
    virtual int getWayAllocationMax() const override
    {
        return allocAssoc;
    }

    /**
     * Regenerate the block address from the tag and indexing location.
     *
     * @param block The block.
     * @return the block address.
     */
    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return indexingPolicy->regenerateAddr(blk->getTag(), blk);
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            visitor(blk);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            if (visitor(blk)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace gem5

#endif //__MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
