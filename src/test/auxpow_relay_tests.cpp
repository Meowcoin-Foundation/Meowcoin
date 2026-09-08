// Copyright (c) 2026 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <auxpow.h>
#include <chain.h>
#include <consensus/merkle.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <core_memusage.h>
#include <interfaces/mining.h>
#include <headerssync.h>
#include <net.h>
#include <net_processing.h>
#include <node/blockstorage.h>
#include <node/protocol_version.h>
#include <rpc/auxpow_miner.h>
#include <streams.h>
#include <test/util/net.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>
#include <functional>
#include <thread>

namespace {
class StubBlockTemplate final : public interfaces::BlockTemplate {
    CBlock m_block;
public:
    explicit StubBlockTemplate(CBlock block) : m_block(std::move(block)) {}
    CBlockHeader getBlockHeader() override { return m_block.GetBlockHeader(); }
    CBlock getBlock() override { return m_block; }
    std::vector<CAmount> getTxFees() override { return {}; }
    std::vector<int64_t> getTxSigops() override { return {}; }
    CTransactionRef getCoinbaseTx() override { return m_block.vtx.at(0); }
    std::vector<unsigned char> getCoinbaseCommitment() override { return {}; }
    int getWitnessCommitmentIndex() override { return -1; }
    std::vector<uint256> getCoinbaseMerklePath() override { return {}; }
    bool submitSolution(uint32_t, uint32_t, uint32_t, CTransactionRef) override { return false; }
    std::unique_ptr<interfaces::BlockTemplate> waitNext(node::BlockWaitOptions) override { return {}; }
    void interruptWait() override {}
};

// Exercise the actual TemplateCache with a controllable Mining interface.
// The callback models another thread advancing the tip after the assembler
// released cs_main and before TemplateCache publishes its candidate.
class StubMining final : public interfaces::Mining {
    ChainstateManager& m_chainman;
public:
    int builds{0};
    size_t body_padding{0};
    std::function<void()> after_assembly;
    explicit StubMining(ChainstateManager& chainman) : m_chainman(chainman) {}
    bool isTestChain() override { return true; }
    bool isInitialBlockDownload() override { return false; }
    std::optional<interfaces::BlockRef> getTip() override { return {}; }
    std::optional<interfaces::BlockRef> waitTipChanged(uint256, MillisecondsDouble) override { return {}; }
    bool checkBlock(const CBlock&, const node::BlockCheckOptions&, std::string&, std::string&) override { return false; }
    std::unique_ptr<interfaces::BlockTemplate> createNewBlock(const node::BlockCreateOptions& opts) override {
        CBlock block{m_chainman.GetParams().GenesisBlock()};
        {
            LOCK(cs_main);
            block.hashPrevBlock = m_chainman.ActiveTip()->GetBlockHash();
            block.nHeight = m_chainman.ActiveTip()->nHeight + 1;
        }
        block.nTime += ++builds;
        CMutableTransaction cb{*block.vtx.at(0)};
        cb.vout.at(0).scriptPubKey = opts.coinbase_output_script;
        if (body_padding) cb.vout.emplace_back(0, CScript{} << std::vector<unsigned char>(body_padding, 0));
        block.vtx[0] = MakeTransactionRef(cb);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        if (after_assembly) after_assembly();
        return std::make_unique<StubBlockTemplate>(std::move(block));
    }
};
}

