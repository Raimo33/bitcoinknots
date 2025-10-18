#include <bench/bench.h>
#include <kernel/cs_main.h>
#include <policy/rbf.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <sync.h>
#include <test/util/setup_common.h>
#include <test/util/txmempool.h>
#include <txmempool.h>

#include <memory>
#include <vector>

namespace {

CMutableTransaction MakeTx(const std::vector<COutPoint>& inputs, const std::vector<CAmount>& values) {
    CMutableTransaction tx;
    tx.vin.resize(inputs.size());
    for (size_t i = 0; i < inputs.size(); i++) {
        tx.vin[i].prevout = inputs[i];
    }
    tx.vout.resize(values.size());
    for (size_t i = 0; i < values.size(); i++) {
        tx.vout[i].nValue = values[i];
        tx.vout[i].scriptPubKey = CScript() << OP_1;
    }
    return tx;
}

CTransactionRef AddTxToMempool(CTxMemPool& pool, TestMemPoolEntryHelper& entry, const CMutableTransaction& mtx, CAmount fee) {
    auto txref = MakeTransactionRef(mtx);
    AddToMempool(pool, entry.Fee(fee).FromTx(txref));
    return txref;
}

CTxMemPool::setEntries BuildConflicts(CTxMemPool& pool, const std::vector<CTransactionRef>& txs) {
    CTxMemPool::setEntries conflicts;
    for (const auto& tx : txs) {
        conflicts.insert(pool.GetIter(tx->GetHash()).value());
    }
    return conflicts;
}

} // namespace

static void RBFHasNoNewUnconfirmed_Positive(benchmark::Bench& bench) {
    const auto testing_setup = MakeNoLogFileContext<const TestChain100Setup>();
    CTxMemPool& pool = *Assert(testing_setup->m_node.mempool);
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    auto parent_tx = MakeTx({COutPoint(testing_setup->m_coinbase_txns[0]->GetHash(), 0)}, {5*COIN, 5*COIN});
    auto parent_ref = AddTxToMempool(pool, entry, parent_tx, 1000);

    auto conflict_tx = MakeTx({COutPoint(parent_ref->GetHash(), 0)}, {4*COIN});
    auto conflict_ref = AddTxToMempool(pool, entry, conflict_tx, 2000);
    auto conflicts = BuildConflicts(pool, {conflict_ref});

    auto replacement_tx = MakeTx({COutPoint(parent_ref->GetHash(), 1)}, {4*COIN});
    CTransaction replacement(replacement_tx);

    bench.run([&] NO_THREAD_SAFETY_ANALYSIS {
        auto result = HasNoNewUnconfirmed(replacement, pool, conflicts);
        ankerl::nanobench::doNotOptimizeAway(result);
    });
}

static void RBFHasNoNewUnconfirmed_Negative(benchmark::Bench& bench) {
    const auto testing_setup = MakeNoLogFileContext<const TestChain100Setup>();
    CTxMemPool& pool = *Assert(testing_setup->m_node.mempool);
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    auto unrelated_parent_tx = MakeTx({COutPoint(testing_setup->m_coinbase_txns[1]->GetHash(), 0)}, {5*COIN});
    auto unrelated_parent_ref = AddTxToMempool(pool, entry, unrelated_parent_tx, 1500);

    auto parent_tx = MakeTx({COutPoint(testing_setup->m_coinbase_txns[2]->GetHash(), 0)}, {5*COIN, 5*COIN});
    auto parent_ref = AddTxToMempool(pool, entry, parent_tx, 1000);

    auto conflict_tx = MakeTx({COutPoint(parent_ref->GetHash(), 0)}, {4*COIN});
    auto conflict_ref = AddTxToMempool(pool, entry, conflict_tx, 2000);
    auto conflicts = BuildConflicts(pool, {conflict_ref});

    auto replacement_tx = MakeTx({COutPoint(parent_ref->GetHash(), 1),
                                  COutPoint(unrelated_parent_ref->GetHash(), 0)}, {3*COIN});
    CTransaction replacement(replacement_tx);

    bench.run([&] NO_THREAD_SAFETY_ANALYSIS {
        auto result = HasNoNewUnconfirmed(replacement, pool, conflicts);
        ankerl::nanobench::doNotOptimizeAway(result);
    });
}

static void RBFHasNoNewUnconfirmed_Multiple(benchmark::Bench& bench) {
    const auto testing_setup = MakeNoLogFileContext<const TestChain100Setup>();
    CTxMemPool& pool = *Assert(testing_setup->m_node.mempool);
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    std::vector<CTransactionRef> parent_txs;
    for (size_t i = 0; i < 5; i++) {
        auto parent_tx = MakeTx({COutPoint(testing_setup->m_coinbase_txns[i]->GetHash(), 0)}, {5*COIN, 5*COIN});
        parent_txs.push_back(AddTxToMempool(pool, entry, parent_tx, 1000*(i+1)));
    }

    std::vector<CTransactionRef> conflict_refs;
    for (size_t i = 0; i < parent_txs.size(); i++) {
        auto conflict_tx = MakeTx({COutPoint(parent_txs[i]->GetHash(), 0)}, {4*COIN});
        conflict_refs.push_back(AddTxToMempool(pool, entry, conflict_tx, 2000*(i+1)));
    }
    auto conflicts = BuildConflicts(pool, conflict_refs);

    CMutableTransaction replacement_tx;
    replacement_tx.vin.resize(3);
    replacement_tx.vout.resize(1);
    for (size_t i = 0; i < 3; i++) {
        replacement_tx.vin[i].prevout = COutPoint(parent_txs[i]->GetHash(), 1);
    }
    replacement_tx.vout[0].scriptPubKey = CScript() << OP_1;
    replacement_tx.vout[0].nValue = 12*COIN;
    CTransaction replacement(replacement_tx);

    bench.run([&] NO_THREAD_SAFETY_ANALYSIS {
        auto result = HasNoNewUnconfirmed(replacement, pool, conflicts);
        ankerl::nanobench::doNotOptimizeAway(result);
    });
}

BENCHMARK(RBFHasNoNewUnconfirmed_Positive, benchmark::PriorityLevel::HIGH);
BENCHMARK(RBFHasNoNewUnconfirmed_Negative, benchmark::PriorityLevel::HIGH);
BENCHMARK(RBFHasNoNewUnconfirmed_Multiple, benchmark::PriorityLevel::HIGH);
