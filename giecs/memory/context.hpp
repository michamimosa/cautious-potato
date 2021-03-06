
#pragma once

#include <cstddef>
#include <vector>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <boost/foreach.hpp>

#include <giecs/bits.hpp>
#include <giecs/memory/block.hpp>

namespace giecs
{
namespace memory
{

template <std::size_t page_size, typename align_t>
class Context
{
    public:
        struct CheckOverlapBlocks
        {
            bool operator() (BlockKey const& key1, BlockKey const& key2) const
            {
                return (key1.page_id == key2.page_id);
            }
        };

        typedef std::shared_ptr<Block<page_size, align_t> const> BlockPtr;
        typedef std::unordered_multimap<BlockKey, BlockPtr, BlockKeyHash, CheckOverlapBlocks> BlockMap;
        typedef std::unordered_map<BlockKey, BlockPtr, BlockKeyHash, CheckOverlapBlocks > MasterMap;

        Context()
        {
            this->blocks = new BlockMap();
            this->masters = new MasterMap();
        }

        ~Context()
        {
            delete this->blocks;
            delete this->masters;
        }

        void addBlock(BlockPtr const block, BlockKey const key) const
        {
            this->syncPage(key);
            auto const val = std::make_pair(key, block);
            this->blocks->insert(val);
            this->writePageFirst(val); // initialize, even if no master block is available
        }

        void addBlock(BlockPtr const block, std::vector<BlockKey> const& keys) const
        {
            for(auto const k : keys)
                this->addBlock(block, k);
        }

        std::vector< std::pair< BlockKey const, BlockPtr const> > getPageRange(BlockKey const key) const
        {
            std::vector< std::pair<BlockKey const, BlockPtr const> > vec;

            BOOST_FOREACH(auto b, this->blocks->equal_range(key))
            {
                if(key.accessor_id == b.first.accessor_id)
                {
                    vec.push_back(b);
                }
            }

            return vec;
        }

        BlockPtr getBlock(BlockKey const key) const
        {
//  auto const p = this->blocks->find(key, BlockMap::hasher(), std::equal_to<BlockKey>());
//  if(p != this->blocks->end())

            // overloading find doesn't work..
            BOOST_FOREACH(auto b, this->blocks->equal_range(key))
            {
                if(key == b.first)
                {
                    this->syncPage(b.first);
                    return b.second;
                }
            }

            return NULL;
        }

        void markPageDirty(BlockKey const key) const
        {
            BlockPtr const block = this->getBlock(key);
            this->markPageDirty(key, block);
        }

        void markPageDirty(BlockKey const key, BlockPtr const block) const
        {
            this->masters->insert(std::make_pair(key, block));
        }

    private:
        BlockMap* blocks;
        MasterMap* masters;

        // TODO: optimize
        void writePageFirst(std::pair< BlockKey const, BlockPtr const > const ref) const
        {
            std::size_t const page_id = ref.first.page_id;
            if(this->blocks->count({page_id}) > 1)
            {
                std::array<align_t, page_size> page;
                memset(&page, 0, page_size*sizeof(align_t));

                // take all different accessors.. dumb
                BOOST_FOREACH(auto it, this->blocks->equal_range({page_id}))
                {
                    if(!(it.first.accessor_id == ref.first.accessor_id))
                    {
                        ContextSync<page_size, align_t>* const msync = it.second->createSync(*this);
                        msync->read_page(page_id, page);
                        delete msync;
                    }
                }

                ContextSync<page_size, align_t>* const sync = ref.second->createSync(*this);
                sync->write_page(page_id, page);
                delete sync;
            }
        }

        void syncPage(BlockKey const req_block) const
        {
            std::size_t const page_id = req_block.page_id;
            auto const masterp = this->masters->find({page_id});
            if(masterp != this->masters->end())
            {
                this->masters->erase(masterp);
                BlockKey const masterkey = masterp->first;
//                if(! (masterkey.accessor_id == req_block.accessor_id))
                {
                    std::size_t const page_id = masterkey.page_id;
                    if(this->blocks->count({page_id}) > 1)
                    {
                        ContextSync<page_size, align_t>* const sync = masterp->second->createSync(*this);

                        auto const itp = this->blocks->equal_range({page_id});
                        std::array<align_t, page_size> page;
                        memset(&page, 0, page_size*sizeof(align_t));
                        sync->read_page(page_id, page);

                        BOOST_FOREACH(auto it, itp)
                        {
                            if(! (it.first.accessor_id == masterp->first.accessor_id))
                            {
                                ContextSync<page_size, align_t>* const sync = it.second->createSync(*this);
                                sync->write_page_block(it, page);
                                delete sync;
                            }
                        }

                        delete sync;
                    }
                }
            }
        }
};

} // namespace memory

} // namespace giecs

