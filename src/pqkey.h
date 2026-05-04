// Copyright (c) 2026 ALENOC <https://github.com/ALENOC>
// Copyright (c) 2024-present The Avian Core developers
// Portions Copyright (c) 2026 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

// Post-quantum key types for ML-DSA-44 (FIPS 204).
// CPQPubKey wraps the 1312-byte ML-DSA-44 public key.
// CPQKey wraps the 2560-byte ML-DSA-44 secret key with secure storage.

#ifndef BITCOIN_PQKEY_H
#define BITCOIN_PQKEY_H

#include <crypto/mldsa.h>
#include <support/allocators/secure.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

/** An ML-DSA-44 public key (1312 bytes). */
class CPQPubKey
{
public:
    static constexpr size_t SIZE = mldsa::PUBKEY_SIZE;

private:
    std::array<uint8_t, SIZE> m_data{};
    bool m_valid{false};

public:
    CPQPubKey() = default;

    explicit CPQPubKey(std::span<const uint8_t, SIZE> data)
        : m_valid{true}
    {
        std::copy(data.begin(), data.end(), m_data.begin());
    }

    bool IsValid() const { return m_valid; }

    std::span<const uint8_t, SIZE> GetData() const
    {
        return std::span<const uint8_t, SIZE>{m_data};
    }

    /** Returns SHA256(pubkey) — the 32-byte witness program for OP_2 <wp> outputs. */
    uint256 GetWitnessProgram() const;

    bool Verify(std::span<const uint8_t> sig,
                std::span<const uint8_t> msg) const;

    bool operator==(const CPQPubKey& other) const
    {
        return m_valid == other.m_valid && m_data == other.m_data;
    }
    bool operator!=(const CPQPubKey& other) const { return !(*this == other); }

    template <typename Stream>
    void Serialize(Stream& s) const { s.write(MakeByteSpan(m_data)); }
    template <typename Stream>
    void Unserialize(Stream& s) { s.read(MakeWritableByteSpan(m_data)); m_valid = true; }
};

/** An ML-DSA-44 secret key (2560 bytes, secured memory). */
class CPQKey
{
public:
    static constexpr size_t SIZE = mldsa::SECRETKEY_SIZE;
    using KeyData = std::vector<uint8_t, secure_allocator<uint8_t>>;

private:
    KeyData m_keydata;
    CPQPubKey m_pubkey;

public:
    CPQKey() = default;

    bool IsValid() const { return m_keydata.size() == SIZE && m_pubkey.IsValid(); }

    bool MakeNewKey();
    bool SetSeed(std::span<const uint8_t, mldsa::SEED_SIZE> seed);
    bool SetKeyData(std::span<const uint8_t, SIZE> data, const CPQPubKey& pubkey);

    CPQPubKey GetPubKey() const { return m_pubkey; }

    std::span<const uint8_t, SIZE> GetData() const
    {
        return std::span<const uint8_t, SIZE>{m_keydata.data(), SIZE};
    }

    bool Sign(std::vector<uint8_t>& sig_out, std::span<const uint8_t> msg) const;

    void Clear()
    {
        memory_cleanse(m_keydata.data(), m_keydata.size());
        m_keydata.clear();
        m_pubkey = CPQPubKey{};
    }

    ~CPQKey() { Clear(); }
};

#endif // BITCOIN_PQKEY_H
