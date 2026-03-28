// Copyright (c) 2009-present The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <psmt.h>

#include <common/types.h>
#include <node/types.h>
#include <policy/policy.h>
#include <script/signingprovider.h>
#include <util/check.h>
#include <util/strencodings.h>

using common::PSMTError;

PartiallySignedTransaction::PartiallySignedTransaction(const CMutableTransaction& tx) : tx(tx)
{
    inputs.resize(tx.vin.size());
    outputs.resize(tx.vout.size());
}

bool PartiallySignedTransaction::IsNull() const
{
    return !tx && inputs.empty() && outputs.empty() && unknown.empty();
}

bool PartiallySignedTransaction::Merge(const PartiallySignedTransaction& psmt)
{
    // Prohibited to merge two PSMTs over different transactions
    if (tx->GetHash() != psmt.tx->GetHash()) {
        return false;
    }

    for (unsigned int i = 0; i < inputs.size(); ++i) {
        inputs[i].Merge(psmt.inputs[i]);
    }
    for (unsigned int i = 0; i < outputs.size(); ++i) {
        outputs[i].Merge(psmt.outputs[i]);
    }
    for (auto& xpub_pair : psmt.m_xpubs) {
        if (m_xpubs.count(xpub_pair.first) == 0) {
            m_xpubs[xpub_pair.first] = xpub_pair.second;
        } else {
            m_xpubs[xpub_pair.first].insert(xpub_pair.second.begin(), xpub_pair.second.end());
        }
    }
    unknown.insert(psmt.unknown.begin(), psmt.unknown.end());

    return true;
}

bool PartiallySignedTransaction::AddInput(const CTxIn& txin, PSMTInput& psmtin)
{
    if (std::find(tx->vin.begin(), tx->vin.end(), txin) != tx->vin.end()) {
        return false;
    }
    tx->vin.push_back(txin);
    psmtin.partial_sigs.clear();
    psmtin.final_script_sig.clear();
    psmtin.final_script_witness.SetNull();
    inputs.push_back(psmtin);
    return true;
}

bool PartiallySignedTransaction::AddOutput(const CTxOut& txout, const PSMTOutput& psmtout)
{
    tx->vout.push_back(txout);
    outputs.push_back(psmtout);
    return true;
}

bool PartiallySignedTransaction::GetInputUTXO(CTxOut& utxo, int input_index) const
{
    const PSMTInput& input = inputs[input_index];
    uint32_t prevout_index = tx->vin[input_index].prevout.n;
    if (input.non_witness_utxo) {
        if (prevout_index >= input.non_witness_utxo->vout.size()) {
            return false;
        }
        if (input.non_witness_utxo->GetHash() != tx->vin[input_index].prevout.hash) {
            return false;
        }
        utxo = input.non_witness_utxo->vout[prevout_index];
    } else if (!input.witness_utxo.IsNull()) {
        utxo = input.witness_utxo;
    } else {
        return false;
    }
    return true;
}

bool PSMTInput::IsNull() const
{
    return !non_witness_utxo && witness_utxo.IsNull() && partial_sigs.empty() && unknown.empty() && hd_keypaths.empty() && redeem_script.empty() && witness_script.empty();
}

