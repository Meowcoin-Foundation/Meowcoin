// Copyright (c) 2009-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_PSMT_H
#define BITCOIN_NODE_PSMT_H

#include <psmt.h>

#include <optional>

namespace node {
/**
 * Holds an analysis of one input from a PSMT
 */
struct PSMTInputAnalysis {
    bool has_utxo; //!< Whether we have UTXO information for this input
    bool is_final; //!< Whether the input has all required information including signatures
    PSMTRole next; //!< Which of the BIP 174 roles needs to handle this input next

    std::vector<CKeyID> missing_pubkeys; //!< Pubkeys whose BIP32 derivation path is missing
    std::vector<CKeyID> missing_sigs;    //!< Pubkeys whose signatures are missing
    uint160 missing_redeem_script;       //!< Hash160 of redeem script, if missing
    uint256 missing_witness_script;      //!< SHA256 of witness script, if missing
};

/**
 * Holds the results of AnalyzePSMT (miscellaneous information about a PSMT)
 */
struct PSMTAnalysis {
    std::optional<size_t> estimated_vsize;      //!< Estimated weight of the transaction
    std::optional<CFeeRate> estimated_feerate;  //!< Estimated feerate (fee / weight) of the transaction
    std::optional<CAmount> fee;                 //!< Amount of fee being paid by the transaction
    std::vector<PSMTInputAnalysis> inputs;      //!< More information about the individual inputs of the transaction
    PSMTRole next;                              //!< Which of the BIP 174 roles needs to handle the transaction next
    std::string error;                          //!< Error message

    void SetInvalid(std::string err_msg)
    {
        estimated_vsize = std::nullopt;
        estimated_feerate = std::nullopt;
        fee = std::nullopt;
        inputs.clear();
        next = PSMTRole::CREATOR;
        error = err_msg;
    }
};

/**
 * Provides helpful miscellaneous information about where a PSMT is in the signing workflow.
 *
 * @param[in] psmtx the PSMT to analyze
 * @return A PSMTAnalysis with information about the provided PSMT.
 */
PSMTAnalysis AnalyzePSMT(PartiallySignedTransaction psmtx);
} // namespace node

#endif // BITCOIN_NODE_PSMT_H
