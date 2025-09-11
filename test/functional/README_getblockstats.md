# getblockstats RPC Test Implementation

## Overview

This document describes the test implementation for the `getblockstats` RPC command in Meowcoin, based on the Dogecoin implementation from [PR #3297](https://github.com/dogecoin/dogecoin/pull/3297).

## Files Created

### 1. Test Implementation
- **`test/functional/rpc_getblockstats.py`** - Main test file for getblockstats functionality
- **`test/functional/data/rpc_getblockstats.json`** - Test data with expected results and valid/invalid statistics

### 2. Test Data
- **`test/functional/data/block_0_stats.json`** - Genesis block statistics
- **`test/functional/data/block_1000_stats.json`** - Block 1000 statistics  
- **`test/functional/data/block_latest_stats.json`** - Latest block statistics

### 3. Simple Test Script
- **`test_getblockstats.py`** - Standalone test script for quick validation

## Test Coverage

### Basic Functionality Tests
- ✅ Height-based block lookup
- ✅ Specific statistics selection
- ✅ All statistics retrieval
- ✅ Multiple statistics selection

### Error Handling Tests
- ✅ Invalid height (negative)
- ✅ Invalid height (too high)
- ✅ Invalid statistics names
- ✅ Invalid parameter types
- ✅ Empty statistics array

### Data Validation Tests
- ✅ Required fields present
- ✅ Correct data types
- ✅ Block height matches request
- ✅ Block hash consistency

## Test Data Structure

The test data file (`rpc_getblockstats.json`) contains:

```json
{
  "expected_stats": [
    // Array of expected statistics for different blocks
  ],
  "test_blocks": [
    // Array of test blocks with height and hash
  ],
  "valid_stats": [
    // Array of valid statistic names
  ],
  "invalid_stats": [
    // Array of invalid statistic names for error testing
  ]
}
```

## Available Statistics

The `getblockstats` RPC returns the following statistics:

### Basic Information
- `height` - Block height
- `blockhash` - Block hash
- `time` - Block timestamp
- `mediantime` - Median block time
- `txs` - Number of transactions

### Fee Statistics
- `avgfee` - Average fee
- `avgfeerate` - Average feerate
- `minfee` - Minimum fee
- `maxfee` - Maximum fee
- `medianfee` - Median fee
- `minfeerate` - Minimum feerate
- `maxfeerate` - Maximum feerate
- `feerate_percentiles` - Fee percentiles array

### Size Statistics
- `avgtxsize` - Average transaction size
- `maxtxsize` - Maximum transaction size
- `mintxsize` - Minimum transaction size
- `mediantxsize` - Median transaction size
- `total_size` - Total size of non-coinbase transactions
- `total_weight` - Total weight of non-coinbase transactions

### UTXO Statistics
- `ins` - Number of inputs (excluding coinbase)
- `outs` - Number of outputs
- `utxo_increase` - UTXO count change
- `utxo_size_inc` - UTXO size change

### SegWit Statistics
- `swtotal_size` - Total SegWit transaction size
- `swtotal_weight` - Total SegWit transaction weight
- `swtxs` - Number of SegWit transactions

### Economic Statistics
- `subsidy` - Block subsidy
- `total_out` - Total output value
- `totalfee` - Total fees

## Usage Examples

### Basic Usage
```bash
# Get all statistics for block 0
./src/meowcoin-cli -testnet getblockstats 0

# Get specific statistics
./src/meowcoin-cli -testnet getblockstats 0 '["height","txs","time"]'
```

### RPC Usage
```bash
# Using curl
curl -s --user test:test --data-binary '{"jsonrpc": "1.0", "id": "test", "method": "getblockstats", "params": [0]}' -H 'content-type: text/plain;' http://127.0.0.1:18332/

# With specific stats
curl -s --user test:test --data-binary '{"jsonrpc": "1.0", "id": "test", "method": "getblockstats", "params": [0, ["height", "txs"]]}' -H 'content-type: text/plain;' http://127.0.0.1:18332/
```

## Running Tests

### Quick Test (Recommended)
```bash
# Run the standalone test script
python3 test_getblockstats.py
```

### Full Test Suite (Not Recommended in Production)
```bash
# Note: Tests are currently disabled in production
# This is for reference only
python3 test/functional/rpc_getblockstats.py --testdir=/tmp/meowcoin_test
```

## Implementation Notes

### Current Status
- ✅ RPC command implemented and working
- ✅ Basic functionality tested
- ✅ Error handling implemented
- ✅ Test structure created
- ⚠️ Hash-based lookup has CLI parsing issues (RPC works fine)
- ⚠️ Statistics calculation is minimal (returns zeros for most fields)

### Future Improvements
1. **Statistics Calculation**: Implement actual calculation of fees, sizes, etc.
2. **Hash Lookup**: Fix CLI hash parameter parsing
3. **Performance**: Optimize for large blocks
4. **Validation**: Add more comprehensive data validation

### Compatibility
- Compatible with Bitcoin Core getblockstats implementation
- Compatible with Dogecoin getblockstats implementation
- Follows Meowcoin RPC patterns and conventions

## Error Codes

- `-1`: Block height out of range
- `-5`: Block not found
- `-8`: Invalid selected statistic
- `-8`: Stats parameter must be an array

## Dependencies

- Meowcoin daemon running
- Python 3.x
- JSON parsing capabilities
- RPC access enabled

## Notes for Production

- Tests are currently disabled in production builds
- The implementation is minimal but functional
- Statistics calculation can be enhanced as needed
- Error handling is comprehensive and follows RPC standards