void PSMTInput::FillSignatureData(SignatureData& sigdata) const
{
    if (!final_script_sig.empty()) {
        sigdata.scriptSig = final_script_sig;
        sigdata.complete = true;
    }
    if (!final_script_witness.IsNull()) {
        sigdata.scriptWitness = final_script_witness;
        sigdata.complete = true;
    }
    if (sigdata.complete) {
        return;
    }

    sigdata.signatures.insert(partial_sigs.begin(), partial_sigs.end());
    if (!redeem_script.empty()) {
        sigdata.redeem_script = redeem_script;
    }
    if (!witness_script.empty()) {
        sigdata.witness_script = witness_script;
    }
    for (const auto& key_pair : hd_keypaths) {
        sigdata.misc_pubkeys.emplace(key_pair.first.GetID(), key_pair);
    }
    if (!m_tap_key_sig.empty()) {
        sigdata.taproot_key_path_sig = m_tap_key_sig;
    }
    for (const auto& [pubkey_leaf, sig] : m_tap_script_sigs) {
        sigdata.taproot_script_sigs.emplace(pubkey_leaf, sig);
    }
    if (!m_tap_internal_key.IsNull()) {
        sigdata.tr_spenddata.internal_key = m_tap_internal_key;
    }
    if (!m_tap_merkle_root.IsNull()) {
        sigdata.tr_spenddata.merkle_root = m_tap_merkle_root;
    }
    for (const auto& [leaf_script, control_block] : m_tap_scripts) {
        sigdata.tr_spenddata.scripts.emplace(leaf_script, control_block);
    }
    for (const auto& [pubkey, leaf_origin] : m_tap_bip32_paths) {
        sigdata.taproot_misc_pubkeys.emplace(pubkey, leaf_origin);
        sigdata.tap_pubkeys.emplace(Hash160(pubkey), pubkey);
    }
    for (const auto& [hash, preimage] : ripemd160_preimages) {
        sigdata.ripemd160_preimages.emplace(std::vector<unsigned char>(hash.begin(), hash.end()), preimage);
    }
    for (const auto& [hash, preimage] : sha256_preimages) {
        sigdata.sha256_preimages.emplace(std::vector<unsigned char>(hash.begin(), hash.end()), preimage);
    }
    for (const auto& [hash, preimage] : hash160_preimages) {
        sigdata.hash160_preimages.emplace(std::vector<unsigned char>(hash.begin(), hash.end()), preimage);
    }
    for (const auto& [hash, preimage] : hash256_preimages) {
        sigdata.hash256_preimages.emplace(std::vector<unsigned char>(hash.begin(), hash.end()), preimage);
    }
}

void PSMTInput::FromSignatureData(const SignatureData& sigdata)
{
    if (sigdata.complete) {
        partial_sigs.clear();
        hd_keypaths.clear();
        redeem_script.clear();
        witness_script.clear();

        if (!sigdata.scriptSig.empty()) {
            final_script_sig = sigdata.scriptSig;
        }
        if (!sigdata.scriptWitness.IsNull()) {
            final_script_witness = sigdata.scriptWitness;
        }
        return;
    }

    partial_sigs.insert(sigdata.signatures.begin(), sigdata.signatures.end());
    if (redeem_script.empty() && !sigdata.redeem_script.empty()) {
        redeem_script = sigdata.redeem_script;
    }
    if (witness_script.empty() && !sigdata.witness_script.empty()) {
        witness_script = sigdata.witness_script;
    }
    for (const auto& entry : sigdata.misc_pubkeys) {
        hd_keypaths.emplace(entry.second);
    }
    if (!sigdata.taproot_key_path_sig.empty()) {
        m_tap_key_sig = sigdata.taproot_key_path_sig;
    }
    for (const auto& [pubkey_leaf, sig] : sigdata.taproot_script_sigs) {
        m_tap_script_sigs.emplace(pubkey_leaf, sig);
    }
    if (!sigdata.tr_spenddata.internal_key.IsNull()) {
        m_tap_internal_key = sigdata.tr_spenddata.internal_key;
    }
    if (!sigdata.tr_spenddata.merkle_root.IsNull()) {
        m_tap_merkle_root = sigdata.tr_spenddata.merkle_root;
    }
    for (const auto& [leaf_script, control_block] : sigdata.tr_spenddata.scripts) {
        m_tap_scripts.emplace(leaf_script, control_block);
    }
    for (const auto& [pubkey, leaf_origin] : sigdata.taproot_misc_pubkeys) {
        m_tap_bip32_paths.emplace(pubkey, leaf_origin);
    }
}

void PSMTInput::Merge(const PSMTInput& input)
{
    if (!non_witness_utxo && input.non_witness_utxo) non_witness_utxo = input.non_witness_utxo;
    if (witness_utxo.IsNull() && !input.witness_utxo.IsNull()) {
        witness_utxo = input.witness_utxo;
    }

    partial_sigs.insert(input.partial_sigs.begin(), input.partial_sigs.end());
    ripemd160_preimages.insert(input.ripemd160_preimages.begin(), input.ripemd160_preimages.end());
    sha256_preimages.insert(input.sha256_preimages.begin(), input.sha256_preimages.end());
    hash160_preimages.insert(input.hash160_preimages.begin(), input.hash160_preimages.end());
    hash256_preimages.insert(input.hash256_preimages.begin(), input.hash256_preimages.end());
    hd_keypaths.insert(input.hd_keypaths.begin(), input.hd_keypaths.end());
    unknown.insert(input.unknown.begin(), input.unknown.end());
    m_tap_script_sigs.insert(input.m_tap_script_sigs.begin(), input.m_tap_script_sigs.end());
    m_tap_scripts.insert(input.m_tap_scripts.begin(), input.m_tap_scripts.end());
    m_tap_bip32_paths.insert(input.m_tap_bip32_paths.begin(), input.m_tap_bip32_paths.end());

    if (redeem_script.empty() && !input.redeem_script.empty()) redeem_script = input.redeem_script;
    if (witness_script.empty() && !input.witness_script.empty()) witness_script = input.witness_script;
    if (final_script_sig.empty() && !input.final_script_sig.empty()) final_script_sig = input.final_script_sig;
    if (final_script_witness.IsNull() && !input.final_script_witness.IsNull()) final_script_witness = input.final_script_witness;
    if (m_tap_key_sig.empty() && !input.m_tap_key_sig.empty()) m_tap_key_sig = input.m_tap_key_sig;
    if (m_tap_internal_key.IsNull() && !input.m_tap_internal_key.IsNull()) m_tap_internal_key = input.m_tap_internal_key;
    if (m_tap_merkle_root.IsNull() && !input.m_tap_merkle_root.IsNull()) m_tap_merkle_root = input.m_tap_merkle_root;
}

