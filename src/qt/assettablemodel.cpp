// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Meowcoin Core developers
// Copyright (c) 2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/assettablemodel.h>
#include <qt/assetrecord.h>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>
#include <wallet/wallet.h>

#include <consensus/amount.h>
#include <assets/assets.h>
#include <validation.h>
#include <qt/platformstyle.h>

#include <QDebug>
#include <QImage>
#include <QStringList>
#include <QPixmap>

#include <optional>

namespace {

/**
 * Decoration pixmaps are requested often while painting; load once after QGuiApplication exists.
 * Do not use plain QPixmap members here: static storage would construct them before main().
 */
struct AssetTableDecorationCache {
    std::optional<QPixmap> extLinkLight;
    std::optional<QPixmap> extLinkDark;
    std::optional<QPixmap> exportIcon;
    std::optional<QPixmap> adminLight;
    std::optional<QPixmap> adminDark;

    void ensure()
    {
        if (extLinkLight) return;
        extLinkLight.emplace(QPixmap::fromImage(QImage(QStringLiteral(":/icons/external_link"))));
        extLinkDark.emplace(QPixmap::fromImage(QImage(QStringLiteral(":/icons/external_link_dark"))));
        exportIcon.emplace(QPixmap::fromImage(QImage(QStringLiteral(":/icons/export"))));
        adminLight.emplace(QPixmap::fromImage(QImage(QStringLiteral(":/icons/asset_administrator"))));
        adminDark.emplace(QPixmap::fromImage(QImage(QStringLiteral(":/icons/asset_administrator_dark"))));
    }
};

AssetTableDecorationCache g_assetTableDecos;

} // namespace

/** Get all asset balances for a wallet (native MEWC + issued assets).
 *
 * Light-weight scan that only iterates asset UTXOs (via GetTXOs()),
 * skipping the expensive AvailableCoins() regular-coin scan and the
 * per-output signing-provider / input-size calculations that are only
 * needed for coin selection. Uses TRY_LOCK so the GUI thread is never
 * blocked waiting for cs_wallet.
 */
static bool GetAllMyAssetBalances(wallet::CWallet* pwallet,
    std::map<std::string, CAmount>& amounts)
{
    if (!pwallet) return false;

    TRY_LOCK(pwallet->cs_wallet, locked_wallet);
    if (!locked_wallet) return false;

    for (const auto& [outpoint, txo] : pwallet->GetTXOs()) {
        const CTxOut& output = txo.GetTxOut();

        if (!output.scriptPubKey.IsAssetScript())
            continue;
        if (pwallet->IsSpent(outpoint))
            continue;

        const wallet::CWalletTx& wtx = txo.GetWalletTx();
        int nDepth = pwallet->GetTxDepthInMainChain(wtx);
        if (nDepth < 0)
            continue;
        if (nDepth == 0 && !wtx.InMempool())
            continue;

        CAssetOutputEntry data;
        if (GetAssetData(output.scriptPubKey, data))
            amounts[data.assetName] += data.nAmount;
    }

    return true;
}


class AssetTablePriv {
public:
    AssetTablePriv(AssetTableModel *_parent) :
            parent(_parent)
    {
    }

    AssetTableModel *parent;

    QList<AssetRecord> cachedBalances;
    // Cache of last-seen balances for change detection
    std::map<std::string, CAmount> cachedAmounts;

