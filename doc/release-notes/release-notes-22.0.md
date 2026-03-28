22.0 Release Notes
==================

Meowcoin Core version 22.0 is now available from:

  <https://meowcoincore.org/bin/meowcoin-core-22.0/>

This release includes new features, various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/meowcoin/meowcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://meowcoincore.org/en/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes in some cases), then run the
installer (on Windows) or just copy over `/Applications/Meowcoin-Qt` (on Mac)
or `meowcoind`/`meowcoin-qt` (on Linux).

Upgrading directly from a version of Meowcoin Core that has reached its EOL is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Meowcoin Core are generally supported.

Compatibility
==============

Meowcoin Core is supported and extensively tested on operating systems
using the Linux kernel, macOS 10.14+, and Windows 7 and newer.  Meowcoin
Core should also work on most other Unix-like systems but is not as
frequently tested on them.  It is not recommended to use Meowcoin Core on
unsupported systems.

From Meowcoin Core 22.0 onwards, macOS versions earlier than 10.14 are no longer supported.

Notable changes
===============

P2P and network changes
-----------------------
- Added support for running Meowcoin Core as an
  [I2P (Invisible Internet Project)](https://en.wikipedia.org/wiki/I2P) service
  and connect to such services. See [i2p.md](https://github.com/meowcoin/meowcoin/blob/22.x/doc/i2p.md) for details. (#20685)
- This release removes support for Tor version 2 hidden services in favor of Tor
  v3 only, as the Tor network [dropped support for Tor
  v2](https://blog.torproject.org/v2-deprecation-timeline) with the release of
  Tor version 0.4.6.  Henceforth, Meowcoin Core ignores Tor v2 addresses; it
  neither rumors them over the network to other peers, nor stores them in memory
  or to `peers.dat`.  (#22050)

- Added NAT-PMP port mapping support via
  [`libnatpmp`](https://miniupnp.tuxfamily.org/libnatpmp.html). (#18077)

New and Updated RPCs
--------------------

- Due to [BIP 350](https://github.com/meowcoin/bips/blob/master/bip-0350.mediawiki)
  being implemented, behavior for all RPCs that accept addresses is changed when
  a native witness version 1 (or higher) is passed. These now require a Bech32m
  encoding instead of a Bech32 one, and Bech32m encoding will be used for such
  addresses in RPC output as well. No version 1 addresses should be created
  for mainnet until consensus rules are adopted that give them meaning
  (as will happen through [BIP 341](https://github.com/meowcoin/bips/blob/master/bip-0341.mediawiki)).
  Once that happens, Bech32m is expected to be used for them, so this shouldn't
  affect any production systems, but may be observed on other networks where such
  addresses already have meaning (like signet). (#20861)

- The `getpeerinfo` RPC returns two new boolean fields, `bip152_hb_to` and
  `bip152_hb_from`, that respectively indicate whether we selected a peer to be
  in compact blocks high-bandwidth mode or whether a peer selected us as a
  compact blocks high-bandwidth peer. High-bandwidth peers send new block
  announcements via a `cmpctblock` message rather than the usual inv/headers
  announcements. See BIP 152 for more details. (#19776)

- `getpeerinfo` no longer returns the following fields: `addnode`, `banscore`,
  and `whitelisted`, which were previously deprecated in 0.21. Instead of
  `addnode`, the `connection_type` field returns manual. Instead of
  `whitelisted`, the `permissions` field indicates if the peer has special
  privileges. The `banscore` field has simply been removed. (#20755)

- The following RPCs:  `gettxout`, `getrawtransaction`, `decoderawtransaction`,
  `decodescript`, `gettransaction`, and REST endpoints: `/rest/tx`,
  `/rest/getutxos`, `/rest/block` deprecated the following fields (which are no
  longer returned in the responses by default): `addresses`, `reqSigs`.
  The `-deprecatedrpc=addresses` flag must be passed for these fields to be
  included in the RPC response. This flag/option will be available only for this major release, after which
  the deprecation will be removed entirely. Note that these fields are attributes of
  the `scriptPubKey` object returned in the RPC response. However, in the response
  of `decodescript` these fields are top-level attributes, and included again as attributes
  of the `scriptPubKey` object. (#20286)

- When creating a hex-encoded meowcoin transaction using the `meowcoin-tx` utility
  with the `-json` option set, the following fields: `addresses`, `reqSigs` are no longer
  returned in the tx output of the response. (#20286)

- The `listbanned` RPC now returns two new numeric fields: `ban_duration` and `time_remaining`.
  Respectively, these new fields indicate the duration of a ban and the time remaining until a ban expires,
  both in seconds. Additionally, the `ban_created` field is repositioned to come before `banned_until`. (#21602)

- The `setban` RPC can ban onion addresses again. This fixes a regression introduced in version 0.21.0. (#20852)

- The `getnodeaddresses` RPC now returns a "network" field indicating the
  network type (ipv4, ipv6, onion, or i2p) for each address.  (#21594)

- `getnodeaddresses` now also accepts a "network" argument (ipv4, ipv6, onion,
  or i2p) to return only addresses of the specified network.  (#21843)

- The `testmempoolaccept` RPC now accepts multiple transactions (still experimental at the moment,
  API may be unstable). This is intended for testing transaction packages with dependency
  relationships; it is not recommended for batch-validating independent transactions. In addition to
  mempool policy, package policies apply: the list cannot contain more than 25 transactions or have a
  total size exceeding 101K virtual bytes, and cannot conflict with (spend the same inputs as) each other or
  the mempool, even if it would be a valid BIP125 replace-by-fee. There are some known limitations to
  the accuracy of the test accept: it's possible for `testmempoolaccept` to return "allowed"=True for a
  group of transactions, but "too-long-mempool-chain" if they are actually submitted. (#20833)

- `addmultisigaddress` and `createmultisig` now support up to 20 keys for
  Segwit addresses. (#20867)

Changes to Wallet or GUI related RPCs can be found in the GUI or Wallet section below.

Build System
------------

- Release binaries are now produced using the new `guix`-based build system.
  The [/doc/release-process.md](/doc/release-process.md) document has been updated accordingly.

Files
-----

- The list of banned hosts and networks (via `setban` RPC) is now saved on disk
  in JSON format in `banlist.json` instead of `banlist.dat`. `banlist.dat` is
  only read on startup if `banlist.json` is not present. Changes are only written to the new
  `banlist.json`. A future version of Meowcoin Core may completely ignore
  `banlist.dat`. (#20966)

New settings
------------

- The `-natpmp` option has been added to use NAT-PMP to map the listening port.
  If both UPnP and NAT-PMP are enabled, a successful allocation from UPnP
  prevails over one from NAT-PMP. (#18077)

Updated settings
----------------

Changes to Wallet or GUI related settings can be found in the GUI or Wallet section below.

- Passing an invalid `-rpcauth` argument now cause meowcoind to fail to start.  (#20461)

Tools and Utilities
-------------------

- A new CLI `-addrinfo` command returns the number of addresses known to the
  node per network type (including Tor v2 versus v3) and total. This can be
  useful to see if the node knows enough addresses in a network to use options
  like `-onlynet=<network>` or to upgrade to this release of Meowcoin Core 22.0
  that supports Tor v3 only.  (#21595)

- A new `-rpcwaittimeout` argument to `meowcoin-cli` sets the timeout
  in seconds to use with `-rpcwait`. If the timeout expires,
  `meowcoin-cli` will report a failure. (#21056)

Wallet
------

- External signers such as hardware wallets can now be used through the new RPC methods `enumeratesigners` and `displayaddress`. Support is also added to the `send` RPC call. This feature is experimental. See [external-signer.md](https://github.com/meowcoin/meowcoin/blob/22.x/doc/external-signer.md) for details. (#16546)

- A new `listdescriptors` RPC is available to inspect the contents of descriptor-enabled wallets.
  The RPC returns public versions of all imported descriptors, including their timestamp and flags.
  For ranged descriptors, it also returns the range boundaries and the next index to generate addresses from. (#20226)

- The `bumpfee` RPC is not available with wallets that have private keys
  disabled. `psmtbumpfee` can be used instead. (#20891)

- The `fundrawtransaction`, `send` and `walletcreatefundedpsmt` RPCs now support an `include_unsafe` option
  that when `true` allows using unsafe inputs to fund the transaction.
  Note that the resulting transaction may become invalid if one of the unsafe inputs disappears.
  If that happens, the transaction must be funded with different inputs and republished. (#21359)

- We now support up to 20 keys in `multi()` and `sortedmulti()` descriptors
  under `wsh()`. (#20867)

- Taproot descriptors can be imported into the wallet only after activation has occurred on the network (e.g. mainnet, testnet, signet) in use. See [descriptors.md](https://github.com/meowcoin/meowcoin/blob/22.x/doc/descriptors.md) for supported descriptors.

GUI changes
-----------

- External signers such as hardware wallets can now be used. These require an external tool such as [HWI](https://github.com/meowcoin-core/HWI) to be installed and configured under Options -> Wallet. When creating a new wallet a new option "External signer" will appear in the dialog. If the device is detected, its name is suggested as the wallet name. The watch-only keys are then automatically imported. Receive addresses can be verified on the device. The send dialog will automatically use the connected device. This feature is experimental and the UI may freeze for a few seconds when performing these actions.

Low-level changes
=================

RPC
---

- The RPC server can process a limited number of simultaneous RPC requests.
  Previously, if this limit was exceeded, the RPC server would respond with
  [status code 500 (`HTTP_INTERNAL_SERVER_ERROR`)](https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#5xx_server_errors).
  Now it returns status code 503 (`HTTP_SERVICE_UNAVAILABLE`). (#18335)

- Error codes have been updated to be more accurate for the following error cases (#18466):
  - `signmessage` now returns RPC_INVALID_ADDRESS_OR_KEY (-5) if the
    passed address is invalid. Previously returned RPC_TYPE_ERROR (-3).
  - `verifymessage` now returns RPC_INVALID_ADDRESS_OR_KEY (-5) if the
    passed address is invalid. Previously returned RPC_TYPE_ERROR (-3).
  - `verifymessage` now returns RPC_TYPE_ERROR (-3) if the passed signature
    is malformed. Previously returned RPC_INVALID_ADDRESS_OR_KEY (-5).

Tests
-----

22.0 change log
===============

A detailed list of changes in this version follows. To keep the list to a manageable length, small refactors and typo fixes are not included, and similar changes are sometimes condensed into one line.

### Consensus
- meowcoin/meowcoin#19438 Introduce deploymentstatus (ajtowns)
- meowcoin/meowcoin#20207 Follow-up extra comments on taproot code and tests (sipa)
- meowcoin/meowcoin#21330 Deal with missing data in signature hashes more consistently (sipa)

### Policy
- meowcoin/meowcoin#18766 Disable fee estimation in blocksonly mode (by removing the fee estimates global) (darosior)
- meowcoin/meowcoin#20497 Add `MAX_STANDARD_SCRIPTSIG_SIZE` to policy (sanket1729)
- meowcoin/meowcoin#20611 Move `TX_MAX_STANDARD_VERSION` to policy (MarcoFalke)

### Mining
- meowcoin/meowcoin#19937, meowcoin/meowcoin#20923 Signet mining utility (ajtowns)

### Block and transaction handling
- meowcoin/meowcoin#14501 Fix possible data race when committing block files (luke-jr)
- meowcoin/meowcoin#15946 Allow maintaining the blockfilterindex when using prune (jonasschnelli)
- meowcoin/meowcoin#18710 Add local thread pool to CCheckQueue (hebasto)
- meowcoin/meowcoin#19521 Coinstats Index (fjahr)
- meowcoin/meowcoin#19806 UTXO snapshot activation (jamesob)
- meowcoin/meowcoin#19905 Remove dead CheckForkWarningConditionsOnNewFork (MarcoFalke)
- meowcoin/meowcoin#19935 Move SaltedHashers to separate file and add some new ones (achow101)
- meowcoin/meowcoin#20054 Remove confusing and useless "unexpected version" warning (MarcoFalke)
- meowcoin/meowcoin#20519 Handle rename failure in `DumpMempool(…)` by using the `RenameOver(…)` return value (practicalswift)
- meowcoin/meowcoin#20749, meowcoin/meowcoin#20750, meowcoin/meowcoin#21055, meowcoin/meowcoin#21270, meowcoin/meowcoin#21525, meowcoin/meowcoin#21391, meowcoin/meowcoin#21767, meowcoin/meowcoin#21866 Prune `g_chainman` usage (dongcarl)
- meowcoin/meowcoin#20833 rpc/validation: enable packages through testmempoolaccept (glozow)
- meowcoin/meowcoin#20834 Locks and docs in ATMP and CheckInputsFromMempoolAndCache (glozow)
- meowcoin/meowcoin#20854 Remove unnecessary try-block (amitiuttarwar)
- meowcoin/meowcoin#20868 Remove redundant check on pindex (jarolrod)
- meowcoin/meowcoin#20921 Don't try to invalidate genesis block in CChainState::InvalidateBlock (theStack)
- meowcoin/meowcoin#20972 Locks: Annotate CTxMemPool::check to require `cs_main` (dongcarl)
- meowcoin/meowcoin#21009 Remove RewindBlockIndex logic (dhruv)
- meowcoin/meowcoin#21025 Guard chainman chainstates with `cs_main` (dongcarl)
- meowcoin/meowcoin#21202 Two small clang lock annotation improvements (amitiuttarwar)
- meowcoin/meowcoin#21523 Run VerifyDB on all chainstates (jamesob)
- meowcoin/meowcoin#21573 Update libsecp256k1 subtree to latest master (sipa)
- meowcoin/meowcoin#21582, meowcoin/meowcoin#21584, meowcoin/meowcoin#21585 Fix assumeutxo crashes (MarcoFalke)
- meowcoin/meowcoin#21681 Fix ActivateSnapshot to use hardcoded nChainTx (jamesob)
- meowcoin/meowcoin#21796 index: Avoid async shutdown on init error (MarcoFalke)
- meowcoin/meowcoin#21946 Document and test lack of inherited signaling in RBF policy (ariard)
- meowcoin/meowcoin#22084 Package testmempoolaccept followups (glozow)
- meowcoin/meowcoin#22102 Remove `Warning:` from warning message printed for unknown new rules (prayank23)
- meowcoin/meowcoin#22112 Force port 0 in I2P (vasild)
- meowcoin/meowcoin#22135 CRegTestParams: Use `args` instead of `gArgs` (kiminuo)
- meowcoin/meowcoin#22146 Reject invalid coin height and output index when loading assumeutxo (MarcoFalke)
- meowcoin/meowcoin#22253 Distinguish between same tx and same-nonwitness-data tx in mempool (glozow)
- meowcoin/meowcoin#22261 Two small fixes to node broadcast logic (jnewbery)
- meowcoin/meowcoin#22415 Make `m_mempool` optional in CChainState (jamesob)
- meowcoin/meowcoin#22499 Update assumed chain params (sriramdvt)
- meowcoin/meowcoin#22589 net, doc: update I2P hardcoded seeds and docs for 22.0 (jonatack)

### P2P protocol and network code
- meowcoin/meowcoin#18077 Add NAT-PMP port forwarding support (hebasto)
- meowcoin/meowcoin#18722 addrman: improve performance by using more suitable containers (vasild)
- meowcoin/meowcoin#18819 Replace `cs_feeFilter` with simple std::atomic (MarcoFalke)
- meowcoin/meowcoin#19203 Add regression fuzz harness for CVE-2017-18350. Add FuzzedSocket (practicalswift)
- meowcoin/meowcoin#19288 fuzz: Add fuzzing harness for TorController (practicalswift)
- meowcoin/meowcoin#19415 Make DNS lookup mockable, add fuzzing harness (practicalswift)
- meowcoin/meowcoin#19509 Per-Peer Message Capture (troygiorshev)
- meowcoin/meowcoin#19763 Don't try to relay to the address' originator (vasild)
- meowcoin/meowcoin#19771 Replace enum CConnMan::NumConnections with enum class ConnectionDirection (luke-jr)
- meowcoin/meowcoin#19776 net, rpc: expose high bandwidth mode state via getpeerinfo (theStack)
- meowcoin/meowcoin#19832 Put disconnecting logs into BCLog::NET category (hebasto)
- meowcoin/meowcoin#19858 Periodically make block-relay connections and sync headers (sdaftuar)
- meowcoin/meowcoin#19884 No delay in adding fixed seeds if -dnsseed=0 and peers.dat is empty (dhruv)
- meowcoin/meowcoin#20079 Treat handshake misbehavior like unknown message (MarcoFalke)
- meowcoin/meowcoin#20138 Assume that SetCommonVersion is called at most once per peer (MarcoFalke)
- meowcoin/meowcoin#20162 p2p: declare Announcement::m_state as uint8_t, add getter/setter (jonatack)
- meowcoin/meowcoin#20197 Protect onions in AttemptToEvictConnection(), add eviction protection test coverage (jonatack)
- meowcoin/meowcoin#20210 assert `CNode::m_inbound_onion` is inbound in ctor, add getter, unit tests (jonatack)
- meowcoin/meowcoin#20228 addrman: Make addrman a top-level component (jnewbery)
- meowcoin/meowcoin#20234 Don't bind on 0.0.0.0 if binds are restricted to Tor (vasild)
- meowcoin/meowcoin#20477 Add unit testing of node eviction logic (practicalswift)
- meowcoin/meowcoin#20516 Well-defined CAddress disk serialization, and addrv2 anchors.dat (sipa)
- meowcoin/meowcoin#20557 addrman: Fix new table bucketing during unserialization (jnewbery)
- meowcoin/meowcoin#20561 Periodically clear `m_addr_known` (sdaftuar)
- meowcoin/meowcoin#20599 net processing: Tolerate sendheaders and sendcmpct messages before verack (jnewbery)
- meowcoin/meowcoin#20616 Check CJDNS address is valid (lontivero)
- meowcoin/meowcoin#20617 Remove `m_is_manual_connection` from CNodeState (ariard)
- meowcoin/meowcoin#20624 net processing: Remove nStartingHeight check from block relay (jnewbery)
- meowcoin/meowcoin#20651 Make p2p recv buffer timeout 20 minutes for all peers (jnewbery)
- meowcoin/meowcoin#20661 Only select from addrv2-capable peers for torv3 address relay (sipa)
- meowcoin/meowcoin#20685 Add I2P support using I2P SAM (vasild)
- meowcoin/meowcoin#20690 Clean up logging of outbound connection type (sdaftuar)
- meowcoin/meowcoin#20721 Move ping data to `net_processing` (jnewbery)
- meowcoin/meowcoin#20724 Cleanup of -debug=net log messages (ajtowns)
- meowcoin/meowcoin#20747 net processing: Remove dropmessagestest (jnewbery)
- meowcoin/meowcoin#20764 cli -netinfo peer connections dashboard updates 🎄 ✨ (jonatack)
- meowcoin/meowcoin#20788 add RAII socket and use it instead of bare SOCKET (vasild)
- meowcoin/meowcoin#20791 remove unused legacyWhitelisted in AcceptConnection() (jonatack)
- meowcoin/meowcoin#20816 Move RecordBytesSent() call out of `cs_vSend` lock (jnewbery)
- meowcoin/meowcoin#20845 Log to net debug in MaybeDiscourageAndDisconnect except for noban and manual peers (MarcoFalke)
- meowcoin/meowcoin#20864 Move SocketSendData lock annotation to header (MarcoFalke)
- meowcoin/meowcoin#20965 net, rpc:  return `NET_UNROUTABLE` as `not_publicly_routable`, automate helps (jonatack)
- meowcoin/meowcoin#20966 banman: save the banlist in a JSON format on disk (vasild)
- meowcoin/meowcoin#21015 Make all of `net_processing` (and some of net) use std::chrono types (dhruv)
- meowcoin/meowcoin#21029 meowcoin-cli: Correct docs (no "generatenewaddress" exists) (luke-jr)
- meowcoin/meowcoin#21148 Split orphan handling from `net_processing` into txorphanage (ajtowns)
- meowcoin/meowcoin#21162 Net Processing: Move RelayTransaction() into PeerManager (jnewbery)
- meowcoin/meowcoin#21167 make `CNode::m_inbound_onion` public, initialize explicitly (jonatack)
- meowcoin/meowcoin#21186 net/net processing: Move addr data into `net_processing` (jnewbery)
- meowcoin/meowcoin#21187 Net processing: Only call PushAddress() from `net_processing` (jnewbery)
- meowcoin/meowcoin#21198 Address outstanding review comments from PR20721 (jnewbery)
- meowcoin/meowcoin#21222 log: Clarify log message when file does not exist (MarcoFalke)
- meowcoin/meowcoin#21235 Clarify disconnect log message in ProcessGetBlockData, remove send bool (MarcoFalke)
- meowcoin/meowcoin#21236 Net processing: Extract `addr` send functionality into MaybeSendAddr() (jnewbery)
- meowcoin/meowcoin#21261 update inbound eviction protection for multiple networks, add I2P peers (jonatack)
- meowcoin/meowcoin#21328 net, refactor: pass uint16 CService::port as uint16 (jonatack)
- meowcoin/meowcoin#21387 Refactor sock to add I2P fuzz and unit tests (vasild)
- meowcoin/meowcoin#21395 Net processing: Remove unused CNodeState.address member (jnewbery)
- meowcoin/meowcoin#21407 i2p: limit the size of incoming messages (vasild)
- meowcoin/meowcoin#21506 p2p, refactor: make NetPermissionFlags an enum class (jonatack)
- meowcoin/meowcoin#21509 Don't send FEEFILTER in blocksonly mode (mzumsande)
- meowcoin/meowcoin#21560 Add Tor v3 hardcoded seeds (laanwj)
- meowcoin/meowcoin#21563 Restrict period when `cs_vNodes` mutex is locked (hebasto)
- meowcoin/meowcoin#21564 Avoid calling getnameinfo when formatting IPv4 addresses in CNetAddr::ToStringIP (practicalswift)
- meowcoin/meowcoin#21631 i2p: always check the return value of Sock::Wait() (vasild)
- meowcoin/meowcoin#21644 p2p, bugfix: use NetPermissions::HasFlag() in CConnman::Bind() (jonatack)
- meowcoin/meowcoin#21659 flag relevant Sock methods with [[nodiscard]] (vasild)
- meowcoin/meowcoin#21750 remove unnecessary check of `CNode::cs_vSend` (vasild)
- meowcoin/meowcoin#21756 Avoid calling `getnameinfo` when formatting IPv6 addresses in `CNetAddr::ToStringIP` (practicalswift)
- meowcoin/meowcoin#21775 Limit `m_block_inv_mutex` (MarcoFalke)
- meowcoin/meowcoin#21825 Add I2P hardcoded seeds (jonatack)
- meowcoin/meowcoin#21843 p2p, rpc: enable GetAddr, GetAddresses, and getnodeaddresses by network (jonatack)
- meowcoin/meowcoin#21845 net processing: Don't require locking `cs_main` before calling RelayTransactions() (jnewbery)
- meowcoin/meowcoin#21872 Sanitize message type for logging (laanwj)
- meowcoin/meowcoin#21914 Use stronger AddLocal() for our I2P address (vasild)
- meowcoin/meowcoin#21985 Return IPv6 scope id in `CNetAddr::ToStringIP()` (laanwj)
- meowcoin/meowcoin#21992 Remove -feefilter option (amadeuszpawlik)
- meowcoin/meowcoin#21996 Pass strings to NetPermissions::TryParse functions by const ref (jonatack)
- meowcoin/meowcoin#22013 ignore block-relay-only peers when skipping DNS seed (ajtowns)
- meowcoin/meowcoin#22050 Remove tor v2 support (jonatack)
- meowcoin/meowcoin#22096 AddrFetch - don't disconnect on self-announcements (mzumsande)
- meowcoin/meowcoin#22141 net processing: Remove hash and fValidatedHeaders from QueuedBlock (jnewbery)
- meowcoin/meowcoin#22144 Randomize message processing peer order (sipa)
- meowcoin/meowcoin#22147 Protect last outbound HB compact block peer (sdaftuar)
- meowcoin/meowcoin#22179 Torv2 removal followups (vasild)
- meowcoin/meowcoin#22211 Relay I2P addresses even if not reachable (by us) (vasild)
- meowcoin/meowcoin#22284 Performance improvements to ProtectEvictionCandidatesByRatio() (jonatack)
- meowcoin/meowcoin#22387 Rate limit the processing of rumoured addresses (sipa)
- meowcoin/meowcoin#22455 addrman: detect on-disk corrupted nNew and nTried during unserialization (vasild)

### Wallet
- meowcoin/meowcoin#15710 Catch `ios_base::failure` specifically (Bushstar)
- meowcoin/meowcoin#16546 External signer support - Wallet Box edition (Sjors)
- meowcoin/meowcoin#17331 Use effective values throughout coin selection (achow101)
- meowcoin/meowcoin#18418 Increase `OUTPUT_GROUP_MAX_ENTRIES` to 100 (fjahr)
- meowcoin/meowcoin#18842 Mark replaced tx to not be in the mempool anymore (MarcoFalke)
- meowcoin/meowcoin#19136 Add `parent_desc` to `getaddressinfo` (achow101)
- meowcoin/meowcoin#19137 wallettool: Add dump and createfromdump commands (achow101)
- meowcoin/meowcoin#19651 `importdescriptor`s update existing (S3RK)
- meowcoin/meowcoin#20040 Refactor OutputGroups to handle fees and spending eligibility on grouping (achow101)
- meowcoin/meowcoin#20202 Make BDB support optional (achow101)
- meowcoin/meowcoin#20226, meowcoin/meowcoin#21277, - meowcoin/meowcoin#21063 Add `listdescriptors` command (S3RK)
- meowcoin/meowcoin#20267 Disable and fix tests for when BDB is not compiled (achow101)
- meowcoin/meowcoin#20275 List all wallets in non-SQLite and non-BDB builds (ryanofsky)
- meowcoin/meowcoin#20365 wallettool: Add parameter to create descriptors wallet (S3RK)
- meowcoin/meowcoin#20403 `upgradewallet` fixes, improvements, test coverage (jonatack)
- meowcoin/meowcoin#20448 `unloadwallet`: Allow specifying `wallet_name` param matching RPC endpoint wallet (luke-jr)
- meowcoin/meowcoin#20536 Error with "Transaction too large" if the funded tx will end up being too large after signing (achow101)
- meowcoin/meowcoin#20687 Add missing check for -descriptors wallet tool option (MarcoFalke)
- meowcoin/meowcoin#20952 Add BerkeleyDB version sanity check at init time (laanwj)
- meowcoin/meowcoin#21127 Load flags before everything else (Sjors)
- meowcoin/meowcoin#21141 Add new format string placeholders for walletnotify (maayank)
- meowcoin/meowcoin#21238 A few descriptor improvements to prepare for Taproot support (sipa)
- meowcoin/meowcoin#21302 `createwallet` examples for descriptor wallets (S3RK)
- meowcoin/meowcoin#21329 descriptor wallet: Cache last hardened xpub and use in normalized descriptors (achow101)
- meowcoin/meowcoin#21365 Basic Taproot signing support for descriptor wallets (sipa)
- meowcoin/meowcoin#21417 Misc external signer improvement and HWI 2 support (Sjors)
- meowcoin/meowcoin#21467 Move external signer out of wallet module (Sjors)
- meowcoin/meowcoin#21572 Fix wrong wallet RPC context set after #21366 (ryanofsky)
- meowcoin/meowcoin#21574 Drop JSONRPCRequest constructors after #21366 (ryanofsky)
- meowcoin/meowcoin#21666 Miscellaneous external signer changes (fanquake)
- meowcoin/meowcoin#21759 Document coin selection code (glozow)
- meowcoin/meowcoin#21786 Ensure mewc/vB feerates are in range (mantissa of 3) (jonatack)
- meowcoin/meowcoin#21944 Fix issues when `walletdir` is root directory (prayank23)
- meowcoin/meowcoin#22042 Replace size/weight estimate tuple with struct for named fields (instagibbs)
- meowcoin/meowcoin#22051 Basic Taproot derivation support for descriptors (sipa)
- meowcoin/meowcoin#22154 Add OutputType::BECH32M and related wallet support for fetching bech32m addresses (achow101)
- meowcoin/meowcoin#22156 Allow tr() import only when Taproot is active (achow101)
- meowcoin/meowcoin#22166 Add support for inferring tr() descriptors (sipa)
- meowcoin/meowcoin#22173 Do not load external signers wallets when unsupported (achow101)
- meowcoin/meowcoin#22308 Add missing BlockUntilSyncedToCurrentChain (MarcoFalke)
- meowcoin/meowcoin#22334 Do not spam about non-existent spk managers (S3RK)
- meowcoin/meowcoin#22379 Erase spkmans rather than setting to nullptr (achow101)
- meowcoin/meowcoin#22421 Make IsSegWitOutput return true for taproot outputs (sipa)
- meowcoin/meowcoin#22461 Change ScriptPubKeyMan::Upgrade default to True (achow101)
- meowcoin/meowcoin#22492 Reorder locks in dumpwallet to avoid lock order assertion (achow101)
- meowcoin/meowcoin#22686 Use GetSelectionAmount in ApproximateBestSubset (achow101)

### RPC and other APIs
- meowcoin/meowcoin#18335, meowcoin/meowcoin#21484 cli: Print useful error if meowcoind rpc work queue exceeded (LarryRuane)
- meowcoin/meowcoin#18466 Fix invalid parameter error codes for `{sign,verify}message` RPCs (theStack)
- meowcoin/meowcoin#18772 Calculate fees in `getblock` using BlockUndo data (robot-visions)
- meowcoin/meowcoin#19033 http: Release work queue after event base finish (promag)
- meowcoin/meowcoin#19055 Add MuHash3072 implementation (fjahr)
- meowcoin/meowcoin#19145 Add `hash_type` MUHASH for gettxoutsetinfo (fjahr)
- meowcoin/meowcoin#19847 Avoid duplicate set lookup in `gettxoutproof` (promag)
- meowcoin/meowcoin#20286 Deprecate `addresses` and `reqSigs` from RPC outputs (mjdietzx)
- meowcoin/meowcoin#20459 Fail to return undocumented return values (MarcoFalke)
- meowcoin/meowcoin#20461 Validate `-rpcauth` arguments (promag)
- meowcoin/meowcoin#20556 Properly document return values (`submitblock`, `gettxout`, `getblocktemplate`, `scantxoutset`) (MarcoFalke)
- meowcoin/meowcoin#20755 Remove deprecated fields from `getpeerinfo` (amitiuttarwar)
- meowcoin/meowcoin#20832 Better error messages for invalid addresses (eilx2)
- meowcoin/meowcoin#20867 Support up to 20 keys for multisig under Segwit context (darosior)
- meowcoin/meowcoin#20877 cli: `-netinfo` user help and argument parsing improvements (jonatack)
- meowcoin/meowcoin#20891 Remove deprecated bumpfee behavior (achow101)
- meowcoin/meowcoin#20916 Return wtxid from `testmempoolaccept` (MarcoFalke)
- meowcoin/meowcoin#20917 Add missing signet mentions in network name lists (theStack)
- meowcoin/meowcoin#20941 Document `RPC_TRANSACTION_ALREADY_IN_CHAIN` exception (jarolrod)
- meowcoin/meowcoin#20944 Return total fee in `getmempoolinfo` (MarcoFalke)
- meowcoin/meowcoin#20964 Add specific error code for "wallet already loaded" (laanwj)
- meowcoin/meowcoin#21053 Document {previous,next}blockhash as optional (theStack)
- meowcoin/meowcoin#21056 Add a `-rpcwaittimeout` parameter to limit time spent waiting (cdecker)
- meowcoin/meowcoin#21192 cli: Treat high detail levels as maximum in `-netinfo` (laanwj)
- meowcoin/meowcoin#21311 Document optional fields for `getchaintxstats` result (theStack)
- meowcoin/meowcoin#21359 `include_unsafe` option for fundrawtransaction (t-bast)
- meowcoin/meowcoin#21426 Remove `scantxoutset` EXPERIMENTAL warning (jonatack)
- meowcoin/meowcoin#21544 Missing doc updates for bumpfee psmt update (MarcoFalke)
- meowcoin/meowcoin#21594 Add `network` field to `getnodeaddresses` (jonatack)
- meowcoin/meowcoin#21595, meowcoin/meowcoin#21753 cli: Create `-addrinfo` (jonatack)
- meowcoin/meowcoin#21602 Add additional ban time fields to `listbanned` (jarolrod)
- meowcoin/meowcoin#21679 Keep default argument value in correct type (promag)
- meowcoin/meowcoin#21718 Improve error message for `getblock` invalid datatype (klementtan)
- meowcoin/meowcoin#21913 RPCHelpMan fixes (kallewoof)
- meowcoin/meowcoin#22021 `bumpfee`/`psmtbumpfee` fixes and updates (jonatack)
- meowcoin/meowcoin#22043 `addpeeraddress` test coverage, code simplify/constness (jonatack)
- meowcoin/meowcoin#22327 cli: Avoid truncating `-rpcwaittimeout` (MarcoFalke)

### GUI
- meowcoin/meowcoin#18948 Call setParent() in the parent's context (hebasto)
- meowcoin/meowcoin#20482 Add depends qt fix for ARM macs (jonasschnelli)
- meowcoin/meowcoin#21836 scripted-diff: Replace three dots with ellipsis in the ui strings (hebasto)
- meowcoin/meowcoin#21935 Enable external signer support for GUI builds (Sjors)
- meowcoin/meowcoin#22133 Make QWindowsVistaStylePlugin available again (regression) (hebasto)
- meowcoin-core/gui#4 UI external signer support (e.g. hardware wallet) (Sjors)
- meowcoin-core/gui#13 Hide peer detail view if multiple are selected (promag)
- meowcoin-core/gui#18 Add peertablesortproxy module (hebasto)
- meowcoin-core/gui#21 Improve pruning tooltip (fluffypony, BitcoinErrorLog)
- meowcoin-core/gui#72 Log static plugins meta data and used style (hebasto)
- meowcoin-core/gui#79 Embed monospaced font (hebasto)
- meowcoin-core/gui#85 Remove unused "What's This" button in dialogs on Windows OS (hebasto)
- meowcoin-core/gui#115 Replace "Hide tray icon" option with positive "Show tray icon" one (hebasto)
- meowcoin-core/gui#118 Remove BDB version from the Information tab (hebasto)
- meowcoin-core/gui#121 Early subscribe core signals in transaction table model (promag)
- meowcoin-core/gui#123 Do not accept command while executing another one (hebasto)
- meowcoin-core/gui#125 Enable changing the autoprune block space size in intro dialog (luke-jr)
- meowcoin-core/gui#138 Unlock encrypted wallet "OK" button bugfix (mjdietzx)
- meowcoin-core/gui#139 doc: Improve gui/src/qt README.md (jarolrod)
- meowcoin-core/gui#154 Support macOS Dark mode (goums, Uplab)
- meowcoin-core/gui#162 Add network to peers window and peer details (jonatack)
- meowcoin-core/gui#163, meowcoin-core/gui#180 Peer details: replace Direction with Connection Type (jonatack)
- meowcoin-core/gui#164 Handle peer addition/removal in a right way (hebasto)
- meowcoin-core/gui#165 Save QSplitter state in QSettings (hebasto)
- meowcoin-core/gui#173 Follow Qt docs when implementing rowCount and columnCount (hebasto)
- meowcoin-core/gui#179 Add Type column to peers window, update peer details name/tooltip (jonatack)
- meowcoin-core/gui#186 Add information to "Confirm fee bump" window (prayank23)
- meowcoin-core/gui#189 Drop workaround for QTBUG-42503 which was fixed in Qt 5.5.0 (prusnak)
- meowcoin-core/gui#194 Save/restore RPCConsole geometry only for window (hebasto)
- meowcoin-core/gui#202 Fix right panel toggle in peers tab (RandyMcMillan)
- meowcoin-core/gui#203 Display plain "Inbound" in peer details (jonatack)
- meowcoin-core/gui#204 Drop buggy TableViewLastColumnResizingFixer class (hebasto)
- meowcoin-core/gui#205, meowcoin-core/gui#229 Save/restore TransactionView and recentRequestsView tables column sizes (hebasto)
- meowcoin-core/gui#206 Display fRelayTxes and `bip152_highbandwidth_{to, from}` in peer details (jonatack)
- meowcoin-core/gui#213 Add Copy Address Action to Payment Requests (jarolrod)
- meowcoin-core/gui#214 Disable requests context menu actions when appropriate (jarolrod)
- meowcoin-core/gui#217 Make warning label look clickable (jarolrod)
- meowcoin-core/gui#219 Prevent the main window popup menu (hebasto)
- meowcoin-core/gui#220 Do not translate file extensions (hebasto)
- meowcoin-core/gui#221 RPCConsole translatable string fixes and improvements (jonatack)
- meowcoin-core/gui#226 Add "Last Block" and "Last Tx" rows to peer details area (jonatack)
- meowcoin-core/gui#233 qt test: Don't bind to regtest port (achow101)
- meowcoin-core/gui#243 Fix issue when disabling the auto-enabled blank wallet checkbox (jarolrod)
- meowcoin-core/gui#246 Revert "qt: Use "fusion" style on macOS Big Sur with old Qt" (hebasto)
- meowcoin-core/gui#248 For values of "Bytes transferred" and "Bytes/s" with 1000-based prefix names use 1000-based divisor instead of 1024-based (wodry)
- meowcoin-core/gui#251 Improve URI/file handling message (hebasto)
- meowcoin-core/gui#256 Save/restore column sizes of the tables in the Peers tab (hebasto)
- meowcoin-core/gui#260 Handle exceptions isntead of crash (hebasto)
- meowcoin-core/gui#263 Revamp context menus (hebasto)
- meowcoin-core/gui#271 Don't clear console prompt when font resizing (jarolrod)
- meowcoin-core/gui#275 Support runtime appearance adjustment on macOS (hebasto)
- meowcoin-core/gui#276 Elide long strings in their middle in the Peers tab (hebasto)
- meowcoin-core/gui#281 Set shortcuts for console's resize buttons (jarolrod)
- meowcoin-core/gui#293 Enable wordWrap for Services (RandyMcMillan)
- meowcoin-core/gui#296 Do not use QObject::tr plural syntax for numbers with a unit symbol (hebasto)
- meowcoin-core/gui#297 Avoid unnecessary translations (hebasto)
- meowcoin-core/gui#298 Peertableview alternating row colors (RandyMcMillan)
- meowcoin-core/gui#300 Remove progress bar on modal overlay (brunoerg)
- meowcoin-core/gui#309 Add access to the Peers tab from the network icon (hebasto)
- meowcoin-core/gui#311 Peers Window rename 'Peer id' to 'Peer' (jarolrod)
- meowcoin-core/gui#313 Optimize string concatenation by default (hebasto)
- meowcoin-core/gui#325 Align numbers in the "Peer Id" column to the right (hebasto)
- meowcoin-core/gui#329 Make console buttons look clickable (jarolrod)
- meowcoin-core/gui#330 Allow prompt icon to be colorized (jarolrod)
- meowcoin-core/gui#331 Make RPC console welcome message translation-friendly (hebasto)
- meowcoin-core/gui#332 Replace disambiguation strings with translator comments (hebasto)
- meowcoin-core/gui#335 test: Use QSignalSpy instead of QEventLoop (jarolrod)
- meowcoin-core/gui#343 Improve the GUI responsiveness when progress dialogs are used (hebasto)
- meowcoin-core/gui#361 Fix GUI segfault caused by meowcoin/meowcoin#22216 (ryanofsky)
- meowcoin-core/gui#362 Add keyboard shortcuts to context menus (luke-jr)
- meowcoin-core/gui#366 Dark Mode fixes/portability (luke-jr)
- meowcoin-core/gui#375 Emit dataChanged signal to dynamically re-sort Peers table (hebasto)
- meowcoin-core/gui#393 Fix regression in "Encrypt Wallet" menu item (hebasto)
- meowcoin-core/gui#396 Ensure external signer option remains disabled without signers (achow101)
- meowcoin-core/gui#406 Handle new added plurals in `meowcoin_en.ts` (hebasto)

### Build system
- meowcoin/meowcoin#17227 Add Android packaging support (icota)
- meowcoin/meowcoin#17920 guix: Build support for macOS (dongcarl)
- meowcoin/meowcoin#18298 Fix Qt processing of configure script for depends with DEBUG=1 (hebasto)
- meowcoin/meowcoin#19160 multiprocess: Add basic spawn and IPC support (ryanofsky)
- meowcoin/meowcoin#19504 Bump minimum python version to 3.6 (ajtowns)
- meowcoin/meowcoin#19522 fix building libconsensus with reduced exports for Darwin targets (fanquake)
- meowcoin/meowcoin#19683 Pin clang search paths for darwin host (dongcarl)
- meowcoin/meowcoin#19764 Split boost into build/host packages + bump + cleanup (dongcarl)
- meowcoin/meowcoin#19817 libtapi 1100.0.11 (fanquake)
- meowcoin/meowcoin#19846 enable unused member function diagnostic (Zero-1729)
- meowcoin/meowcoin#19867 Document and cleanup Qt hacks (fanquake)
- meowcoin/meowcoin#20046 Set `CMAKE_INSTALL_RPATH` for native packages (ryanofsky)
- meowcoin/meowcoin#20223 Drop the leading 0 from the version number (achow101)
- meowcoin/meowcoin#20333 Remove `native_biplist` dependency (fanquake)
- meowcoin/meowcoin#20353 configure: Support -fdebug-prefix-map and -fmacro-prefix-map (ajtowns)
- meowcoin/meowcoin#20359 Various config.site.in improvements and linting (dongcarl)
- meowcoin/meowcoin#20413 Require C++17 compiler (MarcoFalke)
- meowcoin/meowcoin#20419 Set minimum supported macOS to 10.14 (fanquake)
- meowcoin/meowcoin#20421 miniupnpc 2.2.2 (fanquake)
- meowcoin/meowcoin#20422 Mac deployment unification (fanquake)
- meowcoin/meowcoin#20424 Update univalue subtree (MarcoFalke)
- meowcoin/meowcoin#20449 Fix Windows installer build (achow101)
- meowcoin/meowcoin#20468 Warn when generating man pages for binaries built from a dirty branch (tylerchambers)
- meowcoin/meowcoin#20469 Avoid secp256k1.h include from system (dergoegge)
- meowcoin/meowcoin#20470 Replace genisoimage with xorriso (dongcarl)
- meowcoin/meowcoin#20471 Use C++17 in depends (fanquake)
- meowcoin/meowcoin#20496 Drop unneeded macOS framework dependencies (hebasto)
- meowcoin/meowcoin#20520 Do not force Precompiled Headers (PCH) for building Qt on Linux (hebasto)
- meowcoin/meowcoin#20549 Support make src/meowcoin-node and src/meowcoin-gui (promag)
- meowcoin/meowcoin#20565 Ensure PIC build for bdb on Android (BlockMechanic)
- meowcoin/meowcoin#20594 Fix getauxval calls in randomenv.cpp (jonasschnelli)
- meowcoin/meowcoin#20603 Update crc32c subtree (MarcoFalke)
- meowcoin/meowcoin#20609 configure: output notice that test binary is disabled by fuzzing (apoelstra)
- meowcoin/meowcoin#20619 guix: Quality of life improvements (dongcarl)
- meowcoin/meowcoin#20629 Improve id string robustness (dongcarl)
- meowcoin/meowcoin#20641 Use Qt top-level build facilities (hebasto)
- meowcoin/meowcoin#20650 Drop workaround for a fixed bug in Qt build system (hebasto)
- meowcoin/meowcoin#20673 Use more legible qmake commands in qt package (hebasto)
- meowcoin/meowcoin#20684 Define .INTERMEDIATE target once only (hebasto)
- meowcoin/meowcoin#20720 more robustly check for fcf-protection support (fanquake)
- meowcoin/meowcoin#20734 Make platform-specific targets available for proper platform builds only (hebasto)
- meowcoin/meowcoin#20936 build fuzz tests by default (danben)
- meowcoin/meowcoin#20937 guix: Make nsis reproducible by respecting SOURCE-DATE-EPOCH (dongcarl)
- meowcoin/meowcoin#20938 fix linking against -latomic when building for riscv (fanquake)
- meowcoin/meowcoin#20939 fix `RELOC_SECTION` security check for meowcoin-util (fanquake)
- meowcoin/meowcoin#20963 gitian-linux: Build binaries for 64-bit POWER (continued) (laanwj)
- meowcoin/meowcoin#21036 gitian: Bump descriptors to focal for 22.0 (fanquake)
- meowcoin/meowcoin#21045 Adds switch to enable/disable randomized base address in MSVC builds (EthanHeilman)
- meowcoin/meowcoin#21065 make macOS HOST in download-osx generic (fanquake)
- meowcoin/meowcoin#21078 guix: only download sources for hosts being built (fanquake)
- meowcoin/meowcoin#21116 Disable --disable-fuzz-binary for gitian/guix builds (hebasto)
- meowcoin/meowcoin#21182 remove mostly pointless `BOOST_PROCESS` macro (fanquake)
- meowcoin/meowcoin#21205 actually fail when Boost is missing (fanquake)
- meowcoin/meowcoin#21209 use newer source for libnatpmp (fanquake)
- meowcoin/meowcoin#21226 Fix fuzz binary compilation under windows (danben)
- meowcoin/meowcoin#21231 Add /opt/homebrew to path to look for boost libraries (fyquah)
- meowcoin/meowcoin#21239 guix: Add codesignature attachment support for osx+win (dongcarl)
- meowcoin/meowcoin#21250 Make `HAVE_O_CLOEXEC` available outside LevelDB (bugfix) (theStack)
- meowcoin/meowcoin#21272 guix: Passthrough `SDK_PATH` into container (dongcarl)
- meowcoin/meowcoin#21274 assumptions:  Assume C++17 (fanquake)
- meowcoin/meowcoin#21286 Bump minimum Qt version to 5.9.5 (hebasto)
- meowcoin/meowcoin#21298 guix: Bump time-machine, glibc, and linux-headers (dongcarl)
- meowcoin/meowcoin#21304 guix: Add guix-clean script + establish gc-root for container profiles (dongcarl)
- meowcoin/meowcoin#21320 fix libnatpmp macos cross compile (fanquake)
- meowcoin/meowcoin#21321 guix: Add curl to required tool list (hebasto)
- meowcoin/meowcoin#21333 set Unicode true for NSIS installer (fanquake)
- meowcoin/meowcoin#21339 Make `AM_CONDITIONAL([ENABLE_EXTERNAL_SIGNER])` unconditional (hebasto)
- meowcoin/meowcoin#21349 Fix fuzz-cuckoocache cross-compiling with DEBUG=1 (hebasto)
- meowcoin/meowcoin#21354 build, doc: Drop no longer required packages from macOS cross-compiling dependencies (hebasto)
- meowcoin/meowcoin#21363 build, qt: Improve Qt static plugins/libs check code (hebasto)
- meowcoin/meowcoin#21375 guix: Misc feedback-based fixes + hier restructuring (dongcarl)
- meowcoin/meowcoin#21376 Qt 5.12.10 (fanquake)
- meowcoin/meowcoin#21382 Clean remnants of QTBUG-34748 fix (hebasto)
- meowcoin/meowcoin#21400 Fix regression introduced in #21363 (hebasto)
- meowcoin/meowcoin#21403 set --build when configuring packages in depends (fanquake)
- meowcoin/meowcoin#21421 don't try and use -fstack-clash-protection on Windows (fanquake)
- meowcoin/meowcoin#21423 Cleanups and follow ups after bumping Qt to 5.12.10 (hebasto)
- meowcoin/meowcoin#21427 Fix `id_string` invocations (dongcarl)
- meowcoin/meowcoin#21430 Add -Werror=implicit-fallthrough compile flag (hebasto)
- meowcoin/meowcoin#21457 Split libtapi and clang out of `native_cctools` (fanquake)
- meowcoin/meowcoin#21462 guix: Add guix-{attest,verify} scripts (dongcarl)
- meowcoin/meowcoin#21495 build, qt: Fix static builds on macOS Big Sur (hebasto)
- meowcoin/meowcoin#21497 Do not opt-in unused CoreWLAN stuff in depends for macOS (hebasto)
- meowcoin/meowcoin#21543 Enable safe warnings for msvc builds (hebasto)
- meowcoin/meowcoin#21565 Make `meowcoin_qt.m4` more generic (fanquake)
- meowcoin/meowcoin#21610 remove -Wdeprecated-register from NOWARN flags (fanquake)
- meowcoin/meowcoin#21613 enable -Wdocumentation (fanquake)
- meowcoin/meowcoin#21629 Fix configuring when building depends with `NO_BDB=1` (fanquake)
- meowcoin/meowcoin#21654 build, qt: Make Qt rcc output always deterministic (hebasto)
- meowcoin/meowcoin#21655 build, qt: No longer need to set `QT_RCC_TEST=1` for determinism (hebasto)
- meowcoin/meowcoin#21658 fix make deploy for arm64-darwin (sgulls)
- meowcoin/meowcoin#21694 Use XLIFF file to provide more context to Transifex translators (hebasto)
- meowcoin/meowcoin#21708, meowcoin/meowcoin#21593 Drop pointless sed commands (hebasto)
- meowcoin/meowcoin#21731 Update msvc build to use Qt5.12.10 binaries (sipsorcery)
- meowcoin/meowcoin#21733 Re-add command to install vcpkg (dplusplus1024)
- meowcoin/meowcoin#21793 Use `-isysroot` over `--sysroot` on macOS (fanquake)
- meowcoin/meowcoin#21869 Add missing `-D_LIBCPP_DEBUG=1` to debug flags (MarcoFalke)
- meowcoin/meowcoin#21889 macho: check for control flow instrumentation (fanquake)
- meowcoin/meowcoin#21920 Improve macro for testing -latomic requirement (MarcoFalke)
- meowcoin/meowcoin#21991 libevent 2.1.12-stable (fanquake)
- meowcoin/meowcoin#22054 Bump Qt version to 5.12.11 (hebasto)
- meowcoin/meowcoin#22063 Use Qt archive of the same version as the compiled binaries (hebasto)
- meowcoin/meowcoin#22070 Don't use cf-protection when targeting arm-apple-darwin (fanquake)
- meowcoin/meowcoin#22071 Latest config.guess and config.sub (fanquake)
- meowcoin/meowcoin#22075 guix: Misc leftover usability improvements (dongcarl)
- meowcoin/meowcoin#22123 Fix qt.mk for mac arm64 (promag)
- meowcoin/meowcoin#22174 build, qt: Fix libraries linking order for Linux hosts (hebasto)
- meowcoin/meowcoin#22182 guix: Overhaul how guix-{attest,verify} works and hierarchy (dongcarl)
- meowcoin/meowcoin#22186 build, qt: Fix compiling qt package in depends with GCC 11 (hebasto)
- meowcoin/meowcoin#22199 macdeploy: minor fixups and simplifications (fanquake)
- meowcoin/meowcoin#22230 Fix MSVC linker /SubSystem option for meowcoin-qt.exe (hebasto)
- meowcoin/meowcoin#22234 Mark print-% target as phony (dgoncharov)
- meowcoin/meowcoin#22238 improve detection of eBPF support (fanquake)
- meowcoin/meowcoin#22258 Disable deprecated-copy warning only when external warnings are enabled (MarcoFalke)
- meowcoin/meowcoin#22320 set minimum required Boost to 1.64.0 (fanquake)
- meowcoin/meowcoin#22348 Fix cross build for Windows with Boost Process (hebasto)
- meowcoin/meowcoin#22365 guix: Avoid relying on newer symbols by rebasing our cross toolchains on older glibcs (dongcarl)
- meowcoin/meowcoin#22381 guix: Test security-check sanity before performing them (with macOS) (fanquake)
- meowcoin/meowcoin#22405 Remove --enable-glibc-back-compat from Guix build (fanquake)
- meowcoin/meowcoin#22406 Remove --enable-determinism configure option (fanquake)
- meowcoin/meowcoin#22410 Avoid GCC 7.1 ABI change warning in guix build (sipa)
- meowcoin/meowcoin#22436 use aarch64 Clang if cross-compiling for darwin on aarch64 (fanquake)
- meowcoin/meowcoin#22465 guix: Pin kernel-header version, time-machine to upstream 1.3.0 commit (dongcarl)
- meowcoin/meowcoin#22511 guix: Silence `getent(1)` invocation, doc fixups (dongcarl)
- meowcoin/meowcoin#22531 guix: Fixes to guix-{attest,verify} (achow101)
- meowcoin/meowcoin#22642 release: Release with separate sha256sums and sig files (dongcarl)
- meowcoin/meowcoin#22685 clientversion: No suffix `#if CLIENT_VERSION_IS_RELEASE` (dongcarl)
- meowcoin/meowcoin#22713 Fix build with Boost 1.77.0 (sizeofvoid)

### Tests and QA
- meowcoin/meowcoin#14604 Add test and refactor `feature_block.py` (sanket1729)
- meowcoin/meowcoin#17556 Change `feature_config_args.py` not to rely on strange regtest=0 behavior (ryanofsky)
- meowcoin/meowcoin#18795 wallet issue with orphaned rewards (domob1812)
- meowcoin/meowcoin#18847 compressor: Use a prevector in CompressScript serialization (jb55)
- meowcoin/meowcoin#19259 fuzz: Add fuzzing harness for LoadMempool(…) and DumpMempool(…) (practicalswift)
- meowcoin/meowcoin#19315 Allow outbound & block-relay-only connections in functional tests. (amitiuttarwar)
- meowcoin/meowcoin#19698 Apply strict verification flags for transaction tests and assert backwards compatibility (glozow)
- meowcoin/meowcoin#19801 Check for all possible `OP_CLTV` fail reasons in `feature_cltv.py` (BIP 65) (theStack)
- meowcoin/meowcoin#19893 Remove or explain syncwithvalidationinterfacequeue (MarcoFalke)
- meowcoin/meowcoin#19972 fuzz: Add fuzzing harness for node eviction logic (practicalswift)
- meowcoin/meowcoin#19982 Fix inconsistent lock order in `wallet_tests/CreateWallet` (hebasto)
- meowcoin/meowcoin#20000 Fix creation of "std::string"s with \0s (vasild)
- meowcoin/meowcoin#20047 Use `wait_for_{block,header}` helpers in `p2p_fingerprint.py` (theStack)
- meowcoin/meowcoin#20171 Add functional test `test_txid_inv_delay` (ariard)
- meowcoin/meowcoin#20189 Switch to BIP341's suggested scheme for outputs without script (sipa)
- meowcoin/meowcoin#20248 Fix length of R check in `key_signature_tests` (dgpv)
- meowcoin/meowcoin#20276, meowcoin/meowcoin#20385, meowcoin/meowcoin#20688, meowcoin/meowcoin#20692 Run various mempool tests even with wallet disabled (mjdietzx)
- meowcoin/meowcoin#20323 Create or use existing properly initialized NodeContexts (dongcarl)
- meowcoin/meowcoin#20354 Add `feature_taproot.py --previous_release` (MarcoFalke)
- meowcoin/meowcoin#20370 fuzz: Version handshake (MarcoFalke)
- meowcoin/meowcoin#20377 fuzz: Fill various small fuzzing gaps (practicalswift)
- meowcoin/meowcoin#20425 fuzz: Make CAddrMan fuzzing harness deterministic (practicalswift)
- meowcoin/meowcoin#20430 Sanitizers: Add suppression for unsigned-integer-overflow in libstdc++ (jonasschnelli)
- meowcoin/meowcoin#20437 fuzz: Avoid time-based "non-determinism" in fuzzing harnesses by using mocked GetTime() (practicalswift)
- meowcoin/meowcoin#20458 Add `is_bdb_compiled` helper (Sjors)
- meowcoin/meowcoin#20466 Fix intermittent `p2p_fingerprint` issue (MarcoFalke)
- meowcoin/meowcoin#20472 Add testing of ParseInt/ParseUInt edge cases with leading +/-/0:s (practicalswift)
- meowcoin/meowcoin#20507 sync: print proper lock order location when double lock is detected (vasild)
- meowcoin/meowcoin#20522 Fix sync issue in `disconnect_p2ps` (amitiuttarwar)
- meowcoin/meowcoin#20524 Move `MIN_VERSION_SUPPORTED` to p2p.py (jnewbery)
- meowcoin/meowcoin#20540 Fix `wallet_multiwallet` issue on windows (MarcoFalke)
- meowcoin/meowcoin#20560 fuzz: Link all targets once (MarcoFalke)
- meowcoin/meowcoin#20567 Add option to git-subtree-check to do full check, add help (laanwj)
- meowcoin/meowcoin#20569 Fix intermittent `wallet_multiwallet` issue with `got_loading_error` (MarcoFalke)
- meowcoin/meowcoin#20613 Use Popen.wait instead of RPC in `assert_start_raises_init_error` (MarcoFalke)
- meowcoin/meowcoin#20663 fuzz: Hide `script_assets_test_minimizer` (MarcoFalke)
- meowcoin/meowcoin#20674 fuzz: Call SendMessages after ProcessMessage to increase coverage (MarcoFalke)
- meowcoin/meowcoin#20683 Fix restart node race (MarcoFalke)
- meowcoin/meowcoin#20686 fuzz: replace CNode code with fuzz/util.h::ConsumeNode() (jonatack)
- meowcoin/meowcoin#20733 Inline non-member functions with body in fuzzing headers (pstratem)
- meowcoin/meowcoin#20737 Add missing assignment in `mempool_resurrect.py` (MarcoFalke)
- meowcoin/meowcoin#20745 Correct `epoll_ctl` data race suppression (hebasto)
- meowcoin/meowcoin#20748 Add race:SendZmqMessage tsan suppression (MarcoFalke)
- meowcoin/meowcoin#20760 Set correct nValue for multi-op-return policy check (MarcoFalke)
- meowcoin/meowcoin#20761 fuzz: Check that `NULL_DATA` is unspendable (MarcoFalke)
- meowcoin/meowcoin#20765 fuzz: Check that certain script TxoutType are nonstandard (mjdietzx)
- meowcoin/meowcoin#20772 fuzz: Bolster ExtractDestination(s) checks (mjdietzx)
- meowcoin/meowcoin#20789 fuzz: Rework strong and weak net enum fuzzing (MarcoFalke)
- meowcoin/meowcoin#20828 fuzz: Introduce CallOneOf helper to replace switch-case (MarcoFalke)
- meowcoin/meowcoin#20839 fuzz: Avoid extraneous copy of input data, using Span<> (MarcoFalke)
- meowcoin/meowcoin#20844 Add sanitizer suppressions for AMD EPYC CPUs (MarcoFalke)
- meowcoin/meowcoin#20857 Update documentation in `feature_csv_activation.py` (PiRK)
- meowcoin/meowcoin#20876 Replace getmempoolentry with testmempoolaccept in MiniWallet (MarcoFalke)
- meowcoin/meowcoin#20881 fuzz: net permission flags in net processing (MarcoFalke)
- meowcoin/meowcoin#20882 fuzz: Add missing muhash registration (MarcoFalke)
- meowcoin/meowcoin#20908 fuzz: Use mocktime in `process_message*` fuzz targets (MarcoFalke)
- meowcoin/meowcoin#20915 fuzz: Fail if message type is not fuzzed (MarcoFalke)
- meowcoin/meowcoin#20946 fuzz: Consolidate fuzzing TestingSetup initialization (dongcarl)
- meowcoin/meowcoin#20954 Declare `nodes` type `in test_framework.py` (kiminuo)
- meowcoin/meowcoin#20955 Fix `get_previous_releases.py` for aarch64 (MarcoFalke)
- meowcoin/meowcoin#20969 check that getblockfilter RPC fails without block filter index (theStack)
- meowcoin/meowcoin#20971 Work around libFuzzer deadlock (MarcoFalke)
- meowcoin/meowcoin#20993 Store subversion (user agent) as string in `msg_version` (theStack)
- meowcoin/meowcoin#20995 fuzz: Avoid initializing version to less than `MIN_PEER_PROTO_VERSION` (MarcoFalke)
- meowcoin/meowcoin#20998 Fix BlockToJsonVerbose benchmark (martinus)
- meowcoin/meowcoin#21003 Move MakeNoLogFileContext to `libtest_util`, and use it in bench (MarcoFalke)
- meowcoin/meowcoin#21008 Fix zmq test flakiness, improve speed (theStack)
- meowcoin/meowcoin#21023 fuzz: Disable shuffle when merge=1 (MarcoFalke)
- meowcoin/meowcoin#21037 fuzz: Avoid designated initialization (C++20) in fuzz tests (practicalswift)
- meowcoin/meowcoin#21042 doc, test: Improve `setup_clean_chain` documentation (fjahr)
- meowcoin/meowcoin#21080 fuzz: Configure check for main function (take 2) (MarcoFalke)
- meowcoin/meowcoin#21084 Fix timeout decrease in `feature_assumevalid` (brunoerg)
- meowcoin/meowcoin#21096 Re-add dead code detection (flack)
- meowcoin/meowcoin#21100 Remove unused function `xor_bytes` (theStack)
- meowcoin/meowcoin#21115 Fix Windows cross build (hebasto)
- meowcoin/meowcoin#21117 Remove `assert_blockchain_height` (MarcoFalke)
- meowcoin/meowcoin#21121 Small unit test improvements, including helper to make mempool transaction (amitiuttarwar)
- meowcoin/meowcoin#21124 Remove unnecessary assignment in bdb (brunoerg)
- meowcoin/meowcoin#21125 Change `BOOST_CHECK` to `BOOST_CHECK_EQUAL` for paths (kiminuo)
- meowcoin/meowcoin#21142, meowcoin/meowcoin#21512 fuzz: Add `tx_pool` fuzz target (MarcoFalke)
- meowcoin/meowcoin#21165 Use mocktime in `test_seed_peers` (dhruv)
- meowcoin/meowcoin#21169 fuzz: Add RPC interface fuzzing. Increase fuzzing coverage from 65% to 70% (practicalswift)
- meowcoin/meowcoin#21170 bench: Add benchmark to write json into a string (martinus)
- meowcoin/meowcoin#21178 Run `mempool_reorg.py` even with wallet disabled (DariusParvin)
- meowcoin/meowcoin#21185 fuzz: Remove expensive and redundant muhash from crypto fuzz target (MarcoFalke)
- meowcoin/meowcoin#21200 Speed up `rpc_blockchain.py` by removing miniwallet.generate() (MarcoFalke)
- meowcoin/meowcoin#21211 Move `P2WSH_OP_TRUE` to shared test library (MarcoFalke)
- meowcoin/meowcoin#21228 Avoid comparision of integers with different signs (jonasschnelli)
- meowcoin/meowcoin#21230 Fix `NODE_NETWORK_LIMITED_MIN_BLOCKS` disconnection (MarcoFalke)
- meowcoin/meowcoin#21252 Add missing wait for sync to `feature_blockfilterindex_prune` (MarcoFalke)
- meowcoin/meowcoin#21254 Avoid connecting to real network when running tests (MarcoFalke)
- meowcoin/meowcoin#21264 fuzz: Two scripted diff renames (MarcoFalke)
- meowcoin/meowcoin#21280 Bug fix in `transaction_tests` (glozow)
- meowcoin/meowcoin#21293 Replace accidentally placed bit-OR with logical-OR (hebasto)
- meowcoin/meowcoin#21297 `feature_blockfilterindex_prune.py` improvements (jonatack)
- meowcoin/meowcoin#21310 zmq test: fix sync-up by matching notification to generated block (theStack)
- meowcoin/meowcoin#21334 Additional BIP9 tests (Sjors)
- meowcoin/meowcoin#21338 Add functional test for anchors.dat (brunoerg)
- meowcoin/meowcoin#21345 Bring `p2p_leak.py` up to date (mzumsande)
- meowcoin/meowcoin#21357 Unconditionally check for fRelay field in test framework (jarolrod)
- meowcoin/meowcoin#21358 fuzz: Add missing include (`test/util/setup_common.h`) (MarcoFalke)
- meowcoin/meowcoin#21371 fuzz: fix gcc Woverloaded-virtual build warnings (jonatack)
- meowcoin/meowcoin#21373 Generate fewer blocks in `feature_nulldummy` to fix timeouts, speed up (jonatack)
- meowcoin/meowcoin#21390 Test improvements for UTXO set hash tests (fjahr)
- meowcoin/meowcoin#21410 increase `rpc_timeout` for fundrawtx `test_transaction_too_large` (jonatack)
- meowcoin/meowcoin#21411 add logging, reduce blocks, move `sync_all` in `wallet_` groups (jonatack)
- meowcoin/meowcoin#21438 Add ParseUInt8() test coverage (jonatack)
- meowcoin/meowcoin#21443 fuzz: Implement `fuzzed_dns_lookup_function` as a lambda (practicalswift)
- meowcoin/meowcoin#21445 cirrus: Use SSD cluster for speedup (MarcoFalke)
- meowcoin/meowcoin#21477 Add test for CNetAddr::ToString IPv6 address formatting (RFC 5952) (practicalswift)
- meowcoin/meowcoin#21487 fuzz: Use ConsumeWeakEnum in addrman for service flags (MarcoFalke)
- meowcoin/meowcoin#21488 Add ParseUInt16() unit test and fuzz coverage (jonatack)
- meowcoin/meowcoin#21491 test: remove duplicate assertions in util_tests (jonatack)
- meowcoin/meowcoin#21522 fuzz: Use PickValue where possible (MarcoFalke)
- meowcoin/meowcoin#21531 remove qt byteswap compattests (fanquake)
- meowcoin/meowcoin#21557 small cleanup in RPCNestedTests tests (fanquake)
- meowcoin/meowcoin#21586 Add missing suppression for signed-integer-overflow:txmempool.cpp (MarcoFalke)
- meowcoin/meowcoin#21592 Remove option to make TestChain100Setup non-deterministic (MarcoFalke)
- meowcoin/meowcoin#21597 Document `race:validation_chainstatemanager_tests` suppression (MarcoFalke)
- meowcoin/meowcoin#21599 Replace file level integer overflow suppression with function level suppression (practicalswift)
- meowcoin/meowcoin#21604 Document why no symbol names can be used for suppressions (MarcoFalke)
- meowcoin/meowcoin#21606 fuzz: Extend psmt fuzz target a bit (MarcoFalke)
- meowcoin/meowcoin#21617 fuzz: Fix uninitialized read in i2p test (MarcoFalke)
- meowcoin/meowcoin#21630 fuzz: split FuzzedSock interface and implementation (vasild)
- meowcoin/meowcoin#21634 Skip SQLite fsyncs while testing (achow101)
- meowcoin/meowcoin#21669 Remove spurious double lock tsan suppressions by bumping to clang-12 (MarcoFalke)
- meowcoin/meowcoin#21676 Use mocktime to avoid intermittent failure in `rpc_tests` (MarcoFalke)
- meowcoin/meowcoin#21677 fuzz: Avoid use of low file descriptor ids (which may be in use) in FuzzedSock (practicalswift)
- meowcoin/meowcoin#21678 Fix TestPotentialDeadLockDetected suppression (hebasto)
- meowcoin/meowcoin#21689 Remove intermittently failing and not very meaningful `BOOST_CHECK` in `cnetaddr_basic` (practicalswift)
- meowcoin/meowcoin#21691 Check that no versionbits are re-used (MarcoFalke)
- meowcoin/meowcoin#21707 Extend functional tests for addr relay (mzumsande)
- meowcoin/meowcoin#21712 Test default `include_mempool` value of gettxout (promag)
- meowcoin/meowcoin#21738 Use clang-12 for ASAN, Add missing suppression (MarcoFalke)
- meowcoin/meowcoin#21740 add new python linter to check file names and permissions (windsok)
- meowcoin/meowcoin#21749 Bump shellcheck version (hebasto)
- meowcoin/meowcoin#21754 Run `feature_cltv` with MiniWallet (MarcoFalke)
- meowcoin/meowcoin#21762 Speed up `mempool_spend_coinbase.py` (MarcoFalke)
- meowcoin/meowcoin#21773 fuzz: Ensure prevout is consensus-valid (MarcoFalke)
- meowcoin/meowcoin#21777 Fix `feature_notifications.py` intermittent issue (MarcoFalke)
- meowcoin/meowcoin#21785 Fix intermittent issue in `p2p_addr_relay.py` (MarcoFalke)
- meowcoin/meowcoin#21787 Fix off-by-ones in `rpc_fundrawtransaction` assertions (jonatack)
- meowcoin/meowcoin#21792 Fix intermittent issue in `p2p_segwit.py` (MarcoFalke)
- meowcoin/meowcoin#21795 fuzz: Terminate immediately if a fuzzing harness tries to perform a DNS lookup (belt and suspenders) (practicalswift)
- meowcoin/meowcoin#21798 fuzz: Create a block template in `tx_pool` targets (MarcoFalke)
- meowcoin/meowcoin#21804 Speed up `p2p_segwit.py` (jnewbery)
- meowcoin/meowcoin#21810 fuzz: Various RPC fuzzer follow-ups (practicalswift)
- meowcoin/meowcoin#21814 Fix `feature_config_args.py` intermittent issue (MarcoFalke)
- meowcoin/meowcoin#21821 Add missing test for empty P2WSH redeem (MarcoFalke)
- meowcoin/meowcoin#21822 Resolve bug in `interface_meowcoin_cli.py` (klementtan)
- meowcoin/meowcoin#21846 fuzz: Add `-fsanitize=integer` suppression needed for RPC fuzzer (`generateblock`) (practicalswift)
- meowcoin/meowcoin#21849 fuzz: Limit toxic test globals to their respective scope (MarcoFalke)
- meowcoin/meowcoin#21867 use MiniWallet for `p2p_blocksonly.py` (theStack)
- meowcoin/meowcoin#21873 minor fixes & improvements for files linter test (windsok)
- meowcoin/meowcoin#21874 fuzz: Add `WRITE_ALL_FUZZ_TARGETS_AND_ABORT` (MarcoFalke)
- meowcoin/meowcoin#21884 fuzz: Remove unused --enable-danger-fuzz-link-all option (MarcoFalke)
- meowcoin/meowcoin#21890 fuzz: Limit ParseISO8601DateTime fuzzing to 32-bit (MarcoFalke)
- meowcoin/meowcoin#21891 fuzz: Remove strprintf test cases that are known to fail (MarcoFalke)
- meowcoin/meowcoin#21892 fuzz: Avoid excessively large min fee rate in `tx_pool` (MarcoFalke)
- meowcoin/meowcoin#21895 Add TSA annotations to the WorkQueue class members (hebasto)
- meowcoin/meowcoin#21900 use MiniWallet for `feature_csv_activation.py` (theStack)
- meowcoin/meowcoin#21909 fuzz: Limit max insertions in timedata fuzz test (MarcoFalke)
- meowcoin/meowcoin#21922 fuzz: Avoid timeout in EncodeBase58 (MarcoFalke)
- meowcoin/meowcoin#21927 fuzz: Run const CScript member functions only once (MarcoFalke)
- meowcoin/meowcoin#21929 fuzz: Remove incorrect float round-trip serialization test (MarcoFalke)
- meowcoin/meowcoin#21936 fuzz: Terminate immediately if a fuzzing harness tries to create a TCP socket (belt and suspenders) (practicalswift)
- meowcoin/meowcoin#21941 fuzz: Call const member functions in addrman fuzz test only once (MarcoFalke)
- meowcoin/meowcoin#21945 add P2PK support to MiniWallet (theStack)
- meowcoin/meowcoin#21948 Fix off-by-one in mockscheduler test RPC (MarcoFalke)
- meowcoin/meowcoin#21953 fuzz: Add `utxo_snapshot` target (MarcoFalke)
- meowcoin/meowcoin#21970 fuzz: Add missing CheckTransaction before CheckTxInputs (MarcoFalke)
- meowcoin/meowcoin#21989 Use `COINBASE_MATURITY` in functional tests (kiminuo)
- meowcoin/meowcoin#22003 Add thread safety annotations (ajtowns)
- meowcoin/meowcoin#22004 fuzz: Speed up transaction fuzz target (MarcoFalke)
- meowcoin/meowcoin#22005 fuzz: Speed up banman fuzz target (MarcoFalke)
- meowcoin/meowcoin#22029 [fuzz] Improve transport deserialization fuzz test coverage (dhruv)
- meowcoin/meowcoin#22048 MiniWallet: introduce enum type for output mode (theStack)
- meowcoin/meowcoin#22057 use MiniWallet (P2PK mode) for `feature_dersig.py` (theStack)
- meowcoin/meowcoin#22065 Mark `CheckTxInputs` `[[nodiscard]]`. Avoid UUM in fuzzing harness `coins_view` (practicalswift)
- meowcoin/meowcoin#22069 fuzz: don't try and use fopencookie() when building for Android (fanquake)
- meowcoin/meowcoin#22082 update nanobench from release 4.0.0 to 4.3.4 (martinus)
- meowcoin/meowcoin#22086 remove BasicTestingSetup from unit tests that don't need it (fanquake)
- meowcoin/meowcoin#22089 MiniWallet: fix fee calculation for P2PK and check tx vsize (theStack)
- meowcoin/meowcoin#21107, meowcoin/meowcoin#22092 Convert documentation into type annotations (fanquake)
- meowcoin/meowcoin#22095 Additional BIP32 test vector for hardened derivation with leading zeros (kristapsk)
- meowcoin/meowcoin#22103 Fix IPv6 check on BSD systems (n-thumann)
- meowcoin/meowcoin#22118 check anchors.dat when node starts for the first time (brunoerg)
- meowcoin/meowcoin#22120 `p2p_invalid_block`: Check that a block rejected due to too-new tim… (willcl-ark)
- meowcoin/meowcoin#22153 Fix `p2p_leak.py` intermittent failure (mzumsande)
- meowcoin/meowcoin#22169 p2p, rpc, fuzz: various tiny follow-ups (jonatack)
- meowcoin/meowcoin#22176 Correct outstanding -Werror=sign-compare errors (Empact)
- meowcoin/meowcoin#22180 fuzz: Increase branch coverage of the float fuzz target (MarcoFalke)
- meowcoin/meowcoin#22187 Add `sync_blocks` in `wallet_orphanedreward.py` (domob1812)
- meowcoin/meowcoin#22201 Fix TestShell to allow running in Jupyter Notebook (josibake)
- meowcoin/meowcoin#22202 Add temporary coinstats suppressions (MarcoFalke)
- meowcoin/meowcoin#22203 Use ConnmanTestMsg from test lib in `denialofservice_tests` (MarcoFalke)
- meowcoin/meowcoin#22210 Use MiniWallet in `test_no_inherited_signaling` RBF test (MarcoFalke)
- meowcoin/meowcoin#22224 Update msvc and appveyor builds to use Qt5.12.11 binaries (sipsorcery)
- meowcoin/meowcoin#22249 Kill process group to avoid dangling processes when using `--failfast` (S3RK)
- meowcoin/meowcoin#22267 fuzz: Speed up crypto fuzz target (MarcoFalke)
- meowcoin/meowcoin#22270 Add meowcoin-util tests (+refactors) (MarcoFalke)
- meowcoin/meowcoin#22271 fuzz: Assert roundtrip equality for `CPubKey` (theStack)
- meowcoin/meowcoin#22279 fuzz: add missing ECCVerifyHandle to `base_encode_decode` (apoelstra)
- meowcoin/meowcoin#22292 bench, doc: benchmarking updates and fixups (jonatack)
- meowcoin/meowcoin#22306 Improvements to `p2p_addr_relay.py` (amitiuttarwar)
- meowcoin/meowcoin#22310 Add functional test for replacement relay fee check (ariard)
- meowcoin/meowcoin#22311 Add missing syncwithvalidationinterfacequeue in `p2p_blockfilters` (MarcoFalke)
- meowcoin/meowcoin#22313 Add missing `sync_all` to `feature_coinstatsindex` (MarcoFalke)
- meowcoin/meowcoin#22322 fuzz: Check banman roundtrip (MarcoFalke)
- meowcoin/meowcoin#22363 Use `script_util` helpers for creating P2{PKH,SH,WPKH,WSH} scripts (theStack)
- meowcoin/meowcoin#22399 fuzz: Rework CTxDestination fuzzing (MarcoFalke)
- meowcoin/meowcoin#22408 add tests for `bad-txns-prevout-null` reject reason (theStack)
- meowcoin/meowcoin#22445 fuzz: Move implementations of non-template fuzz helpers from util.h to util.cpp (sriramdvt)
- meowcoin/meowcoin#22446 Fix `wallet_listdescriptors.py` if bdb is not compiled (hebasto)
- meowcoin/meowcoin#22447 Whitelist `rpc_rawtransaction` peers to speed up tests (jonatack)
- meowcoin/meowcoin#22742 Use proper target in `do_fund_send` (S3RK)

### Miscellaneous
- meowcoin/meowcoin#19337 sync: Detect double lock from the same thread (vasild)
- meowcoin/meowcoin#19809 log: Prefix log messages with function name and source code location if -logsourcelocations is set (practicalswift)
- meowcoin/meowcoin#19866 eBPF Linux tracepoints (jb55)
- meowcoin/meowcoin#20024 init: Fix incorrect warning "Reducing -maxconnections from N to N-1, because of system limitations" (practicalswift)
- meowcoin/meowcoin#20145 contrib: Add getcoins.py script to get coins from (signet) faucet (kallewoof)
- meowcoin/meowcoin#20255 util: Add assume() identity function (MarcoFalke)
- meowcoin/meowcoin#20288 script, doc: Contrib/seeds updates (jonatack)
- meowcoin/meowcoin#20358 src/randomenv.cpp: Fix build on uclibc (ffontaine)
- meowcoin/meowcoin#20406 util: Avoid invalid integer negation in formatmoney and valuefromamount (practicalswift)
- meowcoin/meowcoin#20434 contrib: Parse elf directly for symbol and security checks (laanwj)
- meowcoin/meowcoin#20451 lint: Run mypy over contrib/devtools (fanquake)
- meowcoin/meowcoin#20476 contrib: Add test for elf symbol-check (laanwj)
- meowcoin/meowcoin#20530 lint: Update cppcheck linter to c++17 and improve explicit usage (fjahr)
- meowcoin/meowcoin#20589 log: Clarify that failure to read/write `fee_estimates.dat` is non-fatal (MarcoFalke)
- meowcoin/meowcoin#20602 util: Allow use of c++14 chrono literals (MarcoFalke)
- meowcoin/meowcoin#20605 init: Signal-safe instant shutdown (laanwj)
- meowcoin/meowcoin#20608 contrib: Add symbol check test for PE binaries (fanquake)
- meowcoin/meowcoin#20689 contrib: Replace binary verification script verify.sh with python rewrite (theStack)
- meowcoin/meowcoin#20715 util: Add argsmanager::getcommand() and use it in meowcoin-wallet (MarcoFalke)
- meowcoin/meowcoin#20735 script: Remove outdated extract-osx-sdk.sh (hebasto)
- meowcoin/meowcoin#20817 lint: Update list of spelling linter false positives, bump to codespell 2.0.0 (theStack)
- meowcoin/meowcoin#20884 script: Improve robustness of meowcoind.service on startup (hebasto)
- meowcoin/meowcoin#20906 contrib: Embed c++11 patch in `install_db4.sh` (gruve-p)
- meowcoin/meowcoin#21004 contrib: Fix docker args conditional in gitian-build (setpill)
- meowcoin/meowcoin#21007 meowcoind: Add -daemonwait option to wait for initialization (laanwj)
- meowcoin/meowcoin#21041 log: Move "Pre-allocating up to position 0x[…] in […].dat" log message to debug category (practicalswift)
- meowcoin/meowcoin#21059 Drop boost/preprocessor dependencies (hebasto)
- meowcoin/meowcoin#21087 guix: Passthrough `BASE_CACHE` into container (dongcarl)
- meowcoin/meowcoin#21088 guix: Jump forwards in time-machine and adapt (dongcarl)
- meowcoin/meowcoin#21089 guix: Add support for powerpc64{,le} (dongcarl)
- meowcoin/meowcoin#21110 util: Remove boost `posix_time` usage from `gettime*` (fanquake)
- meowcoin/meowcoin#21111 Improve OpenRC initscript (parazyd)
- meowcoin/meowcoin#21123 code style: Add EditorConfig file (kiminuo)
- meowcoin/meowcoin#21173 util: Faster hexstr => 13% faster blocktojson (martinus)
- meowcoin/meowcoin#21221 tools: Allow argument/parameter bin packing in clang-format (jnewbery)
- meowcoin/meowcoin#21244 Move GetDataDir to ArgsManager (kiminuo)
- meowcoin/meowcoin#21255 contrib: Run test-symbol-check for risc-v (fanquake)
- meowcoin/meowcoin#21271 guix: Explicitly set umask in build container (dongcarl)
- meowcoin/meowcoin#21300 script: Add explanatory comment to tc.sh (dscotese)
- meowcoin/meowcoin#21317 util: Make assume() usable as unary expression (MarcoFalke)
- meowcoin/meowcoin#21336 Make .gitignore ignore src/test/fuzz/fuzz.exe (hebasto)
- meowcoin/meowcoin#21337 guix: Update darwin native packages dependencies (hebasto)
- meowcoin/meowcoin#21405 compat: remove memcpy -> memmove backwards compatibility alias (fanquake)
- meowcoin/meowcoin#21418 contrib: Make systemd invoke dependencies only when ready (laanwj)
- meowcoin/meowcoin#21447 Always add -daemonwait to known command line arguments (hebasto)
- meowcoin/meowcoin#21471 bugfix: Fix `bech32_encode` calls in `gen_key_io_test_vectors.py` (sipa)
- meowcoin/meowcoin#21615 script: Add trusted key for hebasto (hebasto)
- meowcoin/meowcoin#21664 contrib: Use lief for macos and windows symbol & security checks (fanquake)
- meowcoin/meowcoin#21695 contrib: Remove no longer used contrib/meowcoin-qt.pro (hebasto)
- meowcoin/meowcoin#21711 guix: Add full installation and usage documentation (dongcarl)
- meowcoin/meowcoin#21799 guix: Use `gcc-8` across the board (dongcarl)
- meowcoin/meowcoin#21802 Avoid UB in util/asmap (advance a dereferenceable iterator outside its valid range) (MarcoFalke)
- meowcoin/meowcoin#21823 script: Update reviewers (jonatack)
- meowcoin/meowcoin#21850 Remove `GetDataDir(net_specific)` function (kiminuo)
- meowcoin/meowcoin#21871 scripts: Add checks for minimum required os versions (fanquake)
- meowcoin/meowcoin#21966 Remove double serialization; use software encoder for fee estimation (sipa)
- meowcoin/meowcoin#22060 contrib: Add torv3 seed nodes for testnet, drop v2 ones (laanwj)
- meowcoin/meowcoin#22244 devtools: Correctly extract symbol versions in symbol-check (laanwj)
- meowcoin/meowcoin#22533 guix/build: Remove vestigial SKIPATTEST.TAG (dongcarl)
- meowcoin/meowcoin#22643 guix-verify: Non-zero exit code when anything fails (dongcarl)
- meowcoin/meowcoin#22654 guix: Don't include directory name in SHA256SUMS (achow101)

### Documentation
- meowcoin/meowcoin#15451 clarify getdata limit after #14897 (HashUnlimited)
- meowcoin/meowcoin#15545 Explain why CheckBlock() is called before AcceptBlock (Sjors)
- meowcoin/meowcoin#17350 Add developer documentation to isminetype (HAOYUatHZ)
- meowcoin/meowcoin#17934 Use `CONFIG_SITE` variable instead of --prefix option (hebasto)
- meowcoin/meowcoin#18030 Coin::IsSpent() can also mean never existed (Sjors)
- meowcoin/meowcoin#18096 IsFinalTx comment about nSequence & `OP_CLTV` (nothingmuch)
- meowcoin/meowcoin#18568 Clarify developer notes about constant naming (ryanofsky)
- meowcoin/meowcoin#19961 doc: tor.md updates (jonatack)
- meowcoin/meowcoin#19968 Clarify CRollingBloomFilter size estimate (robot-dreams)
- meowcoin/meowcoin#20200 Rename CODEOWNERS to REVIEWERS (adamjonas)
- meowcoin/meowcoin#20329 docs/descriptors.md: Remove hardened marker in the path after xpub (dgpv)
- meowcoin/meowcoin#20380 Add instructions on how to fuzz the P2P layer using Honggfuzz NetDriver (practicalswift)
- meowcoin/meowcoin#20414 Remove generated manual pages from master branch (laanwj)
- meowcoin/meowcoin#20473 Document current boost dependency as 1.71.0 (laanwj)
- meowcoin/meowcoin#20512 Add bash as an OpenBSD dependency (emilengler)
- meowcoin/meowcoin#20568 Use FeeModes doc helper in estimatesmartfee (MarcoFalke)
- meowcoin/meowcoin#20577 libconsensus: add missing error code description, fix NBitcoin link (theStack)
- meowcoin/meowcoin#20587 Tidy up Tor doc (more stringent) (wodry)
- meowcoin/meowcoin#20592 Update wtxidrelay documentation per BIP339 (jonatack)
- meowcoin/meowcoin#20601 Update for FreeBSD 12.2, add GUI Build Instructions (jarolrod)
- meowcoin/meowcoin#20635 fix misleading comment about call to non-existing function (pox)
- meowcoin/meowcoin#20646 Refer to BIPs 339/155 in feature negotiation (jonatack)
- meowcoin/meowcoin#20653 Move addr relay comment in net to correct place (MarcoFalke)
- meowcoin/meowcoin#20677 Remove shouty enums in `net_processing` comments (sdaftuar)
- meowcoin/meowcoin#20741 Update 'Secure string handling' (prayank23)
- meowcoin/meowcoin#20757 tor.md and -onlynet help updates (jonatack)
- meowcoin/meowcoin#20829 Add -netinfo help (jonatack)
- meowcoin/meowcoin#20830 Update developer notes with signet (jonatack)
- meowcoin/meowcoin#20890 Add explicit macdeployqtplus dependencies install step (hebasto)
- meowcoin/meowcoin#20913 Add manual page generation for meowcoin-util (laanwj)
- meowcoin/meowcoin#20985 Add xorriso to macOS depends packages (fanquake)
- meowcoin/meowcoin#20986 Update developer notes to discourage very long lines (jnewbery)
- meowcoin/meowcoin#20987 Add instructions for generating RPC docs (ben-kaufman)
- meowcoin/meowcoin#21026 Document use of make-tag script to make tags (laanwj)
- meowcoin/meowcoin#21028 doc/bips: Add BIPs 43, 44, 49, and 84 (luke-jr)
- meowcoin/meowcoin#21049 Add release notes for listdescriptors RPC (S3RK)
- meowcoin/meowcoin#21060 More precise -debug and -debugexclude doc (wodry)
- meowcoin/meowcoin#21077 Clarify -timeout and -peertimeout config options (glozow)
- meowcoin/meowcoin#21105 Correctly identify script type (niftynei)
- meowcoin/meowcoin#21163 Guix is shipped in Debian and Ubuntu (MarcoFalke)
- meowcoin/meowcoin#21210 Rework internal and external links (MarcoFalke)
- meowcoin/meowcoin#21246 Correction for VerifyTaprootCommitment comments (roconnor-blockstream)
- meowcoin/meowcoin#21263 Clarify that squashing should happen before review (MarcoFalke)
- meowcoin/meowcoin#21323 guix, doc: Update default HOSTS value (hebasto)
- meowcoin/meowcoin#21324 Update build instructions for Fedora (hebasto)
- meowcoin/meowcoin#21343 Revamp macOS build doc (jarolrod)
- meowcoin/meowcoin#21346 install qt5 when building on macOS (fanquake)
- meowcoin/meowcoin#21384 doc: add signet to meowcoin.conf documentation (jonatack)
- meowcoin/meowcoin#21394 Improve comment about protected peers (amitiuttarwar)
- meowcoin/meowcoin#21398 Update fuzzing docs for afl-clang-lto (MarcoFalke)
- meowcoin/meowcoin#21444 net, doc: Doxygen updates and fixes in netbase.{h,cpp} (jonatack)
- meowcoin/meowcoin#21481 Tell howto install clang-format on Debian/Ubuntu (wodry)
- meowcoin/meowcoin#21567 Fix various misleading comments (glozow)
- meowcoin/meowcoin#21661 Fix name of script guix-build (Emzy)
- meowcoin/meowcoin#21672 Remove boostrap info from `GUIX_COMMON_FLAGS` doc (fanquake)
- meowcoin/meowcoin#21688 Note on SDK for macOS depends cross-compile (jarolrod)
- meowcoin/meowcoin#21709 Update reduce-memory.md and meowcoin.conf -maxconnections info (jonatack)
- meowcoin/meowcoin#21710 update helps for addnode rpc and -addnode/-maxconnections config options (jonatack)
- meowcoin/meowcoin#21752 Clarify that feerates are per virtual size (MarcoFalke)
- meowcoin/meowcoin#21811 Remove Visual Studio 2017 reference from readme (sipsorcery)
- meowcoin/meowcoin#21818 Fixup -coinstatsindex help, update meowcoin.conf and files.md (jonatack)
- meowcoin/meowcoin#21856 add OSS-Fuzz section to fuzzing.md doc (adamjonas)
- meowcoin/meowcoin#21912 Remove mention of priority estimation (MarcoFalke)
- meowcoin/meowcoin#21925 Update bips.md for 0.21.1 (MarcoFalke)
- meowcoin/meowcoin#21942 improve make with parallel jobs description (klementtan)
- meowcoin/meowcoin#21947 Fix OSS-Fuzz links (MarcoFalke)
- meowcoin/meowcoin#21988 note that brew installed qt is not supported (jarolrod)
- meowcoin/meowcoin#22056 describe in fuzzing.md how to reproduce a CI crash (jonatack)
- meowcoin/meowcoin#22080 add maxuploadtarget to meowcoin.conf example (jarolrod)
- meowcoin/meowcoin#22088 Improve note on choosing posix mingw32 (jarolrod)
- meowcoin/meowcoin#22109 Fix external links (IRC, …) (MarcoFalke)
- meowcoin/meowcoin#22121 Various validation doc fixups (MarcoFalke)
- meowcoin/meowcoin#22172 Update tor.md, release notes with removal of tor v2 support (jonatack)
- meowcoin/meowcoin#22204 Remove obsolete `okSafeMode` RPC guideline from developer notes (theStack)
- meowcoin/meowcoin#22208 Update `REVIEWERS` (practicalswift)
- meowcoin/meowcoin#22250 add basic I2P documentation (vasild)
- meowcoin/meowcoin#22296 Final merge of release notes snippets, mv to wiki (MarcoFalke)
- meowcoin/meowcoin#22335 recommend `--disable-external-signer` in OpenBSD build guide (theStack)
- meowcoin/meowcoin#22339 Document minimum required libc++ version (hebasto)
- meowcoin/meowcoin#22349 Repository IRC updates (jonatack)
- meowcoin/meowcoin#22360 Remove unused section from release process (MarcoFalke)
- meowcoin/meowcoin#22369 Add steps for Transifex to release process (jonatack)
- meowcoin/meowcoin#22393 Added info to meowcoin.conf doc (bliotti)
- meowcoin/meowcoin#22402 Install Rosetta on M1-macOS for qt in depends (hebasto)
- meowcoin/meowcoin#22432 Fix incorrect `testmempoolaccept` doc (glozow)
- meowcoin/meowcoin#22648 doc, test: improve i2p/tor docs and i2p reachable unit tests (jonatack)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Aaron Clauson
- Adam Jonas
- amadeuszpawlik
- Amiti Uttarwar
- Andrew Chow
- Andrew Poelstra
- Anthony Towns
- Antoine Poinsot
- Antoine Riard
- apawlik
- apitko
- Ben Carman
- Ben Woosley
- benk10
- Bezdrighin
- Block Mechanic
- Brian Liotti
- Bruno Garcia
- Carl Dong
- Christian Decker
- coinforensics
- Cory Fields
- Dan Benjamin
- Daniel Kraft
- Darius Parvin
- Dhruv Mehta
- Dmitry Goncharov
- Dmitry Petukhov
- dplusplus1024
- dscotese
- Duncan Dean
- Elle Mouton
- Elliott Jin
- Emil Engler
- Ethan Heilman
- eugene
- Evan Klitzke
- Fabian Jahr
- Fabrice Fontaine
- fanquake
- fdov
- flack
- Fotis Koutoupas
- Fu Yong Quah
- fyquah
- glozow
- Gregory Sanders
- Guido Vranken
- Gunar C. Gessner
- h
- HAOYUatHZ
- Hennadii Stepanov
- Igor Cota
- Ikko Ashimine
- Ivan Metlushko
- jackielove4u
- James O'Beirne
- Jarol Rodriguez
- Joel Klabo
- John Newbery
- Jon Atack
- Jonas Schnelli
- João Barbosa
- Josiah Baker
- Karl-Johan Alm
- Kiminuo
- Klement Tan
- Kristaps Kaupe
- Larry Ruane
- lisa neigut
- Lucas Ontivero
- Luke Dashjr
- Maayan Keshet
- MarcoFalke
- Martin Ankerl
- Martin Zumsande
- Michael Dietz
- Michael Polzer
- Michael Tidwell
- Niklas Gögge
- nthumann
- Oliver Gugger
- parazyd
- Patrick Strateman
- Pavol Rusnak
- Peter Bushnell
- Pierre K
- Pieter Wuille
- PiRK
- pox
- practicalswift
- Prayank
- R E Broadley
- Rafael Sadowski
- randymcmillan
- Raul Siles
- Riccardo Spagni
- Russell O'Connor
- Russell Yanofsky
- S3RK
- saibato
- Samuel Dobson
- sanket1729
- Sawyer Billings
- Sebastian Falbesoner
- setpill
- sgulls
- sinetek
- Sjors Provoost
- Sriram
- Stephan Oeste
- Suhas Daftuar
- Sylvain Goumy
- t-bast
- Troy Giorshev
- Tushar Singla
- Tyler Chambers
- Uplab
- Vasil Dimov
- W. J. van der Laan
- willcl-ark
- William Bright
- William Casarin
- windsok
- wodry
- Yerzhan Mazhkenov
- Yuval Kogman
- Zero

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/meowcoin/meowcoin/).