void PSMTOutput::FillSignatureData(SignatureData& sigdata) const
{
    if (!redeem_script.empty()) {
        sigdata.redeem_script = redeem_script;
    }
    if (!witness_script.empty()) {
        sigdata.witness_script = witness_script;
    }
    for (const auto& key_pair : hd_keypaths) {
        sigdata.misc_pubkeys.emplace(key_pair.first.GetID(), key_pair);
    }
    if (!m_tap_tree.empty() && m_tap_internal_key.IsFullyValid()) {
        TaprootBuilder builder;
        for (const auto& [depth, leaf_ver, script] : m_tap_tree) {
            builder.Add((int)depth, script, (int)leaf_ver, /*track=*/true);
        }
        assert(builder.IsComplete());
        builder.Finalize(m_tap_internal_key);
        TaprootSpendData spenddata = builder.GetSpendData();

        sigdata.tr_spenddata.internal_key = m_tap_internal_key;
        sigdata.tr_spenddata.Merge(spenddata);
    }
    for (const auto& [pubkey, leaf_origin] : m_tap_bip32_paths) {
        sigdata.taproot_misc_pubkeys.emplace(pubkey, leaf_origin);
        sigdata.tap_pubkeys.emplace(Hash160(pubkey), pubkey);
    }
}

void PSMTOutput::FromSignatureData(const SignatureData& sigdata)
{
    if (redeem_script.empty() && !sigdata.redeem_script.empty()) {
        redeem_script = sigdata.redeem_script;
    }
    if (witness_script.empty() && !sigdata.witness_script.empty()) {
        witness_script = sigdata.witness_script;
    }
    for (const auto& entry : sigdata.misc_pubkeys) {
        hd_keypaths.emplace(entry.second);
    }
    if (!sigdata.tr_spenddata.internal_key.IsNull()) {
        m_tap_internal_key = sigdata.tr_spenddata.internal_key;
    }
    if (sigdata.tr_builder.has_value() && sigdata.tr_builder->HasScripts()) {
        m_tap_tree = sigdata.tr_builder->GetTreeTuples();
    }
    for (const auto& [pubkey, leaf_origin] : sigdata.taproot_misc_pubkeys) {
        m_tap_bip32_paths.emplace(pubkey, leaf_origin);
    }
}

bool PSMTOutput::IsNull() const
{
    return redeem_script.empty() && witness_script.empty() && hd_keypaths.empty() && unknown.empty();
}

void PSMTOutput::Merge(const PSMTOutput& output)
{
    hd_keypaths.insert(output.hd_keypaths.begin(), output.hd_keypaths.end());
    unknown.insert(output.unknown.begin(), output.unknown.end());
    m_tap_bip32_paths.insert(output.m_tap_bip32_paths.begin(), output.m_tap_bip32_paths.end());

    if (redeem_script.empty() && !output.redeem_script.empty()) redeem_script = output.redeem_script;
    if (witness_script.empty() && !output.witness_script.empty()) witness_script = output.witness_script;
    if (m_tap_internal_key.IsNull() && !output.m_tap_internal_key.IsNull()) m_tap_internal_key = output.m_tap_internal_key;
    if (m_tap_tree.empty() && !output.m_tap_tree.empty()) m_tap_tree = output.m_tap_tree;
}

bool PSMTInputSigned(const PSMTInput& input)
{
    return !input.final_script_sig.empty() || !input.final_script_witness.IsNull();
}

