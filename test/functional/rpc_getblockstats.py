#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Meowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test getblockstats rpc call
#

from test_framework.test_framework import MeowcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)
import json
import os
import logging

TESTSDIR = os.path.dirname(os.path.realpath(__file__))

class GetblockstatsTest(MeowcoinTestFramework):

    start_height = 0
    max_stat_pos = 2

    def add_options(self, parser):
        pass

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
        test_data = self.load_test_data()
        
        # Test basic functionality
        self.log.info("Testing basic getblockstats functionality")
        
        # Test with height parameter
        stats = self.get_stats_for_height(0)
        self.log.info("Block 0 stats: %s", stats)
        
        # Verify basic required fields
        required_fields = ['height', 'blockhash', 'time', 'txs', 'mediantime']
        for field in required_fields:
            assert field in stats, f"Missing required field: {field}"
        
        # Test with hash parameter
        block_hash = self.nodes[0].getblockhash(0)
        stats_by_hash = self.nodes[0].getblockstats(hash=block_hash)
        assert_equal(stats, stats_by_hash)
        
        # Test specific stats selection
        height_stats = self.get_stats_for_height(0, ['height', 'txs', 'time'])
        assert_equal(set(height_stats.keys()), {'height', 'txs', 'time'})
        assert_equal(height_stats['height'], 0)
        
        # Test all valid statistics can be queried individually
        valid_stats = test_data.get('valid_stats', [])
        for stat in valid_stats:
            result = self.get_stats_for_height(0, [stat])
            assert_equal(list(result.keys()), [stat])
            self.log.info("Stat %s: %s", stat, result[stat])
        
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
        current_height = self.nodes[0].getblockcount()
        assert_raises_rpc_error(-1, "Block height out of range", 
                               self.get_stats_for_height, current_height + 1)
        
        # Test invalid hash
        assert_raises_rpc_error(-5, "Block not found", 
                               self.nodes[0].getblockstats, 
                               hash="0000000000000000000000000000000000000000000000000000000000000000")
        
        # Test invalid stats
        invalid_stats = test_data.get('invalid_stats', ['invalid_stat'])
        for invalid_stat in invalid_stats:
            assert_raises_rpc_error(-8, f"Invalid selected statistic {invalid_stat}", 
                                   self.get_stats_for_height, 0, [invalid_stat])
        
        # Test stats parameter must be array
        assert_raises_rpc_error(-8, "Stats parameter must be an array", 
                               self.get_stats_for_height, 0, "not_an_array")
        
        # Test with different blocks
        self.log.info("Testing with different blocks")
        test_blocks = test_data.get('test_blocks', [])
        for block_info in test_blocks:
            height = block_info['height']
            expected_hash = block_info['hash']
            
            stats = self.get_stats_for_height(height)
            assert_equal(stats['height'], height)
            assert_equal(stats['blockhash'], expected_hash)
            
            # Test by hash
            stats_by_hash = self.nodes[0].getblockstats(hash=expected_hash)
            assert_equal(stats, stats_by_hash)
        
        # Test edge cases
        self.log.info("Testing edge cases")
        
        # Test empty stats array
        empty_stats = self.get_stats_for_height(0, [])
        assert_equal(empty_stats, {})
        
        # Test with non-existent block hash
        fake_hash = "0000000000000000000000000000000000000000000000000000000000000001"
        assert_raises_rpc_error(-5, "Block not found", 
                               self.nodes[0].getblockstats, hash=fake_hash)
        
        self.log.info("All getblockstats tests passed!")

    def load_test_data(self):
        """Load test data from JSON file"""
        test_data_file = os.path.join(TESTSDIR, 'data', 'rpc_getblockstats.json')
        if os.path.exists(test_data_file):
            with open(test_data_file, 'r') as f:
                return json.load(f)
        return {}

if __name__ == '__main__':
    GetblockstatsTest().main()