BOOST_FIXTURE_TEST_SUITE(auxpow_relay_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(stable_cache_is_per_script_and_debounces_mempool)
{
    auto& chainman = *m_node.chainman;
    StubMining miner{chainman};
    auxpow_miner::TemplateCache cache;
    const CScript a{CScript{} << OP_TRUE};
    const CScript b{CScript{} << OP_FALSE};
    const auto ha = cache.createBlock(a, miner, chainman, *m_node.mempool);
    const auto hb = cache.createBlock(b, miner, chainman, *m_node.mempool);
    BOOST_CHECK(ha != hb);
    m_node.mempool->AddTransactionsUpdated(1);
    for (int i = 0; i < 5; ++i) {
        BOOST_CHECK(cache.createBlock(a, miner, chainman, *m_node.mempool) == ha);
        BOOST_CHECK(cache.createBlock(b, miner, chainman, *m_node.mempool) == hb);
    }
    BOOST_CHECK_EQUAL(miner.builds, 2);
}

BOOST_AUTO_TEST_CASE(tip_change_during_assembly_must_not_poison_cache)
{
    auto& chainman = *m_node.chainman;
    StubMining miner{chainman};
    auxpow_miner::TemplateCache cache;
    const CScript script{CScript{} << OP_TRUE};
    CBlockIndex* original = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
    CBlockHeader next_header{original->GetBlockHeader()};
    next_header.hashPrevBlock = original->GetBlockHash();
    ++next_header.nTime;
    const uint256 next_hash{next_header.GetHash()};
    CBlockIndex next{next_header};
    next.phashBlock = &next_hash;
    next.pprev = original;
    next.nHeight = original->nHeight + 1;
    struct RestoreTip {
        ChainstateManager& chainman;
        CBlockIndex* tip;
        ~RestoreTip() { LOCK(cs_main); chainman.ActiveChain().SetTip(*tip); }
    } restore{chainman, original};
    miner.after_assembly = [&] { LOCK(cs_main); chainman.ActiveChain().SetTip(next); };
    const auto old_job = cache.createBlock(script, miner, chainman, *m_node.mempool);
    miner.after_assembly = {};
    const auto polled_job = cache.createBlock(script, miner, chainman, *m_node.mempool);
    const auto polled = cache.getBlock(polled_job);
    BOOST_REQUIRE(polled);
    BOOST_TEST_MESSAGE("builds=" << miner.builds << " old_job_reused=" << (old_job == polled_job));
    BOOST_CHECK_MESSAGE(polled->hashPrevBlock == next_hash, "Poll after tip advance returned a candidate for the previous tip");
    BOOST_CHECK_EQUAL(miner.builds, 2);
}

BOOST_AUTO_TEST_CASE(template_count_and_memory_are_bounded)
{
    StubMining miner{*m_node.chainman};
    auxpow_miner::TemplateCache cache;
    std::vector<uint256> hashes;
    for (int i = 0; i < 100; ++i) {
        hashes.push_back(cache.createBlock(CScript{} << i, miner, *m_node.chainman, *m_node.mempool));
    }
    int retained{0};
    for (const auto& hash : hashes) retained += bool(cache.getBlock(hash));
    BOOST_CHECK_EQUAL(retained, 64);
    BOOST_CHECK(!cache.getBlock(hashes.front()));
    BOOST_CHECK(cache.getBlock(hashes.back()));

    miner.body_padding = 1500000;
    hashes.clear();
    for (int i = 100; i < 200; ++i) {
        hashes.push_back(cache.createBlock(CScript{} << i, miner, *m_node.chainman, *m_node.mempool));
    }
    size_t retained_bytes{0};
    for (const auto& hash : hashes) {
        if (const auto block = cache.getBlock(hash)) retained_bytes += sizeof(CBlock) + RecursiveDynamicUsage(*block);
    }
    BOOST_CHECK(retained_bytes <= 64 * 1024 * 1024);
    BOOST_CHECK(!cache.getBlock(hashes.front()));
    BOOST_CHECK(cache.getBlock(hashes.back()));
    BOOST_TEST_MESSAGE("mining cache retained bytes=" << retained_bytes);
}

BOOST_AUTO_TEST_CASE(late_builder_preserves_newer_published_job)
{
    auto& chainman = *m_node.chainman;
    StubMining slow{chainman}, fast{chainman};
    auxpow_miner::TemplateCache cache;
    CBlockIndex* original = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
    CBlockHeader next_header{original->GetBlockHeader()};
    next_header.hashPrevBlock = original->GetBlockHash();
    ++next_header.nTime;
    CBlockIndex* next = WITH_LOCK(cs_main, return chainman.m_blockman.AddToBlockIndex(next_header, chainman.m_best_header));
    struct RestoreTip {
        ChainstateManager& chainman;
        CBlockIndex* tip;
        ~RestoreTip() { LOCK(cs_main); chainman.ActiveChain().SetTip(*tip); }
    } restore{chainman, original};
    uint256 fresh;
    const CScript script{CScript{} << OP_TRUE};
    slow.after_assembly = [&] {
        { LOCK(cs_main); chainman.ActiveChain().SetTip(*next); }
        fresh = cache.createBlock(script, fast, chainman, *m_node.mempool);
    };
    const auto stale = cache.createBlock(script, slow, chainman, *m_node.mempool);
    BOOST_REQUIRE(cache.getBlock(stale));
    BOOST_REQUIRE(cache.getBlock(fresh));
    BOOST_CHECK(cache.getBlock(stale)->hashPrevBlock == original->GetBlockHash());
    BOOST_CHECK(cache.getBlock(fresh)->hashPrevBlock == next->GetBlockHash());
    BOOST_CHECK(cache.createBlock(script, fast, chainman, *m_node.mempool) == fresh);
    BOOST_CHECK_EQUAL(fast.builds, 1);
}

BOOST_AUTO_TEST_CASE(header_prefix_reader_checks_record_and_index)
{
    auto& blockman = m_node.chainman->m_blockman;
    CBlock block{m_node.chainman->GetParams().GenesisBlock()};
    ++block.nTime;
    CAuxPow::initAuxPow(block);
    const auto hash = block.GetHash();
    CBlockIndex index{block};
    index.phashBlock = &hash;
    const auto pos = blockman.WriteBlock(block, 1);
    index.nFile = pos.nFile;
    index.nDataPos = pos.nPos;
    index.nStatus |= BLOCK_HAVE_DATA;
    CBlockHeader header;
    // These misses must not populate the cache with incorrect data.
    ++index.nNonce;
    BOOST_CHECK(!blockman.ReadBlockHeader(header, index));
    --index.nNonce;
    {
        auto file = blockman.OpenBlockFile({pos.nFile, pos.nPos - 4}, false);
        file << uint32_t{80};
    }
    BOOST_CHECK(!blockman.ReadBlockHeader(header, index));
    {
        auto file = blockman.OpenBlockFile({pos.nFile, pos.nPos - 4}, false);
        file << uint32_t(GetSerializeSize(TX_WITH_WITNESS(block)));
    }
    // Corrupt the transaction count after the header. Header serving should
    // not decode the body; full-block reads must still reject that body.
    {
        auto file = blockman.OpenBlockFile({pos.nFile, pos.nPos + unsigned(GetSerializeSize(block.GetBlockHeader()))}, false);
        file << uint8_t{255} << uint64_t{MAX_SIZE + 1};
    }
    BOOST_REQUIRE(blockman.ReadBlockHeader(header, index));
    CBlock full;
    BOOST_CHECK(!blockman.ReadBlock(full, index));
}

BOOST_AUTO_TEST_CASE(cache_after_sixty_seconds_requires_real_mempool_change)
{
    StubMining miner{*m_node.chainman};
    auxpow_miner::TemplateCache cache;
    const CScript script{CScript{} << OP_TRUE};
    const auto original = cache.createBlock(script, miner, *m_node.chainman, *m_node.mempool);
    std::this_thread::sleep_for(std::chrono::seconds{61});
    BOOST_CHECK(cache.createBlock(script, miner, *m_node.chainman, *m_node.mempool) == original);
    m_node.mempool->AddTransactionsUpdated(1);
    BOOST_CHECK(cache.createBlock(script, miner, *m_node.chainman, *m_node.mempool) != original);
    BOOST_CHECK_EQUAL(miner.builds, 2);
}

BOOST_AUTO_TEST_CASE(header_cache_evicts_old_proofs_at_memory_limit)
{
    auto& blockman = m_node.chainman->m_blockman;
    std::vector<uint256> hashes;
    std::vector<std::unique_ptr<CBlockIndex>> indexes;
    hashes.reserve(10);
    indexes.reserve(10);
    for (int i = 0; i < 10; ++i) {
        CBlock block{m_node.chainman->GetParams().GenesisBlock()};
        block.nTime += i + 1;
        CAuxPow::initAuxPow(block);
        CMutableTransaction tx{*block.auxpow->tx};
        tx.vout.emplace_back(0, CScript{} << std::vector<unsigned char>(1900000, 0));
        block.auxpow->tx = MakeTransactionRef(std::move(tx));
        hashes.push_back(block.GetHash());
        auto& index = *indexes.emplace_back(std::make_unique<CBlockIndex>(block));
        index.phashBlock = &hashes.back();
        const auto pos = blockman.WriteBlock(block, 1);
        index.nFile = pos.nFile;
        index.nDataPos = pos.nPos;
        index.nStatus |= BLOCK_HAVE_DATA;
        CBlockHeader header;
        BOOST_REQUIRE(blockman.ReadBlockHeader(header, index));
    }
    // Ten 1.9 MB proofs exceed the 16 MiB cache. Mutating the disk copies
    // distinguishes a reread of the evicted entry from a hit on the newest.
    for (const auto* index : {indexes.front().get(), indexes.back().get()}) {
        auto file = blockman.OpenBlockFile({index->nFile, index->nDataPos + 76}, false);
        file << uint32_t(index->nNonce + 1);
    }
    CBlockHeader header;
    BOOST_CHECK(!blockman.ReadBlockHeader(header, *indexes.front()));
    BOOST_CHECK(blockman.ReadBlockHeader(header, *indexes.back()));
}

BOOST_AUTO_TEST_CASE(auxpow_disk_header_roundtrip_and_missing_data)
{
    auto& blockman = m_node.chainman->m_blockman;
    CBlock block{m_node.chainman->GetParams().GenesisBlock()};
    block.nTime++;
    CAuxPow::initAuxPow(block);
    const auto hash = block.GetHash();
    CBlockIndex index{block};
    index.phashBlock = &hash;
    const auto pos = blockman.WriteBlock(block, 1);
    index.nFile = pos.nFile;
    index.nDataPos = pos.nPos;
    index.nStatus |= BLOCK_HAVE_DATA;
    CBlockHeader header;
    BOOST_REQUIRE(blockman.ReadBlockHeader(header, index));
    DataStream actual, expected;
    actual << header;
    expected << block.GetBlockHeader();
    BOOST_CHECK(actual.str() == expected.str());
    BOOST_CHECK(header.auxpow != nullptr);
    BOOST_CHECK_EQUAL(GetSerializeSize(index.GetBlockHeader()), 80U);
    BOOST_TEST_MESSAGE("AuxPoW disk header bytes=" << actual.size());
    // Returned proofs must not alias the cached proof or poison later reads.
    header.auxpow->parentBlock.nNonce++;
    BOOST_REQUIRE(blockman.ReadBlockHeader(header, index));
    DataStream again;
    again << header;
    BOOST_CHECK(again.str() == expected.str());
    index.nStatus &= ~BLOCK_HAVE_DATA;
    BOOST_CHECK(!blockman.ReadBlockHeader(header, index));
}

static void CheckGetheadersBatch(node::NodeContext& m_node, size_t proof_padding, int block_count = 17, size_t body_padding = 0, bool non_witness_padding = false)
{
    LOCK(NetEventsInterface::g_msgproc_mutex);
    auto& chainman = *m_node.chainman;
    auto& blockman = chainman.m_blockman;
    auto& connman = static_cast<ConnmanTestMsg&>(*m_node.connman);
    CNode peer{42, nullptr, CAddress{}, 0, 0, CAddress{}, "", ConnectionType::INBOUND, false};
    connman.Handshake(peer, true, ServiceFlags(NODE_NETWORK | NODE_WITNESS),
                      ServiceFlags(NODE_NETWORK | NODE_WITNESS), PROTOCOL_VERSION, true);
    connman.FlushSendBuffer(peer);
    peer.fPauseSend = false;
    struct FinalizePeer {
        PeerManager& peerman;
        CNode& peer;
        ~FinalizePeer() { peerman.FinalizeNode(peer); }
    } finalize{*m_node.peerman, peer};
    CBlockIndex* original = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
    struct RestoreTip {
        ChainstateManager& chainman;
        CBlockIndex* tip;
        ~RestoreTip() { LOCK(cs_main); chainman.ActiveChain().SetTip(*tip); }
    } restore{chainman, original};
    CBlockIndex* tip = original;
    size_t disk_bytes{0};
    for (int i = 0; i < block_count; ++i) {
        CBlock block{chainman.GetParams().GenesisBlock()};
        if (body_padding) {
            CMutableTransaction cb{*block.vtx[0]};
            cb.vout[0].scriptPubKey = CScript{} << std::vector<unsigned char>(body_padding, 0);
            block.vtx[0] = MakeTransactionRef(cb);
            block.hashMerkleRoot = BlockMerkleRoot(block);
        }
        block.hashPrevBlock = tip->GetBlockHash();
        block.nTime += i + 1;
        block.nVersion.SetBaseVersion(4, chainman.GetConsensus().nAuxpowChainId);
        block.nVersion.SetAuxpow(true);
        // Valid commitment layout; witness padding does not change the txid
        // or the parent-chain PoW commitment. These are disk/index fixtures,
        // not mined blocks or a claim of end-to-end chain acceptance.
        auto hash = block.GetHash();
        std::vector<unsigned char> root(hash.begin(), hash.end());
        std::reverse(root.begin(), root.end());
        std::vector<unsigned char> script{0xfa, 0xbe, 'm', 'm'};
        script.insert(script.end(), root.begin(), root.end());
        script.insert(script.end(), {1, 0, 0, 0, 0, 0, 0, 0});
        CMutableTransaction parent_cb;
        parent_cb.vin.resize(1);
        parent_cb.vin[0].prevout.SetNull();
        parent_cb.vin[0].scriptSig = CScript{script.begin(), script.end()};
        if (non_witness_padding) {
            // This is committed data and cannot be removed by witness stripping.
            parent_cb.vout.emplace_back(0, CScript{} << std::vector<unsigned char>(proof_padding, 0));
        } else {
            parent_cb.vin[0].scriptWitness.stack = {std::vector<unsigned char>(proof_padding, 0)};
            parent_cb.vout.emplace_back(0, CScript{} << OP_TRUE);
        }
        CMutableTransaction unpadded{parent_cb};
        unpadded.vin[0].scriptWitness.SetNull();
        BOOST_REQUIRE(parent_cb.GetHash() == unpadded.GetHash());
        block.auxpow = std::make_shared<CAuxPow>(MakeTransactionRef(parent_cb));
        block.auxpow->nIndex = 0;
        block.auxpow->nChainIndex = 0;
        block.auxpow->parentBlock.nVersion.SetGenesisVersion(1);
        block.auxpow->parentBlock.hashMerkleRoot = block.auxpow->GetHash();
        BOOST_REQUIRE(block.auxpow->check(hash, block.nVersion.GetChainId(), chainman.GetConsensus()));
        BOOST_REQUIRE(GetBlockWeight(block) <= MAX_BLOCK_WEIGHT);
        if (!non_witness_padding && i == 0) {
            CBlock no_parent_witness{block};
            no_parent_witness.auxpow = std::make_shared<CAuxPow>(*block.auxpow);
            no_parent_witness.auxpow->tx = MakeTransactionRef(unpadded);
            const auto size_delta = GetSerializeSize(TX_WITH_WITNESS(block)) - GetSerializeSize(TX_WITH_WITNESS(no_parent_witness));
            const auto weight_delta = GetBlockWeight(block) - GetBlockWeight(no_parent_witness);
            BOOST_TEST_MESSAGE("parent witness serialized-byte delta=" << size_delta << " weight delta=" << weight_delta);
            BOOST_CHECK_EQUAL(weight_delta, size_delta * WITNESS_SCALE_FACTOR);
        }
        const auto pos = blockman.WriteBlock(block, i + 1);
        disk_bytes += GetSerializeSize(TX_WITH_WITNESS(block));
        {
            LOCK(cs_main);
            tip = blockman.AddToBlockIndex(block, chainman.m_best_header);
            tip->nFile = pos.nFile;
            tip->nDataPos = pos.nPos;
            tip->nStatus |= BLOCK_HAVE_DATA;
            chainman.ActiveChain().SetTip(*tip);
        }
    }

    auto drain = [](CNode& node) {
        std::vector<uint8_t> wire;
        LOCK(node.cs_vSend);
        while (true) {
            const auto& [bytes, more, type] = node.m_transport->GetBytesToSend(false);
            if (bytes.empty()) break;
            wire.insert(wire.end(), bytes.begin(), bytes.end());
            node.m_transport->MarkBytesSent(bytes.size());
        }
        return wire;
    };
    auto request_batch = [&](const uint256& from, bool check_fairness) {
        DataStream request;
        request << CBlockLocator{{from}} << uint256{};
        const auto started = std::chrono::steady_clock::now();
        m_node.peerman->ProcessMessage(peer, NetMsgType::GETHEADERS, request, 0us, std::atomic<bool>{false});
        auto max_slice = std::chrono::steady_clock::now() - started;
        size_t slices{1};
        if (check_fairness) {
            // A 2,000-header range cannot complete in the first 16-read slice.
            BOOST_CHECK(drain(peer).empty());
            // A second peer's ping is processed while the response is pending.
            CNode other{45, nullptr, CAddress{}, 0, 0, CAddress{}, "", ConnectionType::INBOUND, false};
            connman.Handshake(other, true, ServiceFlags(NODE_NETWORK | NODE_WITNESS),
                              ServiceFlags(NODE_NETWORK | NODE_WITNESS), PROTOCOL_VERSION, true);
            FinalizePeer finalize_other{*m_node.peerman, other};
            connman.FlushSendBuffer(other);
            other.fPauseSend = false;
            BOOST_REQUIRE(connman.ReceiveMsgFrom(other, NetMsg::Make(NetMsgType::PING, uint64_t{123})));
            connman.ProcessMessagesOnce(other);
            const auto pong = drain(other);
            BOOST_REQUIRE_EQUAL(pong.size(), 32U);
            BOOST_CHECK_EQUAL(std::string(pong.begin() + 4, pong.begin() + 8), "pong");
            SpanReader nonce_reader{std::span<const unsigned char>{pong}.subspan(24)};
            uint64_t nonce;
            nonce_reader >> nonce;
            BOOST_CHECK_EQUAL(nonce, 123U);
            // A duplicate cannot overwrite the in-flight range with an empty reply.
            DataStream duplicate;
            duplicate << CBlockLocator{{tip->GetBlockHash()}} << uint256{};
            m_node.peerman->ProcessMessage(peer, NetMsgType::GETHEADERS, duplicate, 0us, std::atomic<bool>{false});
        }
        bool more;
        do {
            const auto slice_start = std::chrono::steady_clock::now();
            more = connman.ProcessMessagesOnce(peer);
            max_slice = std::max(max_slice, std::chrono::steady_clock::now() - slice_start);
            BOOST_REQUIRE(++slices <= 2002);
        } while (more);
        const auto served = std::chrono::steady_clock::now();
        const auto wire = drain(peer);
        peer.fPauseSend = false;
        BOOST_REQUIRE(wire.size() > 24);
        BOOST_CHECK(wire.size() - 24 <= MAX_PROTOCOL_MESSAGE_LENGTH);
        CNode receiver{43, nullptr, CAddress{}, 0, 0, CAddress{}, "", ConnectionType::INBOUND, false};
        bool complete{false};
        BOOST_REQUIRE(receiver.ReceiveMsgBytes(wire, complete));
        BOOST_CHECK(complete);
        SpanReader payload{std::span<const unsigned char>{wire}.subspan(24)};
        std::vector<CBlock> headers;
        payload >> TX_WITH_WITNESS(headers);
        BOOST_CHECK(payload.empty());
        std::vector<CBlockHeader> received;
        size_t header_bytes{0};
        uint256 previous{from};
        for (const auto& header : headers) {
            BOOST_REQUIRE(header.auxpow);
            BOOST_CHECK(header.hashPrevBlock == previous);
            BOOST_CHECK(header.vtx.empty());
            BOOST_CHECK(header.auxpow->check(header.GetHash(), header.nVersion.GetChainId(), chainman.GetConsensus()));
            BOOST_CHECK(!header.auxpow->tx->HasWitness());
            previous = header.GetHash();
            header_bytes += GetSerializeSize(header.GetBlockHeader());
            received.push_back(header.GetBlockHeader());
        }
        BOOST_CHECK(header_bytes <= MAX_HEADERS_MESSAGE_SIZE);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        BOOST_TEST_MESSAGE("getheaders: returned=" << received.size() << " header_bytes=" << header_bytes
            << " slices=" << slices << " serve_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(served - started).count()
            << " max_slice_us=" << std::chrono::duration_cast<std::chrono::microseconds>(max_slice).count() << " total_ms=" << elapsed);
        if (check_fairness) BOOST_CHECK(slices >= 125);
        return received;
    };

    uint256 previous{original->GetBlockHash()};
    size_t total{0};
    std::vector<std::vector<CBlockHeader>> batches;
    do {
        auto received = request_batch(previous, block_count == 2000 && total == 0);
        total += received.size();
        BOOST_REQUIRE(total <= static_cast<size_t>(block_count));
        if (!received.empty()) previous = received.back().GetHash();
        const bool full = HeadersMessageIsFull(received);
        if (total < static_cast<size_t>(block_count)) BOOST_REQUIRE(full);
        batches.push_back(std::move(received));
        if (!full) break;
    } while (true);
    BOOST_CHECK_EQUAL(total, static_cast<size_t>(block_count));
    BOOST_CHECK(previous == tip->GetBlockHash());

    // Exercise both production presync phases with the actual size-limited
    // batches. HeadersSyncState assumes PoW was verified by its caller;
    // these fixtures test commitments and continuation, not block acceptance.
    HeadersSyncState sync{44, chainman.GetConsensus(), original, tip->nChainWork};
    for (const auto& batch : batches) {
        if (batch.empty()) continue;
        const auto result = sync.ProcessNextHeaders(batch, HeadersMessageIsFull(batch));
        BOOST_REQUIRE(result.success);
        BOOST_REQUIRE(result.request_more);
    }
    BOOST_REQUIRE(sync.GetState() == HeadersSyncState::State::REDOWNLOAD);
    size_t released{0};
    for (const auto& batch : batches) {
        if (batch.empty()) continue;
        const auto result = sync.ProcessNextHeaders(batch, HeadersMessageIsFull(batch));
        BOOST_REQUIRE(result.success);
        released += result.pow_validated_headers.size();
        if (released < total) BOOST_REQUIRE(result.request_more);
    }
    BOOST_CHECK_EQUAL(released, total);
    BOOST_CHECK(sync.GetState() == HeadersSyncState::State::FINAL);

    // Repeat against the warm shared header cache, including a second final
    // empty response for count-full ranges ending exactly at the tip.
    const auto warm = request_batch(original->GetBlockHash(), block_count == 2000);
    BOOST_CHECK_EQUAL(warm.size(), batches.front().size());
}

BOOST_AUTO_TEST_CASE(getheaders_normal_auxpow_batch_roundtrips)
{
    CheckGetheadersBatch(m_node, 32);
}

BOOST_AUTO_TEST_CASE(getheaders_large_auxpow_batch_must_fit_transport)
{
    CheckGetheadersBatch(m_node, 1900000);
}

BOOST_AUTO_TEST_CASE(getheaders_full_batch_yields_to_other_peers)
{
    CheckGetheadersBatch(m_node, 32, 2000, 16384);
}

BOOST_AUTO_TEST_CASE(getheaders_non_witness_proof_cap_must_preserve_continuation)
{
    CheckGetheadersBatch(m_node, 1900000, 17, 0, true);
}

BOOST_AUTO_TEST_SUITE_END()
