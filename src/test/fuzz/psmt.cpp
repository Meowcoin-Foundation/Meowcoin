// Copyright (c) 2019-2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/psmt.h>
#include <psmt.h>
#include <pubkey.h>
#include <script/script.h>
#include <streams.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/util/random.h>
#include <util/check.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using node::AnalyzePSMT;
using node::PSMTAnalysis;
using node::PSMTInputAnalysis;

FUZZ_TARGET(psmt)
{
    SeedRandomStateForTest(SeedRand::ZEROS);
    FuzzedDataProvider fuzzed_data_provider{buffer.data(), buffer.size()};
    PartiallySignedTransaction psmt_mut;
    std::string error;
    auto str = fuzzed_data_provider.ConsumeRandomLengthString();
    if (!DecodeRawPSMT(psmt_mut, MakeByteSpan(str), error)) {
        return;
    }
    const PartiallySignedTransaction psmt = psmt_mut;

    const PSMTAnalysis analysis = AnalyzePSMT(psmt);
    (void)PSMTRoleName(analysis.next);
    for (const PSMTInputAnalysis& input_analysis : analysis.inputs) {
        (void)PSMTRoleName(input_analysis.next);
    }

    (void)psmt.IsNull();

    std::optional<CMutableTransaction> tx = psmt.tx;
    if (tx) {
        const CMutableTransaction& mtx = *tx;
        const PartiallySignedTransaction psmt_from_tx{mtx};
    }

    for (const PSMTInput& input : psmt.inputs) {
        (void)PSMTInputSigned(input);
        (void)input.IsNull();
    }
    (void)CountPSMTUnsignedInputs(psmt);

    for (const PSMTOutput& output : psmt.outputs) {
        (void)output.IsNull();
    }

    for (size_t i = 0; i < psmt.tx->vin.size(); ++i) {
        CTxOut tx_out;
        if (psmt.GetInputUTXO(tx_out, i)) {
            (void)tx_out.IsNull();
            (void)tx_out.ToString();
        }
    }

    psmt_mut = psmt;
    (void)FinalizePSMT(psmt_mut);

    psmt_mut = psmt;
    CMutableTransaction result;
    if (FinalizeAndExtractPSMT(psmt_mut, result)) {
        const PartiallySignedTransaction psmt_from_tx{result};
    }

    PartiallySignedTransaction psmt_merge;
    str = fuzzed_data_provider.ConsumeRandomLengthString();
    if (!DecodeRawPSMT(psmt_merge, MakeByteSpan(str), error)) {
        psmt_merge = psmt;
    }
    psmt_mut = psmt;
    (void)psmt_mut.Merge(psmt_merge);
    psmt_mut = psmt;
    (void)CombinePSMTs(psmt_mut, {psmt_mut, psmt_merge});
    psmt_mut = psmt;
    for (unsigned int i = 0; i < psmt_merge.tx->vin.size(); ++i) {
        (void)psmt_mut.AddInput(psmt_merge.tx->vin[i], psmt_merge.inputs[i]);
    }
    for (unsigned int i = 0; i < psmt_merge.tx->vout.size(); ++i) {
        Assert(psmt_mut.AddOutput(psmt_merge.tx->vout[i], psmt_merge.outputs[i]));
    }
    psmt_mut.unknown.insert(psmt_merge.unknown.begin(), psmt_merge.unknown.end());
}
