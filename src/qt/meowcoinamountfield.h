// Copyright (c) 2011-2021 The Meowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINAMOUNTFIELD_H
#define BITCOIN_QT_BITCOINAMOUNTFIELD_H

#include <consensus/amount.h>
#include <qt/meowcoinunits.h>

#include <QWidget>

class AmountSpinBox;

QT_BEGIN_NAMESPACE
class QValueComboBox;
QT_END_NAMESPACE

/** Widget for entering meowcoin amounts.
  */
class BitcoinAmountField: public QWidget
{
    Q_OBJECT

    // ugly hack: for some unknown reason CAmount (instead of qint64) does not work here as expected
    // discussion: https://github.com/meowcoin/meowcoin/pull/5117
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit BitcoinAmountField(QWidget *parent = nullptr);

    CAmount value(bool *value=nullptr) const;
    void setValue(const CAmount& value);

    /** If allow empty is set to false the field will be set to the minimum allowed value if left empty. **/
    void SetAllowEmpty(bool allow);

    /** Set the minimum value in satoshis **/
    void SetMinValue(const CAmount& value);

    /** Set the maximum value in satoshis **/
    void SetMaxValue(const CAmount& value);

    /** Set single step in satoshis **/
    void setSingleStep(const CAmount& step);

    /** Make read-only **/
    void setReadOnly(bool fReadOnly);

    /** Mark current value as invalid in UI. */
    void setValid(bool valid);
    /** Perform input validation, mark field as invalid if entered value is not valid. */
    bool validate();

    /** Change unit used to display amount. */
    void setDisplayUnit(BitcoinUnit new_unit);

    /** Make field empty and ready for new input. */
    void clear();

    /** Enable/Disable. */
    void setEnabled(bool fEnabled);

    /** Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907),
        in these cases we have to set it up manually.
    */
    QWidget *setupTabChain(QWidget *prev);

Q_SIGNALS:
    void valueChanged();

protected:
    /** Intercept focus-in event and ',' key presses */
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    AmountSpinBox* amount{nullptr};
    QValueComboBox *unit;

private Q_SLOTS:
    void unitChanged(int idx);

};

class AssetAmountField : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit AssetAmountField(QWidget *parent = nullptr);

    CAmount value(bool *value=nullptr) const;
    void setValue(const CAmount& value);
    void setUnit(int unit);
    void setMaxAmount(CAmount maxAmount);
    void SetAllowEmpty(bool allow);
    void SetMinValue(const CAmount& value);
    void SetMaxValue(const CAmount& value);
    void setSingleStep(const CAmount& step);
    void setReadOnly(bool fReadOnly);
    void setValid(bool valid);
    bool validate();
    void clear();
    void setEnabled(bool fEnabled);
    QWidget *setupTabChain(QWidget *prev);

Q_SIGNALS:
    void valueChanged();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    AmountSpinBox* amount{nullptr};
    int assetUnit{0};
};

#endif // BITCOIN_QT_BITCOINAMOUNTFIELD_H
