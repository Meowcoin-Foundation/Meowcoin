#!/usr/bin/env python3
"""
Simple test script for getblockstats RPC command
This tests the basic functionality without running the full test suite
"""

import subprocess
import json
import sys

def run_rpc_command(method, params=None):
    """Run an RPC command and return the result"""
    cmd = ['./src/meowcoin-cli', '-testnet', method]
    if params:
        if isinstance(params, list):
            # For arrays, we need to pass them as JSON strings
            if len(params) > 1 and isinstance(params[1], list):
                # This is a stats array, format it properly
                stats_str = json.dumps(params[1])
                cmd.extend([str(params[0]), stats_str])
            else:
                cmd.extend([str(p) for p in params])
        else:
            cmd.append(str(params))
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return json.loads(result.stdout)
    except subprocess.CalledProcessError as e:
        # Check if this is an expected RPC error
        if "error code:" in e.stderr and "error message:" in e.stderr:
            return None  # Expected error
        print(f"Error running command: {e}")
        print(f"Stderr: {e.stderr}")
        return None
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}")
        print(f"Output: {result.stdout}")
        return None

def test_basic_functionality():
    """Test basic getblockstats functionality"""
    print("Testing basic getblockstats functionality...")
    
    # Test with height
    stats = run_rpc_command('getblockstats', [0])
    if stats is None:
        print("❌ Failed to get stats for block 0")
        return False
    
    print(f"✅ Block 0 stats: height={stats.get('height')}, txs={stats.get('txs')}")
    
    # Test specific stats
    specific_stats = run_rpc_command('getblockstats', [0, ['height', 'txs', 'time']])
    if specific_stats and len(specific_stats) == 3:
        print("✅ Specific stats selection works")
    else:
        print("❌ Specific stats selection failed")
        return False
    
    # Test all available stats
    all_stats = run_rpc_command('getblockstats', [0])
    if all_stats and len(all_stats) > 10:
        print(f"✅ All stats returned: {len(all_stats)} fields")
    else:
        print("❌ All stats selection failed")
        return False
    
    return True

def test_error_cases():
    """Test error cases"""
    print("\nTesting error cases...")
    
    # Test invalid height
    result = run_rpc_command('getblockstats', [-1])
    if result is None:
        print("✅ Invalid height properly rejected")
    else:
        print("❌ Invalid height should have been rejected")
        return False
    
    # Test invalid hash
    result = run_rpc_command('getblockstats', ['0000000000000000000000000000000000000000000000000000000000000000'])
    if result is None:
        print("✅ Invalid hash properly rejected")
    else:
        print("❌ Invalid hash should have been rejected")
        return False
    
    # Test invalid stats
    result = run_rpc_command('getblockstats', [0, ['invalid_stat']])
    if result is None:
        print("✅ Invalid stats properly rejected")
    else:
        print("❌ Invalid stats should have been rejected")
        return False
    
    return True

def test_different_blocks():
    """Test with different blocks"""
    print("\nTesting with different blocks...")
    
    # Test genesis block
    stats = run_rpc_command('getblockstats', [0])
    if stats and stats.get('height') == 0:
        print("✅ Genesis block stats work")
    else:
        print("❌ Genesis block stats failed")
        return False
    
    # Test a recent block
    stats = run_rpc_command('getblockstats', [1000])
    if stats and stats.get('height') == 1000:
        print("✅ Block 1000 stats work")
    else:
        print("❌ Block 1000 stats failed")
        return False
    
    return True

def main():
    """Run all tests"""
    print("🧪 Testing getblockstats RPC implementation")
    print("=" * 50)
    
    tests = [
        test_basic_functionality,
        test_error_cases,
        test_different_blocks
    ]
    
    passed = 0
    total = len(tests)
    
    for test in tests:
        if test():
            passed += 1
        else:
            print(f"❌ Test {test.__name__} failed")
    
    print("\n" + "=" * 50)
    print(f"📊 Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! getblockstats implementation is working correctly.")
        return 0
    else:
        print("💥 Some tests failed. Check the implementation.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
