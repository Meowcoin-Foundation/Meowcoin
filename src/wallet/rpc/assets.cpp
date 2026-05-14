// Copyright (c) 2019-2020 The Raven Core developers
// Copyright (c) 2024-present The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>

#include <assets/assets.h>
#include <assets/assetdb.h>
#include <assets/assettypes.h>
#include <key_io.h>
#include <script/solver.h>
#include <util/result.h>
#include <validation.h>

#include <wallet/asset_tx.h>
#include <wallet/coincontrol.h>
#include <wallet/rpc/util.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

#include <univalue.h>

namespace wallet {

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::string AssetTypeToString(AssetType t)
{
    switch (t) {
    case AssetType::ROOT:          return "ROOT";
    case AssetType::SUB:           return "SUB";
    case AssetType::UNIQUE:        return "UNIQUE";
    case AssetType::OWNER:         return "OWNER";
    case AssetType::MSGCHANNEL:    return "MSGCHANNEL";
    case AssetType::VOTE:          return "VOTE";
    case AssetType::REISSUE:       return "REISSUE";
    case AssetType::QUALIFIER:     return "QUALIFIER";
    case AssetType::SUB_QUALIFIER: return "SUB_QUALIFIER";
    case AssetType::RESTRICTED:    return "RESTRICTED";
    case AssetType::INVALID:       return "INVALID";
    default:                       return "UNKNOWN";
    }
}

static void CheckIPFSOrTxidMessage(const std::string& message, int64_t expireTime)
{
    size_t len = message.length();
    if (len == 46 || len == 64) {
        if (len == 64 && !AreMessagesDeployed())
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid txid hash: RIP5 messaging not yet active");
    } else {
        if (len)
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid IPFS/txid hash (IPFS=46 chars, txid=64 chars)");
    }
    if (!message.empty() && message.substr(0, 2) != "Qm") {
        if (!AreMessagesDeployed())
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid IPFS hash: should start with 'Qm'");
        if (!IsHex(message))
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid IPFS/txid hash: not valid hex");
    }
    if (expireTime < 0)
        throw JSONRPCError(RPC_INVALID_PARAMS, "Expire time must be a positive number");
}

// Returns a to-address from user input, generating a fresh wallet key if empty.
static std::string ResolveOrGenerateAddress(CWallet& wallet, const std::string& input)
{
    if (!input.empty()) {
        if (!IsValidDestinationString(input))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Meowcoin address: " + input);
        return input;
    }
    auto op = wallet.GetNewDestination(wallet.m_default_address_type, "");
    if (!op) throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, util::ErrorString(op).original);
    return EncodeDestination(*op);
}

// ─── issue ───────────────────────────────────────────────────────────────────

