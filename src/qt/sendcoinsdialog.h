// Copyright (c) 2011-2022 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SENDCOINSDIALOG_H
#define BITCOIN_QT_SENDCOINSDIALOG_H

#include <primitives/transaction_identifier.h>
#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QTimer>

class PlatformStyle;
class SendCoinsEntry;
class SendCoinsRecipient;
enum class SynchronizationState;
namespace wallet {
class CCoinControl;
} // namespace wallet

namespace Ui {
    class SendCoinsDialog;
}

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

/** Dialog for sending meowcoins */
class SendCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCoinsDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~SendCoinsDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void setAddress(const QString &address);
    void pasteEntry(const SendCoinsRecipient &rv);
    bool handlePaymentRequest(const SendCoinsRecipient &recipient);

    // Only used for testing-purposes
    wallet::CCoinControl* getCoinControl() { return m_coin_control.get(); }

public Q_SLOTS:
    void clear();
    void reject() override;
    void accept() override;
    SendCoinsEntry *addEntry();
    void updateTabsAndLabels();
    void setBalance(const interfaces::WalletBalances& balances);

Q_SIGNALS:
    void coinsSent(const Txid& txid);

private:
    Ui::SendCoinsDialog *ui;
    ClientModel* clientModel{nullptr};
    WalletModel* model{nullptr};
    std::unique_ptr<wallet::CCoinControl> m_coin_control;
    std::unique_ptr<WalletModelTransaction> m_current_transaction;
    bool fNewRecipientAllowed{true};
    bool fFeeMinimized{true};
    const PlatformStyle *platformStyle;

    // Copy PSMT to clipboard and offer to save it.
    void presentPSMT(PartiallySignedTransaction& psmt);
    // Process WalletModel::SendCoinsReturn and generate a pair consisting
    // of a message and message flags for use in Q_EMIT message().
    // Additional parameter msgArg can be used via .arg(msgArg).
    void processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg = QString());
    void minimizeFeeSection(bool fMinimize);
    // Format confirmation message
    bool PrepareSendText(QString& question_string, QString& informative_text, QString& detailed_text);
    /* Sign PSMT using external signer.
     *
     * @param[in,out] psmtx the PSMT to sign
     * @param[in,out] mtx needed to attempt to finalize
     * @param[in,out] complete whether the PSMT is complete (a successfully signed multisig transaction may not be complete)
     *
     * @returns false if any failure occurred, which may include the user rejection of a transaction on the device.
     */
    bool signWithExternalSigner(PartiallySignedTransaction& psmt, CMutableTransaction& mtx, bool& complete);
    void updateFeeMinimizedLabel();
    void updateCoinControlState();

private Q_SLOTS:
    void sendButtonClicked(bool checked);
    void on_buttonChooseFee_clicked();
    void on_buttonMinimizeFee_clicked();
    void removeEntry(SendCoinsEntry* entry);
    void useAvailableBalance(SendCoinsEntry* entry);
    void refreshBalance();
    void coinControlFeatureChanged(bool);
    void coinControlButtonClicked();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    void coinControlChangeChecked(Qt::CheckState);
#else
    void coinControlChangeChecked(int);
#endif
    void coinControlChangeEdited(const QString &);
    void coinControlUpdateLabels();
    void coinControlClipboardQuantity();
    void coinControlClipboardAmount();
    void coinControlClipboardFee();
    void coinControlClipboardAfterFee();
    void coinControlClipboardBytes();
    void coinControlClipboardChange();
    void updateFeeSectionControls();
    void updateNumberOfBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, SyncType synctype, SynchronizationState sync_state);
    void updateSmartFeeLabel();

Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
};


#define SEND_CONFIRM_DELAY   3

class SendConfirmationDialog : public QMessageBox
{
    Q_OBJECT

public:
    SendConfirmationDialog(const QString& title, const QString& text, const QString& informative_text = "", const QString& detailed_text = "", int secDelay = SEND_CONFIRM_DELAY, bool enable_send = true, bool always_show_unsigned = true, QWidget* parent = nullptr);
    /* Returns QMessageBox::Cancel, QMessageBox::Yes when "Send" is
       clicked and QMessageBox::Save when "Create Unsigned" is clicked. */
    int exec() override;

private Q_SLOTS:
    void countDown();
    void updateButtons();

private:
    QAbstractButton *yesButton;
    QAbstractButton *m_psmt_button;
    QTimer countDownTimer;
    int secDelay;
    QString confirmButtonText{tr("Send")};
    bool m_enable_send;
    QString m_psmt_button_text{tr("Create Unsigned")};
};

#endif // BITCOIN_QT_SENDCOINSDIALOG_H
