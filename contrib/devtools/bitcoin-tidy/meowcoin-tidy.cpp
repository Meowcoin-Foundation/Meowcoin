// Copyright (c) 2023 Meowcoin Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nontrivial-threadlocal.h"

#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>

class BitcoinModule final : public clang::tidy::ClangTidyModule
{
public:
    void addCheckFactories(clang::tidy::ClangTidyCheckFactories& CheckFactories) override
    {
        CheckFactories.registerCheck<meowcoin::NonTrivialThreadLocal>("meowcoin-nontrivial-threadlocal");
    }
};

static clang::tidy::ClangTidyModuleRegistry::Add<BitcoinModule>
    X("meowcoin-module", "Adds meowcoin checks.");

volatile int BitcoinModuleAnchorSource = 0;
