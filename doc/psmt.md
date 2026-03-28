# PSMT Howto for Meowcoin Core

Since Meowcoin Core 0.17, an RPC interface exists for Partially Signed Meowcoin
Transactions (PSMTs, as specified in
[BIP 174](https://github.com/meowcoin/bips/blob/master/bip-0174.mediawiki)).

This document describes the overall workflow for producing signed transactions
through the use of PSMT, and the specific RPC commands used in typical
scenarios.

## PSMT in general

PSMT is an interchange format for Meowcoin transactions that are not fully signed
yet, together with relevant metadata to help entities work towards signing it.
It is intended to simplify workflows where multiple parties need to cooperate to
produce a transaction. Examples include hardware wallets, multisig setups, and
[CoinJoin](https://meowcointalk.org/?topic=279249) transactions.

### Overall workflow

Overall, the construction of a fully signed Meowcoin transaction goes through the
following steps:

- A **Creator** proposes a particular transaction to be created. They construct
  a PSMT that contains certain inputs and outputs, but no additional metadata.
- For each input, an **Updater** adds information about the UTXOs being spent by
  the transaction to the PSMT. They also add information about the scripts and
  public keys involved in each of the inputs (and possibly outputs) of the PSMT.
- **Signers** inspect the transaction and its metadata to decide whether they
  agree with the transaction. They can use amount information from the UTXOs
  to assess the values and fees involved. If they agree, they produce a
  partial signature for the inputs for which they have relevant key(s).
- A **Finalizer** is run for each input to convert the partial signatures and
  possibly script information into a final `scriptSig` and/or `scriptWitness`.
- An **Extractor** produces a valid Meowcoin transaction (in network format)
  from a PSMT for which all inputs are finalized.

Generally, each of the above (excluding Creator and Extractor) will simply
add more and more data to a particular PSMT, until all inputs are fully signed.
In a naive workflow, they all have to operate sequentially, passing the PSMT
from one to the next, until the Extractor can convert it to a real transaction.
In order to permit parallel operation, **Combiners** can be employed which merge
metadata from different PSMTs for the same unsigned transaction.

The names above in bold are the names of the roles defined in BIP174. They're
useful in understanding the underlying steps, but in practice, software and
hardware implementations will typically implement multiple roles simultaneously.

## PSMT in Meowcoin Core

### RPCs

- **`converttopsmt` (Creator)** is a utility RPC that converts an
  unsigned raw transaction to PSMT format. It ignores existing signatures.
- **`createpsmt` (Creator)** is a utility RPC that takes a list of inputs and
  outputs and converts them to a PSMT with no additional information. It is
  equivalent to calling `createrawtransaction` followed by `converttopsmt`.
- **`walletcreatefundedpsmt` (Creator, Updater)** is a wallet RPC that creates a
  PSMT with the specified inputs and outputs, adds additional inputs and change
  to it to balance it out, and adds relevant metadata. In particular, for inputs
  that the wallet knows about (counting towards its normal or watch-only
  balance), UTXO information will be added. For outputs and inputs with UTXO
  information present, key and script information will be added which the wallet
  knows about. It is equivalent to running `createrawtransaction`, followed by
  `fundrawtransaction`, and `converttopsmt`.
- **`walletprocesspsmt` (Updater, Signer, Finalizer)** is a wallet RPC that takes as
  input a PSMT, adds UTXO, key, and script data to inputs and outputs that miss
  it, and optionally signs inputs. Where possible it also finalizes the partial
  signatures.
- **`descriptorprocesspsmt` (Updater, Signer, Finalizer)** is a node RPC that takes
  as input a PSMT and a list of descriptors. It updates SegWit inputs with
  information available from the UTXO set and the mempool and signs the inputs using
  the provided descriptors. Where possible it also finalizes the partial signatures.
- **`utxoupdatepsmt` (Updater)** is a node RPC that takes a PSMT and updates it
  to include information available from the UTXO set (works only for SegWit
  inputs).
- **`finalizepsmt` (Finalizer, Extractor)** is a utility RPC that finalizes any
  partial signatures, and if all inputs are finalized, converts the result to a
  fully signed transaction which can be broadcast with `sendrawtransaction`.
- **`combinepsmt` (Combiner)** is a utility RPC that implements a Combiner. It
  can be used at any point in the workflow to merge information added to
  different versions of the same PSMT. In particular it is useful to combine the
  output of multiple Updaters or Signers.
- **`joinpsmts`** (Creator) is a utility RPC that joins multiple PSMTs together,
  concatenating the inputs and outputs. This can be used to construct CoinJoin
  transactions.
- **`decodepsmt`** is a diagnostic utility RPC which will show all information in
  a PSMT in human-readable form, as well as compute its eventual fee if known.
- **`analyzepsmt`** is a utility RPC that examines a PSMT and reports the
  current status of its inputs, the next step in the workflow if known, and if
  possible, computes the fee of the resulting transaction and estimates the
  final weight and feerate.


### Workflows

#### Multisig with multiple Meowcoin Core instances

For a quick start see [Basic M-of-N multisig example using descriptor wallets and PSMTs](./descriptors.md#basic-multisig-example).