bool PSMTInputSignedAndVerified(const PartiallySignedTransaction psmt, unsigned int input_index, const PrecomputedTransactionData* txdata)
{
    CTxOut utxo;
    assert(psmt.inputs.size() >= input_index);
    const PSMTInput& input = psmt.inputs[input_index];

    if (input.non_witness_utxo) {
        // If we're taking our information from a non-witness UTXO, verify that it matches the prevout.
        COutPoint prevout = psmt.tx->vin[input_index].prevout;
        if (prevout.n >= input.non_witness_utxo->vout.size()) {
            return false;
        }
        if (input.non_witness_utxo->GetHash() != prevout.hash) {
            return false;
        }
        utxo = input.non_witness_utxo->vout[prevout.n];
    } else if (!input.witness_utxo.IsNull()) {
        utxo = input.witness_utxo;
    } else {
        return false;
    }

    if (txdata) {
        return VerifyScript(input.final_script_sig, utxo.scriptPubKey, &input.final_script_witness, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker{&(*psmt.tx), input_index, utxo.nValue, *txdata, MissingDataBehavior::FAIL});
    } else {
        return VerifyScript(input.final_script_sig, utxo.scriptPubKey, &input.final_script_witness, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker{&(*psmt.tx), input_index, utxo.nValue, MissingDataBehavior::FAIL});
    }
}

size_t CountPSMTUnsignedInputs(const PartiallySignedTransaction& psmt) {
    size_t count = 0;
    for (const auto& input : psmt.inputs) {
        if (!PSMTInputSigned(input)) {
            count++;
        }
    }

    return count;
}

void UpdatePSMTOutput(const SigningProvider& provider, PartiallySignedTransaction& psmt, int index)
{
    CMutableTransaction& tx = *Assert(psmt.tx);
    const CTxOut& out = tx.vout.at(index);
    PSMTOutput& psmt_out = psmt.outputs.at(index);

    // Fill a SignatureData with output info
    SignatureData sigdata;
    psmt_out.FillSignatureData(sigdata);

    // Construct a would-be spend of this output, to update sigdata with.
    // Note that ProduceSignature is used to fill in metadata (not actual signatures),
    // so provider does not need to provide any private keys (it can be a HidingSigningProvider).
    MutableTransactionSignatureCreator creator(tx, /*input_idx=*/0, out.nValue, SIGHASH_ALL);
    ProduceSignature(provider, creator, out.scriptPubKey, sigdata);

    // Put redeem_script, witness_script, key paths, into PSMTOutput.
    psmt_out.FromSignatureData(sigdata);
}

PrecomputedTransactionData PrecomputePSMTData(const PartiallySignedTransaction& psmt)
{
    const CMutableTransaction& tx = *psmt.tx;
    bool have_all_spent_outputs = true;
    std::vector<CTxOut> utxos(tx.vin.size());
    for (size_t idx = 0; idx < tx.vin.size(); ++idx) {
        if (!psmt.GetInputUTXO(utxos[idx], idx)) have_all_spent_outputs = false;
    }
    PrecomputedTransactionData txdata;
    if (have_all_spent_outputs) {
        txdata.Init(tx, std::move(utxos), true);
    } else {
        txdata.Init(tx, {}, true);
    }
    return txdata;
}