    // loads all current balances into cache
#ifdef ENABLE_WALLET
    /** Rebuild the display cache from pre-computed balances.
     *  If no balances are provided, fetch them now. */
    void refreshWallet(const std::map<std::string, CAmount>* precomputedBalances = nullptr) {
        cachedBalances.clear();
        auto currentActiveAssetCache = GetCurrentAssetCache();
        if (currentActiveAssetCache) {
            // Phase 1: Get asset balances under cs_wallet only (no cs_main).
            // These two locks must never be held simultaneously to avoid
            // deadlocking with the block-processing / mempool-submission
            // threads which acquire cs_main then cs_wallet.
            std::map<std::string, CAmount> fetchedBalances;
            const std::map<std::string, CAmount>& balances =
                precomputedBalances ? *precomputedBalances : fetchedBalances;

            if (!precomputedBalances) {
                wallet::CWallet* pwallet = parent->walletModel ? parent->walletModel->wallet().wallet() : nullptr;
                if (!GetAllMyAssetBalances(pwallet, fetchedBalances)) {
                    return;
                }
            }

            // Phase 2: Look up asset metadata under cs_main only (no cs_wallet).
            {
                LOCK(cs_main);
                std::set<std::string> setAssetsToSkip;
                auto bal = balances.begin();
                for (; bal != balances.end(); bal++) {
                    // retrieve units for asset
                    uint8_t units = OWNER_UNITS;
                    bool fIsAdministrator = true;
                    std::string ipfsHash = "";
                    std::string ansID = "";

                    if (setAssetsToSkip.count(bal->first))
                        continue;

                    if (!IsAssetNameAnOwner(bal->first)) {
                        // Asset is not an administrator asset
                        CNewAsset assetData;
                        if (!currentActiveAssetCache->GetAssetMetaDataIfExists(bal->first, assetData)) {
                            // Do not abort the whole table: e.g. brand-new issuance still in mempool, or
                            // asset DB not updated yet — other balances would all show "(unknown)" in Send.
                            qWarning() << "AssetTableModel: missing metadata for"
                                       << QString::fromStdString(bal->first)
                                       << "- showing quantity with default units until cache/index catches up";
                            units = DEFAULT_UNITS;
                            ipfsHash = "";
                            ansID = "";
                        } else {
                            units = assetData.units;
                            ipfsHash = assetData.strIPFSHash;
                            ansID = assetData.strANSID;
                        }
                        // If we have the administrator asset, add it to the skip list
                        if (balances.count(bal->first + OWNER_TAG)) {
                            setAssetsToSkip.insert(bal->first + OWNER_TAG);
                        } else {
                            fIsAdministrator = false;
                        }
                    } else {
                        // Asset is an administrator asset, if we own assets that is administrators, skip this balance
                        std::string name = bal->first;
                        name.pop_back();
                        if (balances.count(name)) {
                            setAssetsToSkip.insert(bal->first);
                            continue;
                        }
                        // Owner-only token: look up base asset metadata for IPFS/ANS
                        CNewAsset assetData;
                        if (currentActiveAssetCache->GetAssetMetaDataIfExists(name, assetData)) {
                            ipfsHash = assetData.strIPFSHash;
                            ansID = assetData.strANSID;
                        }
                    }
                    cachedBalances.append(AssetRecord(bal->first, bal->second, units, fIsAdministrator, EncodeAssetData(ipfsHash), ansID));
                }
            }
        }
    }
#endif


    int size() {
        return cachedBalances.size();
    }

    AssetRecord *index(int idx) {
        if (idx >= 0 && idx < cachedBalances.size()) {
            return &cachedBalances[idx];
        }
        return 0;
    }

};

AssetTableModel::AssetTableModel(WalletModel *parent) :
        QAbstractTableModel(parent),
        walletModel(parent),
        priv(new AssetTablePriv(this))
{
    columns << tr("Name") << tr("Quantity");
    // Note: Do NOT call refreshWallet() here. The constructor runs on a
    // worker thread (LoadWalletsActivity), and refreshWallet() acquires
    // cs_wallet then cs_main via chain interface calls, which conflicts
    // with other threads. The first checkBalanceChanged() call on the GUI
    // thread will populate the asset table safely.
};

AssetTableModel::~AssetTableModel()
{
    delete priv;
};

