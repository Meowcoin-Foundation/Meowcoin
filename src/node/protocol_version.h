// Copyright (c) 2012-present The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_PROTOCOL_VERSION_H
#define BITCOIN_NODE_PROTOCOL_VERSION_H

/**
 * network protocol versioning
 */

static const int PROTOCOL_VERSION = 70031;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! disconnect from peers older than this proto version
//! Meowcoin: require at least MEOWPOW_VERSION (70030)
static const int MIN_PEER_PROTO_VERSION = 70030;

//! BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

//! "sendheaders" command and announcing blocks with headers starts with this version
static const int SENDHEADERS_VERSION = 70012;

//! "feefilter" tells peers to filter invs to you by fee starts with this version
static const int FEEFILTER_VERSION = 70013;

//! short-id-based block download starts with this version
static const int SHORT_IDS_BLOCKS_VERSION = 70014;

//! not banning for invalid compact blocks starts with this version
static const int INVALID_CB_NO_BAN_VERSION = 70015;

//! "wtxidrelay" command for wtxid-based relay starts with this version
static const int WTXID_RELAY_VERSION = 70016;

//! Meowcoin: X16RV2 activation protocol version
static const int X16RV2_VERSION = 70025;

//! Meowcoin: KAWPOW activation protocol version
static const int KAWPOW_VERSION = 70027;

//! Meowcoin: MEOWPOW activation protocol version
static const int MEOWPOW_VERSION = 70030;

//! Meowcoin: asset data P2P version
static const int ASSETDATA_VERSION = 70017;

//! Meowcoin: updated asset data P2P version
static const int ASSETDATA_VERSION_UPDATED = 70020;

//! Meowcoin: messaging & restricted assets protocol version
static const int MESSAGING_RESTRICTED_ASSETS_VERSION = 70026;

//! Meowcoin: AuxPoW activation protocol version
static const int AUXPOW_VERSION = 70031;

#endif // BITCOIN_NODE_PROTOCOL_VERSION_H
