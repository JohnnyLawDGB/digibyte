<<<<<<< HEAD
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2020 The DigiByte Core developers
=======
// Copyright (c) 2011-2021 The DigiByte Core developers
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/openuridialog.h>
#include <qt/forms/ui_openuridialog.h>

#include <qt/guiutil.h>
<<<<<<< HEAD
=======
#include <qt/platformstyle.h>
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
#include <qt/sendcoinsrecipient.h>

#include <QAbstractButton>
#include <QLineEdit>
#include <QUrl>

<<<<<<< HEAD
OpenURIDialog::OpenURIDialog(QWidget *parent) :
    QDialog(parent, GUIUtil::dialog_flags),
    ui(new Ui::OpenURIDialog)
{
    ui->setupUi(this);
=======
OpenURIDialog::OpenURIDialog(const PlatformStyle* platformStyle, QWidget* parent) : QDialog(parent, GUIUtil::dialog_flags),
                                                                                    ui(new Ui::OpenURIDialog),
                                                                                    m_platform_style(platformStyle)
{
    ui->setupUi(this);
    ui->pasteButton->setIcon(m_platform_style->SingleColorIcon(":/icons/editpaste"));
    QObject::connect(ui->pasteButton, &QAbstractButton::clicked, ui->uriEdit, &QLineEdit::paste);
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

    GUIUtil::handleCloseWindowShortcut(this);
}

OpenURIDialog::~OpenURIDialog()
{
    delete ui;
}

QString OpenURIDialog::getURI()
{
    return ui->uriEdit->text();
}

void OpenURIDialog::accept()
{
    SendCoinsRecipient rcp;
<<<<<<< HEAD
    if(GUIUtil::parseDigiByteURI(getURI(), &rcp))
    {
=======
    if (GUIUtil::parseDigiByteURI(getURI(), &rcp)) {
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
        /* Only accept value URIs */
        QDialog::accept();
    } else {
        ui->uriEdit->setValid(false);
    }
}
<<<<<<< HEAD
=======

void OpenURIDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange) {
        ui->pasteButton->setIcon(m_platform_style->SingleColorIcon(":/icons/editpaste"));
    }

    QDialog::changeEvent(e);
}
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