void AssetTableModel::checkBalanceChanged() {
#ifdef ENABLE_WALLET
    // Quick check: get new balances and compare with cached to avoid
    // expensive cs_main lock and Qt model reset when nothing changed.
    // Uses TRY_LOCK internally — returns false if the wallet lock is
    // busy, so we never block the GUI thread; the next 250ms poll retries.
    wallet::CWallet* pwallet = walletModel ? walletModel->wallet().wallet() : nullptr;
    std::map<std::string, CAmount> newAmounts;
    if (!GetAllMyAssetBalances(pwallet, newAmounts)) {
        return; // Wallet locked or no wallet — skip, try next poll
    }
    if (newAmounts == priv->cachedAmounts) {
        return; // No change — skip full refresh
    }
    priv->cachedAmounts = newAmounts;

    beginResetModel();
    priv->refreshWallet(&newAmounts);
    endResetModel();
#endif
}

int AssetTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int AssetTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant AssetTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    AssetRecord *rec = static_cast<AssetRecord*>(index.internalPointer());

    switch (role)
    {
        case AmountRole:
            return (unsigned long long) rec->quantity;
        case AssetNameRole:
            return QString::fromStdString(rec->name);
        case FormattedAmountRole:
            return QString::fromStdString(rec->formattedQuantity());
        case AdministratorRole:
            return rec->fIsAdministrator;
        case AssetIPFSHashRole:
            return QString::fromStdString(rec->ipfshash);
        case AssetIPFSHashDecorationRole:
        {
            if (index.column() == Quantity)
                return QVariant();

            if (rec->ipfshash.size() == 0)
                return QVariant();

            g_assetTableDecos.ensure();
            return darkModeEnabled ? *g_assetTableDecos.extLinkDark : *g_assetTableDecos.extLinkLight;
        }
        case AssetANSRole:
            return QString::fromStdString(rec->ansID);
        case AssetANSDecorationRole:
        {
            if (index.column() == Quantity)
                return QVariant();

            if (rec->ansID.size() == 0)
                return QVariant();

            g_assetTableDecos.ensure();
            return *g_assetTableDecos.exportIcon;
        }
        case Qt::DecorationRole:
        {
            if (index.column() == Quantity)
                return QVariant();

            if (!rec->fIsAdministrator)
                return QVariant();

            g_assetTableDecos.ensure();
            return darkModeEnabled ? *g_assetTableDecos.adminDark : *g_assetTableDecos.adminLight;
        }
        case Qt::DisplayRole: {
            if (index.column() == Name)
                return QString::fromStdString(rec->name);
            else if (index.column() == Quantity)
                return QString::fromStdString(rec->formattedQuantity());
            return QVariant();
        }
        case Qt::ToolTipRole:
            return formatTooltip(rec);
        case Qt::TextAlignmentRole:
        {
            if (index.column() == Quantity) {
                return QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
            }
            return QVariant();
        }
        default:
            return QVariant();
    }
}

QVariant AssetTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal) {
            if (section < columns.size())
                return columns.at(section);
        } else {
            return section;
        }
    } else if (role == Qt::SizeHintRole) {
        if (orientation == Qt::Vertical)
            return QSize(30, 50);
    } else if (role == Qt::TextAlignmentRole) {
        if (orientation == Qt::Vertical)
            return QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));

        return QVariant(int(Qt::AlignHCenter | Qt::AlignVCenter));
    }

    return QVariant();
}

QModelIndex AssetTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    AssetRecord *data = priv->index(row);
    if(data)
    {
        QModelIndex idx = createIndex(row, column, priv->index(row));
        return idx;
    }

    return QModelIndex();
}

QString AssetTableModel::formatTooltip(const AssetRecord *rec) const
{
    QString tooltip = formatAssetName(rec) + QString("\n") + formatAssetQuantity(rec) + QString("\n") + formatAssetData(rec);
    return tooltip;
}

QString AssetTableModel::formatAssetName(const AssetRecord *wtx) const
{
    return QString::fromStdString(wtx->name);
}

QString AssetTableModel::formatAssetQuantity(const AssetRecord *wtx) const
{
    return QString::fromStdString(wtx->formattedQuantity());
}

QString AssetTableModel::formatAssetData(const AssetRecord *wtx) const
{
    return QString::fromStdString(wtx->ipfshash);
}