PSMTError SignPSMTInput(const SigningProvider& provider, PartiallySignedTransaction& psmt, int index, const PrecomputedTransactionData* txdata, std::optional<int> sighash,  SignatureData* out_sigdata, bool finalize)
{
    PSMTInput& input = psmt.inputs.at(index);
    const CMutableTransaction& tx = *psmt.tx;

    if (PSMTInputSignedAndVerified(psmt, index, txdata)) {
        return PSMTError::OK;
    }

    // Fill SignatureData with input info
    SignatureData sigdata;
    input.FillSignatureData(sigdata);

    // Get UTXO
    bool require_witness_sig = false;
    CTxOut utxo;

    if (input.non_witness_utxo) {
        // If we're taking our information from a non-witness UTXO, verify that it matches the prevout.
        COutPoint prevout = tx.vin[index].prevout;
        if (prevout.n >= input.non_witness_utxo->vout.size()) {
            return PSMTError::MISSING_INPUTS;
        }
        if (input.non_witness_utxo->GetHash() != prevout.hash) {
            return PSMTError::MISSING_INPUTS;
        }
        utxo = input.non_witness_utxo->vout[prevout.n];
    } else if (!input.witness_utxo.IsNull()) {
        utxo = input.witness_utxo;
        // When we're taking our information from a witness UTXO, we can't verify it is actually data from
        // the output being spent. This is safe in case a witness signature is produced (which includes this
        // information directly in the hash), but not for non-witness signatures. Remember that we require
        // a witness signature in this situation.
        require_witness_sig = true;
    } else {
        return PSMTError::MISSING_INPUTS;
    }

    // Get the sighash type
    // If both the field and the parameter are provided, they must match
    // If only the parameter is provided, use it and add it to the PSMT if it is other than SIGHASH_DEFAULT
    // for all input types, and not SIGHASH_ALL for non-taproot input types.
    // If neither are provided, use SIGHASH_DEFAULT if it is taproot, and SIGHASH_ALL for everything else.
    if (!sighash) sighash = utxo.scriptPubKey.IsPayToTaproot() ? SIGHASH_DEFAULT : SIGHASH_ALL;
    Assert(sighash.has_value());
    // For user safety, the desired sighash must be provided if the PSMT wants something other than the default set in the previous line.
    if (input.sighash_type && input.sighash_type != sighash) {
        return PSMTError::SIGHASH_MISMATCH;
    }
    // Set the PSMT sighash field when sighash is not DEFAULT or ALL
    // DEFAULT is allowed for non-taproot inputs since DEFAULT may be passed for them (e.g. the psmt being signed also has taproot inputs)
    // Note that signing already aliases DEFAULT to ALL for non-taproot inputs.
    if (utxo.scriptPubKey.IsPayToTaproot() ? sighash != SIGHASH_DEFAULT :
                                            (sighash != SIGHASH_DEFAULT && sighash != SIGHASH_ALL)) {
        input.sighash_type = sighash;
    }

    // Check all existing signatures use the sighash type
    if (sighash == SIGHASH_DEFAULT) {
        if (!input.m_tap_key_sig.empty() && input.m_tap_key_sig.size() != 64) {
            return PSMTError::SIGHASH_MISMATCH;
        }
        for (const auto& [_, sig] : input.m_tap_script_sigs) {
            if (sig.size() != 64) return PSMTError::SIGHASH_MISMATCH;
        }
    } else {
        if (!input.m_tap_key_sig.empty() && (input.m_tap_key_sig.size() != 65 || input.m_tap_key_sig.back() != *sighash)) {
            return PSMTError::SIGHASH_MISMATCH;
        }
        for (const auto& [_, sig] : input.m_tap_script_sigs) {
            if (sig.size() != 65 || sig.back() != *sighash) return PSMTError::SIGHASH_MISMATCH;
        }
        for (const auto& [_, sig] : input.partial_sigs) {
            if (sig.second.back() != *sighash) return PSMTError::SIGHASH_MISMATCH;
        }
    }

    sigdata.witness = false;
    bool sig_complete;
    if (txdata == nullptr) {
        sig_complete = ProduceSignature(provider, DUMMY_SIGNATURE_CREATOR, utxo.scriptPubKey, sigdata);
    } else {
        MutableTransactionSignatureCreator creator(tx, index, utxo.nValue, txdata, *sighash);
        sig_complete = ProduceSignature(provider, creator, utxo.scriptPubKey, sigdata);
    }
    // Verify that a witness signature was produced in case one was required.
    if (require_witness_sig && !sigdata.witness) return PSMTError::INCOMPLETE;

    // If we are not finalizing, set sigdata.complete to false to not set the scriptWitness
    if (!finalize && sigdata.complete) sigdata.complete = false;

    input.FromSignatureData(sigdata);

    // If we have a witness signature, put a witness UTXO.
    if (sigdata.witness) {
        input.witness_utxo = utxo;
        // We can remove the non_witness_utxo if and only if there are no non-segwit or segwit v0
        // inputs in this transaction. Since this requires inspecting the entire transaction, this
        // is something for the caller to deal with (i.e. FillPSMT).
    }

    // Fill in the missing info
    if (out_sigdata) {
        out_sigdata->missing_pubkeys = sigdata.missing_pubkeys;
        out_sigdata->missing_sigs = sigdata.missing_sigs;
        out_sigdata->missing_redeem_script = sigdata.missing_redeem_script;
        out_sigdata->missing_witness_script = sigdata.missing_witness_script;
    }

    return sig_complete ? PSMTError::OK : PSMTError::INCOMPLETE;
}