RPCHelpMan issue()
{
    return RPCHelpMan{
        "issue",
        "Issue an asset, sub-asset, or unique asset.\n"
        "Asset name must not conflict with any existing asset.\n"
        "Units is the number of decimals precision (0–8). Reissuable controls future supply.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "unique asset name"},
            {"qty",            RPCArg::Type::NUM,  RPCArg::Default{1},      "number of units to issue"},
            {"to_address",     RPCArg::Type::STR,  RPCArg::Default{""}, "address to receive asset; generated if empty"},
            {"change_address", RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change; generated if empty"},
            {"units",          RPCArg::Type::NUM,  RPCArg::Default{0},      "decimal precision 0–8"},
            {"reissuable",     RPCArg::Type::BOOL, RPCArg::Default{true},   "allow future reissuance"},
            {"has_ipfs",       RPCArg::Type::BOOL, RPCArg::Default{false},  "attach IPFS/txid hash"},
            {"ipfs_hash",      RPCArg::Type::STR,  RPCArg::Default{""}, "IPFS hash or txid (required if has_ipfs=true)"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("issue", "\"ASSET_NAME\" 1000")
          + HelpExampleCli("issue", "\"ASSET_NAME\" 1000 \"myaddress\"")
          + HelpExampleCli("issue", "\"ASSET_NAME\" 1000 \"myaddress\" \"changeaddress\" 4")
          + HelpExampleCli("issue", "\"ASSET_NAME\" 1000 \"myaddress\" \"changeaddress\" 8 false true QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E")
          + HelpExampleCli("issue", "\"ASSET_NAME/SUB\" 1000 \"myaddress\" \"changeaddress\" 2 true")
          + HelpExampleCli("issue", "\"ASSET_NAME#uniquetag\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            AssetType assetType;
            std::string assetError;
            if (!IsAssetNameValid(assetName, assetType, assetError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: " + assetName + "\nError: " + assetError);

            if (assetType == AssetType::RESTRICTED)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Use issuerestrictedasset for restricted assets");
            if (assetType == AssetType::QUALIFIER || assetType == AssetType::SUB_QUALIFIER)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Use issuequalifierasset for qualifier assets");
            if (assetType == AssetType::VOTE || assetType == AssetType::REISSUE ||
                assetType == AssetType::OWNER || assetType == AssetType::INVALID)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

            CAmount nAmount      = request.params[1].isNull() ? COIN : AmountFromValue(request.params[1]);
            std::string toAddr   = request.params[2].isNull() ? "" : request.params[2].get_str();
            std::string chgAddr  = request.params[3].isNull() ? "" : request.params[3].get_str();
            int units            = request.params[4].isNull() ? 0  : request.params[4].getInt<int>();
            bool reissuable      = (assetType == AssetType::UNIQUE || assetType == AssetType::MSGCHANNEL ||
                                    assetType == AssetType::QUALIFIER || assetType == AssetType::SUB_QUALIFIER)
                                   ? false : (request.params[5].isNull() ? true : request.params[5].get_bool());
            bool has_ipfs        = !request.params[6].isNull() && request.params[6].get_bool();
            std::string ipfsHash = (has_ipfs && !request.params[7].isNull()) ? request.params[7].get_str() : "";

            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);
            if ((assetType == AssetType::UNIQUE || assetType == AssetType::MSGCHANNEL) &&
                (nAmount != COIN || units != 0 || reissuable))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unique/MsgChannel assets require qty=1, units=0, reissuable=false");
            if ((assetType == AssetType::QUALIFIER || assetType == AssetType::SUB_QUALIFIER) &&
                (nAmount < QUALIFIER_ASSET_MIN_AMOUNT || nAmount > QUALIFIER_ASSET_MAX_AMOUNT || units != 0 || reissuable))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters for qualifier asset (amount 1–10, units=0, reissuable=false)");

            int64_t expireTime = 0;
            if (has_ipfs && !ipfsHash.empty()) CheckIPFSOrTxidMessage(ipfsHash, expireTime);

            toAddr = ResolveOrGenerateAddress(*pwallet, toAddr);

            CNewAsset asset(assetName, nAmount, units, reissuable ? 1 : 0, has_ipfs ? 1 : 0, DecodeAssetData(ipfsHash));
            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateAssetTransaction(*pwallet, ctrl, asset, toAddr, error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── issueunique ─────────────────────────────────────────────────────────────

RPCHelpMan issueunique()
{
    return RPCHelpMan{
        "issueunique",
        "Issue unique asset(s) under an existing root asset you own.\n"
        "One asset is created per element of asset_tags. 5 MEWC is burned per asset.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"root_name",      RPCArg::Type::STR,  RPCArg::Optional::NO,   "root asset name you own"},
            {"asset_tags",     RPCArg::Type::ARR,  RPCArg::Optional::NO,   "unique tags for each asset",
                {{"tag", RPCArg::Type::STR, RPCArg::Optional::NO, "unique tag"}}},
            {"ipfs_hashes",    RPCArg::Type::ARR,  RPCArg::Default{UniValue::VARR}, "optional IPFS hashes, one per tag",
                {{"ipfs_hash", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "IPFS hash"}}},
            {"to_address",     RPCArg::Type::STR,  RPCArg::Default{""}, "address to receive assets; generated if empty"},
            {"change_address", RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change; generated if empty"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("issueunique", "\"MY_ASSET\" '[\"primo\",\"secundo\"]'")
          + HelpExampleCli("issueunique", "\"MY_ASSET\" '[\"primo\",\"secundo\"]' '[\"first_hash\",\"second_hash\"]'")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            const std::string rootName = request.params[0].get_str();
            AssetType assetType;
            std::string assetError;
            if (!IsAssetNameValid(rootName, assetType, assetError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid root asset name: " + rootName + "\nError: " + assetError);
            if (assetType != AssetType::ROOT && assetType != AssetType::SUB)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Root asset must be a top-level or sub-asset");

            const UniValue& assetTags = request.params[1];
            if (!assetTags.isArray() || assetTags.size() < 1)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "asset_tags must be a non-empty array");

            const UniValue& ipfsHashes = request.params[2];
            if (!ipfsHashes.isNull() && ipfsHashes.isArray() && ipfsHashes.size() != assetTags.size())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "If provided, ipfs_hashes must be the same length as asset_tags");

            std::string toAddr  = request.params[3].isNull() ? "" : request.params[3].get_str();
            std::string chgAddr = request.params[4].isNull() ? "" : request.params[4].get_str();

            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

            toAddr = ResolveOrGenerateAddress(*pwallet, toAddr);

            std::vector<CNewAsset> assets;
            for (size_t i = 0; i < assetTags.size(); i++) {
                std::string tag = assetTags[i].get_str();
                if (!IsUniqueTagValid(tag))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid unique asset tag: " + tag);

                std::string assetName = GetUniqueAssetName(rootName, tag);
                bool hasIpfs = !ipfsHashes.isNull() && ipfsHashes.isArray();
                std::string ipfsDecoded = hasIpfs ? DecodeAssetData(ipfsHashes[i].get_str()) : "";
                assets.push_back(CNewAsset(assetName, UNIQUE_ASSET_AMOUNT, UNIQUE_ASSET_UNITS,
                                           UNIQUE_ASSETS_REISSUABLE, hasIpfs ? 1 : 0, ipfsDecoded));
            }

            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateAssetTransaction(*pwallet, ctrl, assets, toAddr, error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── issuequalifierasset ─────────────────────────────────────────────────────

RPCHelpMan issuequalifierasset()
{
    return RPCHelpMan{
        "issuequalifierasset",
        "Issue a qualifier or sub-qualifier asset.\n"
        "If '#' is not prefixed it will be added automatically. Amount must be 1–10.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "unique qualifier asset name"},
            {"qty",            RPCArg::Type::NUM,  RPCArg::Default{1},      "number of units to issue (1–10)"},
            {"to_address",     RPCArg::Type::STR,  RPCArg::Default{""}, "address to receive asset; generated if empty"},
            {"change_address", RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change; generated if empty"},
            {"has_ipfs",       RPCArg::Type::BOOL, RPCArg::Default{false},  "attach IPFS/txid hash"},
            {"ipfs_hash",      RPCArg::Type::STR,  RPCArg::Default{""}, "IPFS hash or txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("issuequalifierasset", "\"#QUALIFIER\" 1")
          + HelpExampleCli("issuequalifierasset", "\"QUALIFIER\" 10 \"myaddress\"")
          + HelpExampleCli("issuequalifierasset", "\"#QUALIFIER\" 1 \"myaddress\" \"changeaddress\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            if (!IsAssetNameAQualifier(assetName))
                assetName = QUALIFIER_CHAR + assetName;

            AssetType assetType;
            std::string assetError;
            if (!IsAssetNameValid(assetName, assetType, assetError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid qualifier asset name: " + assetName + "\nError: " + assetError);
            if (assetType != AssetType::QUALIFIER && assetType != AssetType::SUB_QUALIFIER)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

            CAmount nAmount      = request.params[1].isNull() ? COIN : AmountFromValue(request.params[1]);
            std::string toAddr   = request.params[2].isNull() ? "" : request.params[2].get_str();
            std::string chgAddr  = request.params[3].isNull() ? "" : request.params[3].get_str();
            bool has_ipfs        = !request.params[4].isNull() && request.params[4].get_bool();
            std::string ipfsHash = (has_ipfs && !request.params[5].isNull()) ? request.params[5].get_str() : "";

            if (nAmount < QUALIFIER_ASSET_MIN_AMOUNT || nAmount > QUALIFIER_ASSET_MAX_AMOUNT)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Qualifier asset amount must be between 1 and 10");
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

            int64_t expireTime = 0;
            if (has_ipfs && !ipfsHash.empty()) CheckIPFSOrTxidMessage(ipfsHash, expireTime);

            toAddr = ResolveOrGenerateAddress(*pwallet, toAddr);

            CNewAsset asset(assetName, nAmount, 0, 0, has_ipfs ? 1 : 0, DecodeAssetData(ipfsHash));
            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateAssetTransaction(*pwallet, ctrl, asset, toAddr, error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── issuerestrictedasset ────────────────────────────────────────────────────

RPCHelpMan issuerestrictedasset()
{
    return RPCHelpMan{
        "issuerestrictedasset",
        "Issue a restricted asset.\n"
        "If '$' is not prefixed it will be added automatically.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "unique restricted asset name (starts with '$')"},
            {"qty",            RPCArg::Type::NUM,  RPCArg::Optional::NO,   "quantity to issue"},
            {"verifier",       RPCArg::Type::STR,  RPCArg::Optional::NO,   "verifier string evaluated on restricted transfers"},
            {"to_address",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "address to receive asset; must satisfy verifier"},
            {"change_address", RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change; generated if empty"},
            {"units",          RPCArg::Type::NUM,  RPCArg::Default{0},      "decimal precision 0–8"},
            {"reissuable",     RPCArg::Type::BOOL, RPCArg::Default{true},   "allow future reissuance"},
            {"has_ipfs",       RPCArg::Type::BOOL, RPCArg::Default{false},  "attach IPFS/txid hash"},
            {"ipfs_hash",      RPCArg::Type::STR,  RPCArg::Default{""}, "IPFS hash or txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("issuerestrictedasset", "\"$ASSET\" 1000 \"#KYC & !#AML\" \"myaddress\"")
          + HelpExampleCli("issuerestrictedasset", "\"$ASSET\" 1000 \"#KYC & !#AML\" \"myaddress\" \"changeaddress\" 0 false true QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            if (!IsAssetNameAnRestricted(assetName))
                assetName = RESTRICTED_CHAR + assetName;

            AssetType assetType;
            std::string assetError;
            if (!IsAssetNameValid(assetName, assetType, assetError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: " + assetName + "\nError: " + assetError);
            if (assetType != AssetType::RESTRICTED)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

            CAmount nAmount         = AmountFromValue(request.params[1]);
            std::string verifier    = request.params[2].get_str();
            std::string toAddr      = request.params[3].get_str();
            std::string chgAddr     = request.params[4].isNull() ? "" : request.params[4].get_str();
            int units               = request.params[5].isNull() ? 0    : request.params[5].getInt<int>();
            bool reissuable         = request.params[6].isNull() ? true : request.params[6].get_bool();
            bool has_ipfs           = !request.params[7].isNull() && request.params[7].get_bool();
            std::string ipfsHash    = (has_ipfs && !request.params[8].isNull()) ? request.params[8].get_str() : "";

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);
            if (units < MIN_UNIT || units > MAX_UNIT)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Units must be between 0 and 8");

            std::string verifierStripped = GetStrippedVerifierString(verifier);
            std::string strError;
            if (!ContextualCheckVerifierString(passets, verifierStripped, toAddr, strError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, strError);

            int64_t expireTime = 0;
            if (has_ipfs && !ipfsHash.empty()) CheckIPFSOrTxidMessage(ipfsHash, expireTime);

            CNewAsset asset(assetName, nAmount, units, reissuable ? 1 : 0, has_ipfs ? 1 : 0, DecodeAssetData(ipfsHash));
            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateAssetTransaction(*pwallet, ctrl, asset, toAddr, error, tx, nFeeRequired, &verifierStripped))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── transfer ────────────────────────────────────────────────────────────────

RPCHelpMan transfer()
{
    return RPCHelpMan{
        "transfer",
        "Transfer a quantity of an asset owned by this wallet to a given address.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",          RPCArg::Type::STR,  RPCArg::Optional::NO,   "name of asset to transfer"},
            {"qty",                 RPCArg::Type::NUM,  RPCArg::Optional::NO,   "amount to send"},
            {"to_address",          RPCArg::Type::STR,  RPCArg::Optional::NO,   "destination address"},
            {"message",             RPCArg::Type::STR,  RPCArg::Default{""}, "optional IPFS/txid message (RIP5)"},
            {"expire_time",         RPCArg::Type::NUM,  RPCArg::Default{0},      "UTC timestamp when message expires"},
            {"change_address",      RPCArg::Type::STR,  RPCArg::Default{""}, "MEWC change address"},
            {"asset_change_address",RPCArg::Type::STR,  RPCArg::Default{""}, "asset change address"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("transfer", "\"ASSET_NAME\" 20 \"to_address\"")
          + HelpExampleCli("transfer", "\"ASSET_NAME\" 20 \"to_address\" \"\" \"QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E\" 15863654")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            if (IsAssetNameAQualifier(assetName))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Use transferqualifier to send qualifier assets");

            CAmount nAmount      = AmountFromValue(request.params[1]);
            std::string toAddr   = request.params[2].get_str();
            std::string message  = request.params[3].isNull() ? "" : request.params[3].get_str();
            int64_t expireTime   = (request.params[4].isNull() || message.empty()) ? 0 : request.params[4].getInt<int64_t>();
            std::string chgAddr  = request.params[5].isNull() ? "" : request.params[5].get_str();
            std::string assetChg = request.params[6].isNull() ? "" : request.params[6].get_str();

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid MEWC change address: " + chgAddr);
            if (!assetChg.empty() && !IsValidDestinationString(assetChg))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid asset change address: " + assetChg);

            if (!message.empty()) {
                if (!AreMessagesDeployed())
                    throw JSONRPCError(RPC_INVALID_PARAMS, "Unable to send messages until RIP5 is enabled");
                CheckIPFSOrTxidMessage(message, expireTime);
            }

            std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
            vTransfers.emplace_back(CAssetTransfer(assetName, nAmount, DecodeAssetData(message), expireTime), toAddr);

            CCoinControl ctrl;
            ctrl.destChange      = DecodeDestination(chgAddr);
            ctrl.destAssetChange = DecodeDestination(assetChg);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── transferfromaddresses ───────────────────────────────────────────────────

RPCHelpMan transferfromaddresses()
{
    return RPCHelpMan{
        "transferfromaddresses",
        "Transfer asset from specific addresses in this wallet to a given address.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",           RPCArg::Type::STR,  RPCArg::Optional::NO, "name of asset to transfer"},
            {"from_addresses",       RPCArg::Type::ARR,  RPCArg::Optional::NO, "list of source addresses",
                {{"address", RPCArg::Type::STR, RPCArg::Optional::NO, "address"}}},
            {"qty",                  RPCArg::Type::NUM,  RPCArg::Optional::NO, "amount to send"},
            {"to_address",           RPCArg::Type::STR,  RPCArg::Optional::NO, "destination address"},
            {"message",              RPCArg::Type::STR,  RPCArg::Default{""},  "optional IPFS/txid message (RIP5)"},
            {"expire_time",          RPCArg::Type::NUM,  RPCArg::Default{0},   "UTC timestamp when message expires"},
            {"mewc_change_address",  RPCArg::Type::STR,  RPCArg::Default{""},  "MEWC change address"},
            {"asset_change_address", RPCArg::Type::STR,  RPCArg::Default{""},  "asset change address"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("transferfromaddresses", "\"ASSET\" '[\"addr1\",\"addr2\"]' 20 \"to_address\"")
          + HelpExampleRpc("transferfromaddresses", "\"ASSET\", '[\"addr1\",\"addr2\"]', 20, \"to_address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName    = request.params[0].get_str();
            const UniValue& fromArr  = request.params[1];
            if (!fromArr.isArray() || fromArr.size() < 1)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "from_addresses must be a non-empty array");

            std::set<std::string> setFrom;
            for (size_t i = 0; i < fromArr.size(); i++) {
                std::string addr = fromArr[i].get_str();
                if (!IsValidDestinationString(addr))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid from address: " + addr);
                setFrom.insert(addr);
            }

            CAmount nAmount      = AmountFromValue(request.params[2]);
            std::string toAddr   = request.params[3].get_str();
            std::string message  = request.params[4].isNull() ? "" : request.params[4].get_str();
            int64_t expireTime   = (request.params[5].isNull() || message.empty()) ? 0 : request.params[5].getInt<int64_t>();
            std::string chgAddr  = request.params[6].isNull() ? "" : request.params[6].get_str();
            std::string assetChg = request.params[7].isNull() ? "" : request.params[7].get_str();

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid MEWC change address: " + chgAddr);
            if (!assetChg.empty() && !IsValidDestinationString(assetChg))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid asset change address: " + assetChg);
            if (!message.empty()) CheckIPFSOrTxidMessage(message, expireTime);

            // Select UTXOs at the from_addresses
            CCoinControl ctrl;
            ctrl.strAssetSelected = assetName;
            {
                LOCK(pwallet->cs_wallet);
                CoinFilterParams coin_params;
                coin_params.min_amount = 0;
                CoinsResult available = AvailableCoinsWithAssets(*pwallet, nullptr, std::nullopt, coin_params);
                if (!available.mapAssetCoins.count(assetName))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Wallet doesn't own asset: " + assetName);
                for (const auto& out : available.mapAssetCoins.at(assetName)) {
                    CTxDestination dest;
                    ExtractDestination(out.txout.scriptPubKey, dest);
                    if (setFrom.count(EncodeDestination(dest)))
                        ctrl.SelectAsset(out.outpoint);
                }
            }
            if (!ctrl.HasAssetSelected())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "No asset UTXOs found at the given from addresses");

            ctrl.destChange      = DecodeDestination(chgAddr);
            ctrl.destAssetChange = DecodeDestination(assetChg);

            std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
            vTransfers.emplace_back(CAssetTransfer(assetName, nAmount, DecodeAssetData(message), expireTime), toAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── transferfromaddress ─────────────────────────────────────────────────────

RPCHelpMan transferfromaddress()
{
    return RPCHelpMan{
        "transferfromaddress",
        "Transfer asset from a specific address in this wallet to a given address.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",           RPCArg::Type::STR,  RPCArg::Optional::NO, "name of asset to transfer"},
            {"from_address",         RPCArg::Type::STR,  RPCArg::Optional::NO, "source address"},
            {"qty",                  RPCArg::Type::NUM,  RPCArg::Optional::NO, "amount to send"},
            {"to_address",           RPCArg::Type::STR,  RPCArg::Optional::NO, "destination address"},
            {"message",              RPCArg::Type::STR,  RPCArg::Default{""},  "optional IPFS/txid message (RIP5)"},
            {"expire_time",          RPCArg::Type::NUM,  RPCArg::Default{0},   "UTC timestamp when message expires"},
            {"mewc_change_address",  RPCArg::Type::STR,  RPCArg::Default{""},  "MEWC change address"},
            {"asset_change_address", RPCArg::Type::STR,  RPCArg::Default{""},  "asset change address"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("transferfromaddress", "\"ASSET\" \"fromaddr\" 20 \"toaddr\"")
          + HelpExampleRpc("transferfromaddress", "\"ASSET\", \"fromaddr\", 20, \"toaddr\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName   = request.params[0].get_str();
            std::string fromAddr    = request.params[1].get_str();
            CAmount nAmount         = AmountFromValue(request.params[2]);
            std::string toAddr      = request.params[3].get_str();
            std::string message     = request.params[4].isNull() ? "" : request.params[4].get_str();
            int64_t expireTime      = (request.params[5].isNull() || message.empty()) ? 0 : request.params[5].getInt<int64_t>();
            std::string chgAddr     = request.params[6].isNull() ? "" : request.params[6].get_str();
            std::string assetChg    = request.params[7].isNull() ? "" : request.params[7].get_str();

            if (!IsValidDestinationString(fromAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from_address: " + fromAddr);
            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid MEWC change address: " + chgAddr);
            if (!assetChg.empty() && !IsValidDestinationString(assetChg))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid asset change address: " + assetChg);
            if (!message.empty()) CheckIPFSOrTxidMessage(message, expireTime);

            CCoinControl ctrl;
            ctrl.strAssetSelected = assetName;
            {
                LOCK(pwallet->cs_wallet);
                CoinFilterParams coin_params;
                coin_params.min_amount = 0;
                CoinsResult available = AvailableCoinsWithAssets(*pwallet, nullptr, std::nullopt, coin_params);
                if (!available.mapAssetCoins.count(assetName))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Wallet doesn't own asset: " + assetName);
                for (const auto& out : available.mapAssetCoins.at(assetName)) {
                    CTxDestination dest;
                    ExtractDestination(out.txout.scriptPubKey, dest);
                    if (fromAddr == EncodeDestination(dest))
                        ctrl.SelectAsset(out.outpoint);
                }
            }
            if (!ctrl.HasAssetSelected())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "No asset UTXOs found at from_address: " + fromAddr);

            ctrl.destChange      = DecodeDestination(chgAddr);
            ctrl.destAssetChange = DecodeDestination(assetChg);

            std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
            vTransfers.emplace_back(CAssetTransfer(assetName, nAmount, DecodeAssetData(message), expireTime), toAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── transferqualifier ───────────────────────────────────────────────────────

RPCHelpMan transferqualifier()
{
    return RPCHelpMan{
        "transferqualifier",
        "Transfer a qualifier asset owned by this wallet to a given address.\n"
        "Only use this call for qualifier assets (names starting with '#').\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"qualifier_name",  RPCArg::Type::STR,  RPCArg::Optional::NO,  "name of qualifier asset"},
            {"qty",             RPCArg::Type::NUM,  RPCArg::Optional::NO,  "amount to send"},
            {"to_address",      RPCArg::Type::STR,  RPCArg::Optional::NO,  "destination address"},
            {"change_address",  RPCArg::Type::STR,  RPCArg::Default{""},   "change address"},
            {"message",         RPCArg::Type::STR,  RPCArg::Default{""},   "optional IPFS/txid message (RIP5)"},
            {"expire_time",     RPCArg::Type::NUM,  RPCArg::Default{0},    "UTC timestamp when message expires"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("transferqualifier", "\"#QUALIFIER\" 1 \"to_address\"")
          + HelpExampleCli("transferqualifier", "\"#QUALIFIER\" 1 \"to_address\" \"change_address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            if (!IsAssetNameAQualifier(assetName))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Only qualifier assets (starting with '#') may use transferqualifier");

            CAmount nAmount     = AmountFromValue(request.params[1]);
            std::string toAddr  = request.params[2].get_str();
            std::string chgAddr = request.params[3].isNull() ? "" : request.params[3].get_str();
            std::string message = request.params[4].isNull() ? "" : request.params[4].get_str();
            int64_t expireTime  = (request.params[5].isNull() || message.empty()) ? 0 : request.params[5].getInt<int64_t>();

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);
            if (!message.empty()) {
                if (!AreMessagesDeployed())
                    throw JSONRPCError(RPC_INVALID_PARAMS, "Unable to send messages until RIP5 is enabled");
                CheckIPFSOrTxidMessage(message, expireTime);
            }

            std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
            vTransfers.emplace_back(CAssetTransfer(assetName, nAmount, DecodeAssetData(message), expireTime), toAddr);

            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── reissue ─────────────────────────────────────────────────────────────────

RPCHelpMan reissue()
{
    return RPCHelpMan{
        "reissue",
        "Reissue additional quantity of an asset you own the owner token for.\n"
        "Can change reissuable flag and IPFS hash during reissuance.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "name of asset to reissue"},
            {"qty",            RPCArg::Type::NUM,  RPCArg::Optional::NO,   "additional units to issue"},
            {"to_address",     RPCArg::Type::STR,  RPCArg::Optional::NO,   "address to receive reissued units"},
            {"change_address", RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change"},
            {"reissuable",     RPCArg::Type::BOOL, RPCArg::Default{true},  "allow further reissuance after this"},
            {"new_units",      RPCArg::Type::NUM,  RPCArg::Default{-1},    "new unit precision (-1 = unchanged)"},
            {"new_ipfs",       RPCArg::Type::STR,  RPCArg::Default{""}, "update IPFS/txid hash (empty = unchanged)"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("reissue", "\"ASSET_NAME\" 20 \"address\"")
          + HelpExampleRpc("reissue", "\"ASSET_NAME\" 20 \"address\" \"change\" true 8 \"Qmd286K6pohQcTKYqnS1YhWrCiS4gz7Xi34sdwMe9USZ7u\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName  = request.params[0].get_str();
            CAmount nAmount        = AmountFromValue(request.params[1]);
            std::string toAddr     = request.params[2].get_str();
            std::string chgAddr    = request.params[3].isNull() ? "" : request.params[3].get_str();
            bool reissuable        = request.params[4].isNull() ? true : request.params[4].get_bool();
            int newUnits           = request.params[5].isNull() ? -1  : request.params[5].getInt<int>();
            std::string newIpfs    = request.params[6].isNull() ? ""  : request.params[6].get_str();

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

            int64_t expireTime = 0;
            if (!newIpfs.empty()) CheckIPFSOrTxidMessage(newIpfs, expireTime);

            CReissueAsset reissueAsset(assetName, nAmount, newUnits, reissuable ? 1 : 0, DecodeAssetData(newIpfs), "");
            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateReissueAssetTransaction(*pwallet, ctrl, reissueAsset, toAddr, error, tx, nFeeRequired))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── reissuerestrictedasset ──────────────────────────────────────────────────

RPCHelpMan reissuerestrictedasset()
{
    return RPCHelpMan{
        "reissuerestrictedasset",
        "Reissue additional quantity of a restricted asset you own the owner token for.\n"
        "Can change reissuable flag, verifier string, and IPFS hash.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",      RPCArg::Type::STR,  RPCArg::Optional::NO,   "restricted asset name (starts with '$')"},
            {"qty",             RPCArg::Type::NUM,  RPCArg::Optional::NO,   "additional units to issue"},
            {"to_address",      RPCArg::Type::STR,  RPCArg::Optional::NO,   "address to receive reissued units"},
            {"change_verifier", RPCArg::Type::BOOL, RPCArg::Default{false}, "whether to update the verifier string"},
            {"new_verifier",    RPCArg::Type::STR,  RPCArg::Default{""}, "new verifier string (if change_verifier=true)"},
            {"change_address",  RPCArg::Type::STR,  RPCArg::Default{""}, "address for MEWC change; generated if empty"},
            {"new_units",       RPCArg::Type::NUM,  RPCArg::Default{-1},    "new unit precision (-1 = unchanged)"},
            {"reissuable",      RPCArg::Type::BOOL, RPCArg::Default{true},  "allow further reissuance"},
            {"new_ipfs",        RPCArg::Type::STR,  RPCArg::Default{""}, "update IPFS/txid hash"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("reissuerestrictedasset", "\"$ASSET\" 1000 \"myaddress\" true \"#KYC & !#AML\"")
          + HelpExampleCli("reissuerestrictedasset", "\"$ASSET\" 1000 \"myaddress\" false \"\" \"changeaddress\" -1 false QmTqu3Lk3gmTsQVtjU7rYYM37EAW4xNmbuEAp2Mjr4AV7E")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");

            EnsureWalletIsUnlocked(*pwallet);

            std::string assetName = request.params[0].get_str();
            if (!IsAssetNameAnRestricted(assetName))
                assetName = RESTRICTED_CHAR + assetName;

            AssetType assetType;
            std::string assetError;
            if (!IsAssetNameValid(assetName, assetType, assetError))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: " + assetName + "\nError: " + assetError);
            if (assetType != AssetType::RESTRICTED)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

            CAmount nAmount          = AmountFromValue(request.params[1]);
            std::string toAddr       = request.params[2].get_str();
            bool fChangeVerifier     = !request.params[3].isNull() && request.params[3].get_bool();
            std::string verifier     = request.params[4].isNull() ? "" : request.params[4].get_str();
            std::string chgAddr      = request.params[5].isNull() ? "" : request.params[5].get_str();
            int newUnits             = request.params[6].isNull() ? -1 : request.params[6].getInt<int>();
            bool reissuable          = request.params[7].isNull() ? true : request.params[7].get_bool();
            std::string newIpfs      = request.params[8].isNull() ? "" : request.params[8].get_str();

            if (!IsValidDestinationString(toAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to_address: " + toAddr);
            if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);
            if (newUnits < -1 || newUnits > MAX_UNIT)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Units must be between -1 and 8 (-1 = unchanged)");

            int64_t expireTime = 0;
            if (!newIpfs.empty()) CheckIPFSOrTxidMessage(newIpfs, expireTime);

            std::string verifierStripped = GetStrippedVerifierString(verifier);

            CReissueAsset reissueAsset(assetName, nAmount, newUnits, reissuable ? 1 : 0, DecodeAssetData(newIpfs), "");
            CCoinControl ctrl;
            ctrl.destChange = DecodeDestination(chgAddr);

            CTransactionRef tx;
            CAmount nFeeRequired;
            std::pair<int, std::string> error;
            if (!CreateReissueAssetTransaction(*pwallet, ctrl, reissueAsset, toAddr, error, tx, nFeeRequired,
                                               fChangeVerifier ? &verifierStripped : nullptr))
                throw JSONRPCError(error.first, error.second);

            std::string txid;
            if (!SendAssetTransaction(*pwallet, tx, error, txid))
                throw JSONRPCError(error.first, error.second);

            UniValue result(UniValue::VARR);
            result.push_back(txid);
            return result;
        },
    };
}

// ─── listmyassets ────────────────────────────────────────────────────────────

RPCHelpMan listmyassets()
{
    return RPCHelpMan{
        "listmyassets",
        "List all assets owned by this wallet.\n",
        {
            {"asset",   RPCArg::Type::STR,  RPCArg::Default{"*"},  "filter: asset name or prefix followed by '*'"},
            {"verbose", RPCArg::Type::BOOL, RPCArg::Default{false}, "include outpoint details when true"},
            {"count",   RPCArg::Type::NUM,  RPCArg::Default{INT_MAX}, "maximum number of assets to return"},
            {"start",   RPCArg::Type::NUM,  RPCArg::Default{0},    "skip first N results (negative counts from end)"},
            {"confs",   RPCArg::Type::NUM,  RPCArg::Default{0},    "minimum confirmations required"},
        },
        RPCResult{
            RPCResult::Type::OBJ_DYN, "", "asset balances",
            {{RPCResult::Type::NUM, "asset_name", "balance or object with balance+outpoints when verbose"}}
        },
        RPCExamples{
            HelpExampleRpc("listmyassets", "")
          + HelpExampleCli("listmyassets", "ASSET")
          + HelpExampleCli("listmyassets", "\"ASSET*\" true 10 20")
          + HelpExampleCli("listmyassets", "\"ASSET*\" true 10 20 1")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Asset protocol not yet active");

            std::string filter  = request.params[0].isNull() ? "*" : request.params[0].get_str();
            if (filter.empty()) filter = "*";
            bool verbose        = !request.params[1].isNull() && request.params[1].get_bool();
            size_t count        = (request.params[2].isNull() || request.params[2].getInt<int>() < 1) ? (size_t)INT_MAX : (size_t)request.params[2].getInt<int>();
            long start          = request.params[3].isNull() ? 0 : request.params[3].getInt<int>();
            int confs           = request.params[4].isNull() ? 0 : request.params[4].getInt<int>();

            // Build balance + outpoints maps under cs_wallet
            std::map<std::string, CAmount> balances;
            std::map<std::string, std::vector<std::pair<COutPoint, CAmount>>> outpointsByAsset;
            {
                LOCK(pwallet->cs_wallet);
                CoinFilterParams coin_params;
                coin_params.min_amount = 0;
                CoinsResult available = AvailableCoinsWithAssets(*pwallet, nullptr, std::nullopt, coin_params);

                for (const auto& [assetName, outputs] : available.mapAssetCoins) {
                    // Apply filter
                    if (filter != "*") {
                        std::string prefix = filter;
                        bool isWild = !prefix.empty() && prefix.back() == '*';
                        if (isWild) prefix.pop_back();
                        if (isWild && assetName.find(prefix) != 0) continue;
                        if (!isWild && assetName != filter) continue;
                    }

                    for (const auto& out : outputs) {
                        if (out.depth < confs) continue;
                        CAssetOutputEntry data;
                        if (!GetAssetData(out.txout.scriptPubKey, data)) continue;
                        balances[assetName] += data.nAmount;
                        if (verbose)
                            outpointsByAsset[assetName].emplace_back(out.outpoint, data.nAmount);
                    }
                }
            }

            // Pagination
            auto bal = balances.begin();
            if (start >= 0) {
                size_t skip = (size_t)start;
                for (size_t i = 0; i < skip && bal != balances.end(); i++) ++bal;
            } else {
                size_t skip = (size_t)std::max((long)balances.size() + start, 0L);
                for (size_t i = 0; i < skip && bal != balances.end(); i++) ++bal;
            }

            UniValue result(UniValue::VOBJ);
            size_t n = 0;
            for (; bal != balances.end() && n < count; ++bal, ++n) {
                if (verbose) {
                    UniValue assetObj(UniValue::VOBJ);
                    assetObj.pushKV("balance", AssetUnitValueFromAmount(bal->second, bal->first));
                    UniValue outpoints(UniValue::VARR);
                    for (const auto& [op, amt] : outpointsByAsset[bal->first]) {
                        UniValue tempOut(UniValue::VOBJ);
                        tempOut.pushKV("txid", op.hash.GetHex());
                        tempOut.pushKV("vout", (int)op.n);
                        tempOut.pushKV("amount", AssetUnitValueFromAmount(amt, bal->first));
                        outpoints.push_back(tempOut);
                    }
                    assetObj.pushKV("outpoints", outpoints);
                    result.pushKV(bal->first, assetObj);
                } else {
                    result.pushKV(bal->first, AssetUnitValueFromAmount(bal->second, bal->first));
                }
            }
            return result;
        },
    };
}

// ─── restricted / qualifier tag helpers ──────────────────────────────────────

static UniValue DoUpdateAddressTag(const std::shared_ptr<CWallet>& pwallet,
                                   const JSONRPCRequest& request, const int8_t flag)
{
    EnsureWalletIsUnlocked(*pwallet);

    std::string tagName = request.params[0].get_str();
    if (!IsAssetNameAQualifier(tagName)) {
        std::string temp = QUALIFIER_CHAR + tagName;
        auto idx = temp.find('/');
        if (idx != std::string::npos) temp.insert(idx + 1, "#");
        tagName = temp;
    }

    AssetType assetType;
    std::string assetError;
    if (!IsAssetNameValid(tagName, assetType, assetError))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid tag name: " + tagName + "\nError: " + assetError);
    if (assetType != AssetType::QUALIFIER && assetType != AssetType::SUB_QUALIFIER)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

    std::string address = request.params[1].get_str();
    if (!IsValidDestinationString(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Meowcoin address: " + address);

    std::string chgAddr = request.params[2].isNull() ? "" : request.params[2].get_str();
    if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

    std::string assetData = request.params[3].isNull() ? "" : DecodeAssetData(request.params[3].get_str());
    if (!request.params[3].isNull() && !request.params[3].get_str().empty() && assetData.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset data hash");

    // Transfer 1 qualifier token to change address (must be self-transfer)
    // If no change address given, use the first wallet address
    if (chgAddr.empty()) {
        auto op = pwallet->GetNewDestination(pwallet->m_default_address_type, "");
        if (!op) throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, util::ErrorString(op).original);
        chgAddr = EncodeDestination(*op);
    }

    std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
    vTransfers.emplace_back(CAssetTransfer(tagName, 1 * COIN, assetData), chgAddr);

    std::vector<std::pair<CNullAssetTxData, std::string>> vecAssetData;
    vecAssetData.emplace_back(CNullAssetTxData(tagName, flag), address);

    CCoinControl ctrl;
    ctrl.destChange = DecodeDestination(chgAddr);

    CTransactionRef tx;
    CAmount nFeeRequired;
    std::pair<int, std::string> error;
    if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired, &vecAssetData))
        throw JSONRPCError(error.first, error.second);

    std::string txid;
    if (!SendAssetTransaction(*pwallet, tx, error, txid))
        throw JSONRPCError(error.first, error.second);

    UniValue result(UniValue::VARR);
    result.push_back(txid);
    return result;
}

static UniValue DoUpdateAddressRestriction(const std::shared_ptr<CWallet>& pwallet,
                                           const JSONRPCRequest& request, const int8_t flag)
{
    EnsureWalletIsUnlocked(*pwallet);

    std::string restrictedName = request.params[0].get_str();
    if (!IsAssetNameAnRestricted(restrictedName))
        restrictedName = RESTRICTED_CHAR + restrictedName;

    AssetType assetType;
    std::string assetError;
    if (!IsAssetNameValid(restrictedName, assetType, assetError))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: " + restrictedName + "\nError: " + assetError);
    if (assetType != AssetType::RESTRICTED)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

    std::string address = request.params[1].get_str();
    if (!IsValidDestinationString(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Meowcoin address: " + address);

    std::string chgAddr = request.params[2].isNull() ? "" : request.params[2].get_str();
    if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

    std::string assetData = request.params[3].isNull() ? "" : DecodeAssetData(request.params[3].get_str());
    if (!request.params[3].isNull() && !request.params[3].get_str().empty() && assetData.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset data hash");

    if (chgAddr.empty()) {
        auto op = pwallet->GetNewDestination(pwallet->m_default_address_type, "");
        if (!op) throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, util::ErrorString(op).original);
        chgAddr = EncodeDestination(*op);
    }

    // Transfer 1 owner token back to ourselves
    std::string ownerToken = restrictedName.substr(1) + OWNER_TAG;
    std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
    vTransfers.emplace_back(CAssetTransfer(ownerToken, 1 * COIN, assetData), chgAddr);

    std::vector<std::pair<CNullAssetTxData, std::string>> vecAssetData;
    vecAssetData.emplace_back(CNullAssetTxData(restrictedName, flag), address);

    CCoinControl ctrl;
    ctrl.destChange = DecodeDestination(chgAddr);

    CTransactionRef tx;
    CAmount nFeeRequired;
    std::pair<int, std::string> error;
    if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired, &vecAssetData))
        throw JSONRPCError(error.first, error.second);

    std::string txid;
    if (!SendAssetTransaction(*pwallet, tx, error, txid))
        throw JSONRPCError(error.first, error.second);

    UniValue result(UniValue::VARR);
    result.push_back(txid);
    return result;
}

static UniValue DoUpdateGlobalRestrictedAsset(const std::shared_ptr<CWallet>& pwallet,
                                              const JSONRPCRequest& request, const int8_t flag)
{
    EnsureWalletIsUnlocked(*pwallet);

    std::string restrictedName = request.params[0].get_str();
    AssetType assetType;
    std::string assetError;
    if (!IsAssetNameValid(restrictedName, assetType, assetError))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: " + restrictedName + "\nError: " + assetError);
    if (assetType != AssetType::RESTRICTED)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported asset type: " + AssetTypeToString(assetType));

    std::string chgAddr = request.params[1].isNull() ? "" : request.params[1].get_str();
    if (!chgAddr.empty() && !IsValidDestinationString(chgAddr))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address: " + chgAddr);

    std::string assetData = request.params[2].isNull() ? "" : DecodeAssetData(request.params[2].get_str());
    if (!request.params[2].isNull() && !request.params[2].get_str().empty() && assetData.empty())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset data hash");

    if (chgAddr.empty()) {
        auto op = pwallet->GetNewDestination(pwallet->m_default_address_type, "");
        if (!op) throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, util::ErrorString(op).original);
        chgAddr = EncodeDestination(*op);
    }

    std::string ownerToken = restrictedName.substr(1) + OWNER_TAG;
    std::vector<std::pair<CAssetTransfer, std::string>> vTransfers;
    vTransfers.emplace_back(CAssetTransfer(ownerToken, 1 * COIN, assetData), chgAddr);

    std::vector<CNullAssetTxData> vecGlobalData;
    vecGlobalData.emplace_back(CNullAssetTxData(restrictedName, flag));

    CCoinControl ctrl;
    ctrl.destChange = DecodeDestination(chgAddr);

    CTransactionRef tx;
    CAmount nFeeRequired;
    std::pair<int, std::string> error;
    if (!CreateTransferAssetTransaction(*pwallet, ctrl, vTransfers, "", error, tx, nFeeRequired, nullptr, &vecGlobalData))
        throw JSONRPCError(error.first, error.second);

    std::string txid;
    if (!SendAssetTransaction(*pwallet, tx, error, txid))
        throw JSONRPCError(error.first, error.second);

    UniValue result(UniValue::VARR);
    result.push_back(txid);
    return result;
}

// ─── addtagtoaddress / removetagfromaddress ───────────────────────────────────

RPCHelpMan addtagtoaddress()
{
    return RPCHelpMan{
        "addtagtoaddress",
        "Assign a qualifier tag to an address.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"tag_name",       RPCArg::Type::STR, RPCArg::Optional::NO,  "qualifier asset name (e.g. #TAG)"},
            {"to_address",     RPCArg::Type::STR, RPCArg::Optional::NO,  "address to tag"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for qualifier token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid attached to the qualifier transfer"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("addtagtoaddress", "\"#TAG\" \"to_address\"")
          + HelpExampleRpc("addtagtoaddress", "\"#TAG\", \"to_address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateAddressTag(pwallet, request, 1);
        },
    };
}

RPCHelpMan removetagfromaddress()
{
    return RPCHelpMan{
        "removetagfromaddress",
        "Remove a qualifier tag from an address.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"tag_name",       RPCArg::Type::STR, RPCArg::Optional::NO,  "qualifier asset name to remove"},
            {"to_address",     RPCArg::Type::STR, RPCArg::Optional::NO,  "address to remove tag from"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for qualifier token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("removetagfromaddress", "\"#TAG\" \"to_address\"")
          + HelpExampleRpc("removetagfromaddress", "\"#TAG\", \"to_address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateAddressTag(pwallet, request, 0);
        },
    };
}

// ─── freezeaddress / unfreezeaddress ─────────────────────────────────────────

RPCHelpMan freezeaddress()
{
    return RPCHelpMan{
        "freezeaddress",
        "Prevent an address from transferring a restricted asset.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR, RPCArg::Optional::NO,  "restricted asset name (e.g. $ASSET)"},
            {"address",        RPCArg::Type::STR, RPCArg::Optional::NO,  "address to freeze"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for owner token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("freezeaddress", "\"$RESTRICTED_ASSET\" \"address\"")
          + HelpExampleRpc("freezeaddress", "\"$RESTRICTED_ASSET\", \"address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateAddressRestriction(pwallet, request, 1);
        },
    };
}

RPCHelpMan unfreezeaddress()
{
    return RPCHelpMan{
        "unfreezeaddress",
        "Re-allow an address to transfer a restricted asset.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR, RPCArg::Optional::NO,  "restricted asset name (e.g. $ASSET)"},
            {"address",        RPCArg::Type::STR, RPCArg::Optional::NO,  "address to unfreeze"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for owner token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("unfreezeaddress", "\"$RESTRICTED_ASSET\" \"address\"")
          + HelpExampleRpc("unfreezeaddress", "\"$RESTRICTED_ASSET\", \"address\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateAddressRestriction(pwallet, request, 0);
        },
    };
}

// ─── freezerestrictedasset / unfreezerestrictedasset ─────────────────────────

RPCHelpMan freezerestrictedasset()
{
    return RPCHelpMan{
        "freezerestrictedasset",
        "Freeze all transfers of a restricted asset globally.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR, RPCArg::Optional::NO,  "restricted asset name (e.g. $ASSET)"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for owner token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("freezerestrictedasset", "\"$RESTRICTED_ASSET\"")
          + HelpExampleRpc("freezerestrictedasset", "\"$RESTRICTED_ASSET\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateGlobalRestrictedAsset(pwallet, request, 1);
        },
    };
}

RPCHelpMan unfreezerestrictedasset()
{
    return RPCHelpMan{
        "unfreezerestrictedasset",
        "Unfreeze all transfers of a restricted asset globally.\n"
        "Requires wallet passphrase to be set with walletpassphrase if encrypted.\n",
        {
            {"asset_name",     RPCArg::Type::STR, RPCArg::Optional::NO,  "restricted asset name (e.g. $ASSET)"},
            {"change_address", RPCArg::Type::STR, RPCArg::Default{""}, "change address for owner token"},
            {"asset_data",     RPCArg::Type::STR, RPCArg::Default{""}, "optional IPFS/txid"},
        },
        RPCResult{RPCResult::Type::ARR, "", "list of transaction IDs",
            {{RPCResult::Type::STR_HEX, "", "txid"}}},
        RPCExamples{
            HelpExampleCli("unfreezerestrictedasset", "\"$RESTRICTED_ASSET\"")
          + HelpExampleRpc("unfreezerestrictedasset", "\"$RESTRICTED_ASSET\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet not found");
            if (!AreRestrictedAssetsDeployed())
                throw JSONRPCError(RPC_INVALID_REQUEST, "Restricted asset protocol not yet active");
            return DoUpdateGlobalRestrictedAsset(pwallet, request, 0);
        },
    };
}

} // namespace wallet
