// Copyright (c) 2009-2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <consensus/amount.h>
#include <consensus/tx_verify.h>
#include <node/psmt.h>
#include <policy/policy.h>
#include <policy/settings.h>
#include <tinyformat.h>

#include <numeric>

namespace node {
PSMTAnalysis AnalyzePSMT(PartiallySignedTransaction psmtx)
{
    // Go through each input and build status
    PSMTAnalysis result;

    bool calc_fee = true;

    CAmount in_amt = 0;

    result.inputs.resize(psmtx.tx->vin.size());

    const PrecomputedTransactionData txdata = PrecomputePSMTData(psmtx);

    for (unsigned int i = 0; i < psmtx.tx->vin.size(); ++i) {
        PSMTInput& input = psmtx.inputs[i];
        PSMTInputAnalysis& input_analysis = result.inputs[i];

        // We set next role here and ratchet backwards as required
        input_analysis.next = PSMTRole::EXTRACTOR;

        // Check for a UTXO
        CTxOut utxo;
        if (psmtx.GetInputUTXO(utxo, i)) {
            if (!MoneyRange(utxo.nValue) || !MoneyRange(in_amt + utxo.nValue)) {
                result.SetInvalid(strprintf("PSMT is not valid. Input %u has invalid value", i));
                return result;
            }
            in_amt += utxo.nValue;
            input_analysis.has_utxo = true;
        } else {
            if (input.non_witness_utxo && psmtx.tx->vin[i].prevout.n >= input.non_witness_utxo->vout.size()) {
                result.SetInvalid(strprintf("PSMT is not valid. Input %u specifies invalid prevout", i));
                return result;
            }
            input_analysis.has_utxo = false;
            input_analysis.is_final = false;
            input_analysis.next = PSMTRole::UPDATER;
            calc_fee = false;
        }

        if (!utxo.IsNull() && utxo.scriptPubKey.IsUnspendable()) {
            result.SetInvalid(strprintf("PSMT is not valid. Input %u spends unspendable output", i));
            return result;
        }

        // Check if it is final
        if (!PSMTInputSignedAndVerified(psmtx, i, &txdata)) {
            input_analysis.is_final = false;

            // Figure out what is missing
            SignatureData outdata;
            bool complete = SignPSMTInput(DUMMY_SIGNING_PROVIDER, psmtx, i, &txdata, std::nullopt, &outdata) == PSMTError::OK;

            // Things are missing
            if (!complete) {
                input_analysis.missing_pubkeys = outdata.missing_pubkeys;
                input_analysis.missing_redeem_script = outdata.missing_redeem_script;
                input_analysis.missing_witness_script = outdata.missing_witness_script;
                input_analysis.missing_sigs = outdata.missing_sigs;

                // If we are only missing signatures and nothing else, then next is signer
                if (outdata.missing_pubkeys.empty() && outdata.missing_redeem_script.IsNull() && outdata.missing_witness_script.IsNull() && !outdata.missing_sigs.empty()) {
                    input_analysis.next = PSMTRole::SIGNER;
                } else {
                    input_analysis.next = PSMTRole::UPDATER;
                }
            } else {
                input_analysis.next = PSMTRole::FINALIZER;
            }
        } else if (!utxo.IsNull()){
            input_analysis.is_final = true;
        }
    }

    // Calculate next role for PSMT by grabbing "minimum" PSMTInput next role
    result.next = PSMTRole::EXTRACTOR;
    for (unsigned int i = 0; i < psmtx.tx->vin.size(); ++i) {
        PSMTInputAnalysis& input_analysis = result.inputs[i];
        result.next = std::min(result.next, input_analysis.next);
    }
    assert(result.next > PSMTRole::CREATOR);

    if (calc_fee) {
        // Get the output amount
        CAmount out_amt = std::accumulate(psmtx.tx->vout.begin(), psmtx.tx->vout.end(), CAmount(0),
            [](CAmount a, const CTxOut& b) {
                if (!MoneyRange(a) || !MoneyRange(b.nValue) || !MoneyRange(a + b.nValue)) {
                    return CAmount(-1);
                }
                return a += b.nValue;
            }
        );
        if (!MoneyRange(out_amt)) {
            result.SetInvalid("PSMT is not valid. Output amount invalid");
            return result;
        }

        // Get the fee
        CAmount fee = in_amt - out_amt;
        result.fee = fee;

        // Estimate the size
        CMutableTransaction mtx(*psmtx.tx);
        CCoinsView view_dummy;
        CCoinsViewCache view(&view_dummy);
        bool success = true;

        for (unsigned int i = 0; i < psmtx.tx->vin.size(); ++i) {
            PSMTInput& input = psmtx.inputs[i];
            Coin newcoin;

            if (SignPSMTInput(DUMMY_SIGNING_PROVIDER, psmtx, i, nullptr, std::nullopt) != PSMTError::OK || !psmtx.GetInputUTXO(newcoin.out, i)) {
                success = false;
                break;
            } else {
                mtx.vin[i].scriptSig = input.final_script_sig;
                mtx.vin[i].scriptWitness = input.final_script_witness;
                newcoin.nHeight = 1;
                view.AddCoin(psmtx.tx->vin[i].prevout, std::move(newcoin), true);
            }
        }

        if (success) {
            CTransaction ctx = CTransaction(mtx);
            size_t size(GetVirtualTransactionSize(ctx, GetTransactionSigOpCost(ctx, view, STANDARD_SCRIPT_VERIFY_FLAGS), ::nBytesPerSigOp));
            result.estimated_vsize = size;
            // Estimate fee rate
            CFeeRate feerate(fee, size);
            result.estimated_feerate = feerate;
        }

    }

    return result;
}
} // namespace node