void RemoveUnnecessaryTransactions(PartiallySignedTransaction& psmtx)
{
    // Figure out if any non_witness_utxos should be dropped
    std::vector<unsigned int> to_drop;
    for (unsigned int i = 0; i < psmtx.inputs.size(); ++i) {
        const auto& input = psmtx.inputs.at(i);
        int wit_ver;
        std::vector<unsigned char> wit_prog;
        if (input.witness_utxo.IsNull() || !input.witness_utxo.scriptPubKey.IsWitnessProgram(wit_ver, wit_prog)) {
            // There's a non-segwit input, so we cannot drop any non_witness_utxos
            to_drop.clear();
            break;
        }
        if (wit_ver == 0) {
            // Segwit v0, so we cannot drop any non_witness_utxos
            to_drop.clear();
            break;
        }
        // non_witness_utxos cannot be dropped if the sighash type includes SIGHASH_ANYONECANPAY
        // Since callers should have called SignPSMTInput which updates the sighash type in the PSMT, we only
        // need to look at that field. If it is not present, then we can assume SIGHASH_DEFAULT or SIGHASH_ALL.
        if (input.sighash_type != std::nullopt && (*input.sighash_type & 0x80) == SIGHASH_ANYONECANPAY) {
            to_drop.clear();
            break;
        }

        if (input.non_witness_utxo) {
            to_drop.push_back(i);
        }
    }

    // Drop the non_witness_utxos that we can drop
    for (unsigned int i : to_drop) {
        psmtx.inputs.at(i).non_witness_utxo = nullptr;
    }
}

bool FinalizePSMT(PartiallySignedTransaction& psmtx)
{
    // Finalize input signatures -- in case we have partial signatures that add up to a complete
    //   signature, but have not combined them yet (e.g. because the combiner that created this
    //   PartiallySignedTransaction did not understand them), this will combine them into a final
    //   script.
    bool complete = true;
    const PrecomputedTransactionData txdata = PrecomputePSMTData(psmtx);
    for (unsigned int i = 0; i < psmtx.tx->vin.size(); ++i) {
        PSMTInput& input = psmtx.inputs.at(i);
        complete &= (SignPSMTInput(DUMMY_SIGNING_PROVIDER, psmtx, i, &txdata, input.sighash_type, nullptr, true) == PSMTError::OK);
    }

    return complete;
}

bool FinalizeAndExtractPSMT(PartiallySignedTransaction& psmtx, CMutableTransaction& result)
{
    // It's not safe to extract a PSMT that isn't finalized, and there's no easy way to check
    //   whether a PSMT is finalized without finalizing it, so we just do this.
    if (!FinalizePSMT(psmtx)) {
        return false;
    }

    result = *psmtx.tx;
    for (unsigned int i = 0; i < result.vin.size(); ++i) {
        result.vin[i].scriptSig = psmtx.inputs[i].final_script_sig;
        result.vin[i].scriptWitness = psmtx.inputs[i].final_script_witness;
    }
    return true;
}

bool CombinePSMTs(PartiallySignedTransaction& out, const std::vector<PartiallySignedTransaction>& psmtxs)
{
    out = psmtxs[0]; // Copy the first one

    // Merge
    for (auto it = std::next(psmtxs.begin()); it != psmtxs.end(); ++it) {
        if (!out.Merge(*it)) {
            return false;
        }
    }
    return true;
}

std::string PSMTRoleName(PSMTRole role) {
    switch (role) {
    case PSMTRole::CREATOR: return "creator";
    case PSMTRole::UPDATER: return "updater";
    case PSMTRole::SIGNER: return "signer";
    case PSMTRole::FINALIZER: return "finalizer";
    case PSMTRole::EXTRACTOR: return "extractor";
        // no default case, so the compiler can warn about missing cases
    }
    assert(false);
}

bool DecodeBase64PSMT(PartiallySignedTransaction& psmt, const std::string& base64_tx, std::string& error)
{
    auto tx_data = DecodeBase64(base64_tx);
    if (!tx_data) {
        error = "invalid base64";
        return false;
    }
    return DecodeRawPSMT(psmt, MakeByteSpan(*tx_data), error);
}

bool DecodeRawPSMT(PartiallySignedTransaction& psmt, std::span<const std::byte> tx_data, std::string& error)
{
    DataStream ss_data{tx_data};
    try {
        ss_data >> psmt;
        if (!ss_data.empty()) {
            error = "extra data after PSMT";
            return false;
        }
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

uint32_t PartiallySignedTransaction::GetVersion() const
{
    if (m_version != std::nullopt) {
        return *m_version;
    }
    return 0;
}
