// Copyright (c) 2011-2020 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PSMTOPERATIONSDIALOG_H
#define BITCOIN_QT_PSMTOPERATIONSDIALOG_H

#include <QDialog>
#include <QString>

#include <psmt.h>
#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

namespace Ui {
class PSMTOperationsDialog;
}

/** Dialog showing transaction details. */
class PSMTOperationsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PSMTOperationsDialog(QWidget* parent, WalletModel* walletModel, ClientModel* clientModel);
    ~PSMTOperationsDialog();

    void openWithPSMT(PartiallySignedTransaction psmtx);

public Q_SLOTS:
    void signTransaction();
    void broadcastTransaction();
    void copyToClipboard();
    void saveTransaction();

private:
    Ui::PSMTOperationsDialog* m_ui;
    PartiallySignedTransaction m_transaction_data;
    WalletModel* m_wallet_model;
    ClientModel* m_client_model;

    enum class StatusLevel {
        INFO,
        WARN,
        ERR
    };

    size_t couldSignInputs(const PartiallySignedTransaction &psmtx);
    void updateTransactionDisplay();
    QString renderTransaction(const PartiallySignedTransaction &psmtx);
    void showStatus(const QString &msg, StatusLevel level);
    void showTransactionStatus(const PartiallySignedTransaction &psmtx);
};

#endif // BITCOIN_QT_PSMTOPERATIONSDIALOG_H
