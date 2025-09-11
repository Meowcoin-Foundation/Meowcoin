#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test getblockstats RPC command
"""

from test_framework.test_framework import MeowcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

class GetblockstatsTest(MeowcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = False  # Use existing chain for real data
        self.supports_cli = False

    def get_stats_for_height(self, height, stats=None):
        """Get stats for a specific height"""
        if stats is None:
            return self.nodes[0].getblockstats(height=height)
        else:
            return self.nodes[0].getblockstats(height=height, stats=stats)

    def run_test(self):
        # Test basic functionality
        self.log.info("Testing basic getblockstats functionality")
        
        # Test with height parameter
        stats = self.get_stats_for_height(0)
        self.log.info("Block 0 stats: %s", stats)
        
        # Verify basic required fields
        required_fields = ['height', 'blockhash', 'time', 'txs', 'mediantime']
        for field in required_fields:
            assert field in stats, f"Missing required field: {field}"
        
        # Test specific stats selection
        height_stats = self.get_stats_for_height(0, ['height', 'txs', 'time'])
        assert_equal(set(height_stats.keys()), {'height', 'txs', 'time'})
        assert_equal(height_stats['height'], 0)
        
        # Test multiple stats selection
        some_stats = ['height', 'txs', 'time', 'blockhash']
        multi_stats = self.get_stats_for_height(0, some_stats)
        assert_equal(set(multi_stats.keys()), set(some_stats))
        
        # Test error cases
        self.log.info("Testing error cases")
        
        # Test invalid height (negative)
        assert_raises_rpc_error(-1, "Block height out of range", 
                               self.get_stats_for_height, -1)
        
        # Test invalid height (too high)
        max_height = self.nodes[0].getblockcount()
        assert_raises_rpc_error(-1, "Block height out of range", 
                               self.get_stats_for_height, max_height + 1)
        
        # Test invalid stat name
        assert_raises_rpc_error(-1, "Invalid selected statistic", 
                               self.get_stats_for_height, 0, ['invalid_stat'])
        
        # Test invalid stat type (not array)
        assert_raises_rpc_error(-1, "Stats parameter must be an array", 
                               self.get_stats_for_height, 0, "not_an_array")
        
        # Test invalid stat array element (not string)
        assert_raises_rpc_error(-1, "Stat name must be a string", 
                               self.get_stats_for_height, 0, [123])
        
        self.log.info("All getblockstats tests passed!")

if __name__ == '__main__':
    GetblockstatsTest().main()
