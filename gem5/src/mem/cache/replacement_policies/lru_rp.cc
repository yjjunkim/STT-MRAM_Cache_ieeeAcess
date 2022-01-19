/**
 * Copyright (c) 2018-2020 Inria
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

#include "mem/cache/replacement_policies/lru_rp.hh"

#include <cassert>
#include <memory>

#include "params/LRURP.hh"
#include "sim/cur_tick.hh"
namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

LRU::LRU(const Params &p)
  : Base(p)
{
    params_name = p.name;
}
//yongjun : invalidate
void
LRU::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    // Reset last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
LRU::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Update last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
LRU::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}
ReplaceableEntry*
LRU::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    int is_change = 0;
    ReplaceableEntry* victim_prev;
    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        is_change = 0;
        // Update victim entry if necessary
        if (std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick <
                std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick) {
            victim = candidate;
        }
    }
    return victim;
}
//yongjun LRU victim : get_victim from base_set_assoc.hh
ReplaceableEntry*
LRU::getVictim(const ReplacementCandidates& candidates, int flag) const {
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    // yongjun : IF NOT L2 Cache Return NULL
    if (params_name != "system.l2.replacement_policy") return NULL;

    int is_change = 0;
    ReplaceableEntry *victim_dead = NULL;
    ReplaceableEntry *victim_prev = NULL;
    // Visit all candidates to find victim
    ReplaceableEntry *victim = candidates[0];
    for (const auto &candidate: candidates) {
        is_change = 0;
        //CacheBlk* valid_test = candidate;
        //bool valid_bit = valid_test->isValid();
        // Update victim entry if necessary
        //std::cout<< params_name << "\n";
        //if (params_name == "system.l2.replacement_policy") i++;
        //std::cout << "cadidate lastTouch : "
        //          << std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick << "\n";
        //yongjun : if same touch change victim
        if (std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick <
            std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick) {
            // yongjun : set flag and save prev_victim
            is_change = 1;
            victim_prev = victim;

            victim = candidate;
        }
        if (is_change) { //
            victim_dead = victim_prev;
        }
    }
    /*
    std::cout << "victim lastTouch : "
              << std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick << "\n";
    std::cout << "dead block lastTouch : "
              << std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick << "\n";
    std::cout<<"=== end === " <<'\n';
     */

    //if (params_name == "system.l2.replacement_policy") std::cout << "candidate cnt : " << i << "\n";
    return victim_dead;
}

/*
std::tuple<ReplaceableEntry*, ReplaceableEntry*>
LRU::getVictim_deadblock(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    int is_change = 0;
    ReplaceableEntry* victim_dead = NULL;
    ReplaceableEntry* victim_prev;
    //ReplaceableEntry* victim_list[10];
    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        is_change = 0;
        // Update victim entry if necessary
        //std::cout<< params_name << "\n";
        //if (params_name == "system.l2.replacement_policy") i++;

        std::cout << "cadidate lastTouch : "<<std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick << "\n";
        std::cout << "victim lastTouch : "  <<std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick << "\n";
        if (std::static_pointer_cast<LRUReplData>(candidate->replacementData)->lastTouchTick <
            std::static_pointer_cast<LRUReplData>(victim->replacementData)->lastTouchTick) {
            // yongjun : set flag and save prev_victim
            is_change = 1;
            victim_prev = victim;

            victim = candidate;
        }
        if(is_change){ //
            victim_dead = victim_prev;
        }

    }
    //victim_list[0] = victim;
    //victim_list[1] = victim_dead;
    //if (params_name == "system.l2.replacement_policy") std::cout << "candidate cnt : " << i << "\n";
    //return victim;
    return {victim,victim_dead};
}
*/

std::shared_ptr<ReplacementData>
LRU::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new LRUReplData());
}

} // namespace replacement_policy
} // namespace gem5
