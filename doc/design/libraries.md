# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libmeowcoin_cli*         | RPC client functionality used by *meowcoin-cli* executable |
| *libmeowcoin_common*      | Home for common functionality shared by different executables and libraries. Similar to *libmeowcoin_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libmeowcoin_consensus*   | Consensus functionality used by *libmeowcoin_node* and *libmeowcoin_wallet*. |
| *libmeowcoin_crypto*      | Hardware-optimized functions for data encryption, hashing, message authentication, and key derivation. |
| *libmeowcoin_kernel*      | Consensus engine and support library used for validation by *libmeowcoin_node*. |
| *libmeowcoinqt*           | GUI functionality used by *meowcoin-qt* and *meowcoin-gui* executables. |
| *libmeowcoin_ipc*         | IPC functionality used by *meowcoin-node* and *meowcoin-gui* executables to communicate when [`-DENABLE_IPC=ON`](multiprocess.md) is used. |
| *libmeowcoin_node*        | P2P and RPC server functionality used by *meowcoind* and *meowcoin-qt* executables. |
| *libmeowcoin_util*        | Home for common functionality shared by different executables and libraries. Similar to *libmeowcoin_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libmeowcoin_wallet*      | Wallet functionality used by *meowcoind* and *meowcoin-wallet* executables. |
| *libmeowcoin_wallet_tool* | Lower-level wallet functionality used by *meowcoin-wallet* executable. |
| *libmeowcoin_zmq*         | [ZeroMQ](../zmq.md) functionality used by *meowcoind* and *meowcoin-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. An exception is *libmeowcoin_kernel*, which, at some future point, will have a documented external interface.

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`add_library(meowcoin_* ...)`](../../src/CMakeLists.txt) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libmeowcoin_node* code lives in `src/node/` in the `node::` namespace
  - *libmeowcoin_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libmeowcoin_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libmeowcoin_util* code lives in `src/util/` in the `util::` namespace
  - *libmeowcoin_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

meowcoin-cli[meowcoin-cli]-->libmeowcoin_cli;

meowcoind[meowcoind]-->libmeowcoin_node;
meowcoind[meowcoind]-->libmeowcoin_wallet;

meowcoin-qt[meowcoin-qt]-->libmeowcoin_node;
meowcoin-qt[meowcoin-qt]-->libmeowcoinqt;
meowcoin-qt[meowcoin-qt]-->libmeowcoin_wallet;

meowcoin-wallet[meowcoin-wallet]-->libmeowcoin_wallet;
meowcoin-wallet[meowcoin-wallet]-->libmeowcoin_wallet_tool;

libmeowcoin_cli-->libmeowcoin_util;
libmeowcoin_cli-->libmeowcoin_common;

libmeowcoin_consensus-->libmeowcoin_crypto;

libmeowcoin_common-->libmeowcoin_consensus;
libmeowcoin_common-->libmeowcoin_crypto;
libmeowcoin_common-->libmeowcoin_util;

libmeowcoin_kernel-->libmeowcoin_consensus;
libmeowcoin_kernel-->libmeowcoin_crypto;
libmeowcoin_kernel-->libmeowcoin_util;

libmeowcoin_node-->libmeowcoin_consensus;
libmeowcoin_node-->libmeowcoin_crypto;
libmeowcoin_node-->libmeowcoin_kernel;
libmeowcoin_node-->libmeowcoin_common;
libmeowcoin_node-->libmeowcoin_util;

libmeowcoinqt-->libmeowcoin_common;
libmeowcoinqt-->libmeowcoin_util;

libmeowcoin_util-->libmeowcoin_crypto;

libmeowcoin_wallet-->libmeowcoin_common;
libmeowcoin_wallet-->libmeowcoin_crypto;
libmeowcoin_wallet-->libmeowcoin_util;

libmeowcoin_wallet_tool-->libmeowcoin_wallet;
libmeowcoin_wallet_tool-->libmeowcoin_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class meowcoin-qt,meowcoind,meowcoin-cli,meowcoin-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Crypto* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus, crypto, and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libmeowcoin_wallet* and *libmeowcoin_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libmeowcoin_crypto* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libmeowcoin_consensus* should only depend on *libmeowcoin_crypto*, and all other libraries besides *libmeowcoin_crypto* should be allowed to depend on it.

- *libmeowcoin_util* should be a standalone dependency that any library can depend on, and it should not depend on other libraries except *libmeowcoin_crypto*. It provides basic utilities that fill in gaps in the C++ standard library and provide lightweight abstractions over platform-specific features. Since the util library is distributed with the kernel and is usable by kernel applications, it shouldn't contain functions that external code shouldn't call, like higher level code targeted at the node or wallet. (*libmeowcoin_common* is a better place for higher level code, or code that is meant to be used by internal applications only.)

- *libmeowcoin_common* is a home for miscellaneous shared code used by different Meowcoin Core applications. It should not depend on anything other than *libmeowcoin_util*, *libmeowcoin_consensus*, and *libmeowcoin_crypto*.

- *libmeowcoin_kernel* should only depend on *libmeowcoin_util*, *libmeowcoin_consensus*, and *libmeowcoin_crypto*.

- The only thing that should depend on *libmeowcoin_kernel* internally should be *libmeowcoin_node*. GUI and wallet libraries *libmeowcoinqt* and *libmeowcoin_wallet* in particular should not depend on *libmeowcoin_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be able to get it from *libmeowcoin_consensus*, *libmeowcoin_common*, *libmeowcoin_crypto*, and *libmeowcoin_util*, instead of *libmeowcoin_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libmeowcoinqt*, *libmeowcoin_node*, *libmeowcoin_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](../../src/interfaces/) abstract interfaces.

## Work in progress

- Validation code is moving from *libmeowcoin_node* to *libmeowcoin_kernel* as part of [The libmeowcoinkernel Project #27587](https://github.com/meowcoin/meowcoin/issues/27587)
