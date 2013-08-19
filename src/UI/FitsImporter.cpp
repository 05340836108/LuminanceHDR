/**
 * This file is a part of LuminanceHDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2013 Franco Comida
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * @author Franco Comida <fcomida@users.sourceforge.net>
 */

#include <QColorDialog>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QtConcurrentMap>
#include <QtConcurrentFilter>
#include <QDebug>

#include <valarray> 

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include "FitsImporter.h"
#include "ui_FitsImporter.h"

#include "Common/CommonFunctions.h"
#include "Libpfs/manip/rotate.h"
#include <Libpfs/utils/transform.h>
#include "HdrCreation/mtb_alignment.h"

using namespace pfs;

static const int previewWidth = 300;
static const int previewHeight = 200;

FitsImporter::~FitsImporter() 
{
    if (m_align)
        delete m_align;
}

FitsImporter::FitsImporter(QWidget *parent)
    : QWizard(parent)
    , m_align(NULL)
    , m_ui(new Ui::FitsImporter)
{
    m_ui->setupUi(this);

#ifdef WIN32
    setWizardStyle(WizardStyle::ModernStyle);
#endif
#ifdef Q_WS_MAC
    this->setWindowModality(Qt::WindowModal); // In OS X, the QMessageBox is modal to the window
#endif
    m_previewLabel = new QLabel(this);
    m_previewLabel->resize(600,400);
    m_previewLabel->setScaledContents(true);
    QPalette* palette = new QPalette(); 
    palette->setColor(QPalette::Foreground,Qt::red);
    m_previewLabel->setPalette(*palette);
    m_previewLabel->setFrameStyle(QFrame::Box);
    m_previewLabel->setLineWidth(3);
    m_previewLabel->hide();
    m_previewFrame = new PreviewFrame;
    m_ui->verticalLayoutPreviews->addWidget(m_previewFrame);
    m_previewFrame->addLabel(new SimplePreviewLabel(0));
    m_previewFrame->addLabel(new SimplePreviewLabel(1));
    m_previewFrame->addLabel(new SimplePreviewLabel(2));
    m_previewFrame->addLabel(new SimplePreviewLabel(3));
    m_previewFrame->addLabel(new SimplePreviewLabel(4));
    m_previewFrame->show();
    m_previewFrame->getLabel(0)->setEnabled(false);
    m_previewFrame->getLabel(1)->setEnabled(false);
    m_previewFrame->getLabel(2)->setEnabled(false);
    m_previewFrame->getLabel(3)->setEnabled(false);
    m_previewFrame->getLabel(4)->setEnabled(false);
    m_ui->groupBoxMessages->setVisible(false);
    m_ui->progressBar->setVisible(false);
    connect(this, SIGNAL(setValue(int)), m_ui->progressBar, SLOT(setValue(int)), Qt::DirectConnection);
    connect(this, SIGNAL(setRange(int,int)), m_ui->progressBar, SLOT(setRange(int,int)), Qt::DirectConnection);
    connect(m_previewFrame->getLabel(0), SIGNAL(selected(int)), this, SLOT(previewLabelSelected(int)));
    connect(m_previewFrame->getLabel(1), SIGNAL(selected(int)), this, SLOT(previewLabelSelected(int)));
    connect(m_previewFrame->getLabel(2), SIGNAL(selected(int)), this, SLOT(previewLabelSelected(int)));
    connect(m_previewFrame->getLabel(3), SIGNAL(selected(int)), this, SLOT(previewLabelSelected(int)));
    connect(m_previewFrame->getLabel(4), SIGNAL(selected(int)), this, SLOT(previewLabelSelected(int)));

    initColors();
}

void FitsImporter::initColors()
{
    m_colorRed = QColor(255, 0, 0);
    m_colorGreen = QColor(0, 255, 0);
    m_colorBlue = QColor(0, 0, 255);
    m_colorH = QColor(255, 0, 0);
    
    m_ui->pushButtonColorRed->setStyleSheet("background-color:" + m_colorRed.name());
    m_ui->pushButtonColorGreen->setStyleSheet("background-color:" + m_colorGreen.name());
    m_ui->pushButtonColorBlue->setStyleSheet("background-color:" + m_colorBlue.name());
    m_ui->pushButtonColorH->setStyleSheet("background-color:" + m_colorH.name());
}

void FitsImporter::selectInputFile(QLineEdit* textField, QString* channel)
{
    QString filetypes = "FITS (*.fit *.FIT *.fits *.FITS);;";
    *channel = QFileDialog::getOpenFileName(this, tr("Load one FITS image..."),
                                                      m_luminance_options.getDefaultPathHdrIn(),
                                                      filetypes );
    textField->setText(*channel);
    checkLoadButton();

    if (!channel->isEmpty())
    {
        QFileInfo qfi(*channel);
        m_luminance_options.setDefaultPathHdrIn( qfi.path() );
    }

    if (!m_luminosityChannel.isEmpty() && !m_redChannel.isEmpty() && !m_greenChannel.isEmpty() &&
        !m_blueChannel.isEmpty() && !m_hChannel.isEmpty())
    {
        on_pushButtonLoad_clicked();
    }
}

void FitsImporter::on_pushButtonLuminosity_clicked()
{
    selectInputFile(m_ui->lineEditLuminosity, &m_luminosityChannel);
}

void FitsImporter::on_pushButtonRed_clicked()
{
    selectInputFile(m_ui->lineEditRed, &m_redChannel);
}

void FitsImporter::on_pushButtonGreen_clicked()
{
    selectInputFile(m_ui->lineEditGreen, &m_greenChannel);
}

void FitsImporter::on_pushButtonBlue_clicked()
{
    selectInputFile(m_ui->lineEditBlue, &m_blueChannel);
}

void FitsImporter::on_pushButtonH_clicked()
{
    selectInputFile(m_ui->lineEditH, &m_hChannel);
}

void FitsImporter::checkLoadButton()
{
    m_ui->pushButtonLoad->setEnabled(!m_redChannel.isEmpty() ||
                                     !m_greenChannel.isEmpty() ||
                                     !m_blueChannel.isEmpty() ||
                                     !m_luminosityChannel.isEmpty() ||
                                     !m_hChannel.isEmpty());
}

void FitsImporter::on_pushButtonLoad_clicked()
{
    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );
    m_ui->pushButtonLoad->setEnabled(false);
    m_data.clear();
    m_tmpdata.clear();
    m_channels.clear();
    m_contents.clear();
    m_qimages.clear();
    m_channels << m_redChannel
               << m_greenChannel
               << m_blueChannel
               << m_luminosityChannel
               << m_hChannel;

    BOOST_FOREACH(const QString& i, m_channels) {
        m_tmpdata.push_back( HdrCreationItem(i) );
    }

    // parallel load of the data...
    connect(&m_futureWatcher, SIGNAL(finished()), this, SLOT(loadFilesDone()), Qt::DirectConnection);

    // Start the computation.
    m_futureWatcher.setFuture( QtConcurrent::map(m_tmpdata.begin(), m_tmpdata.end(), LoadFile()) );
}

void FitsImporter::loadFilesDone()
{ 
    qDebug() << "Data loaded ... move to internal structure!";
    disconnect(&m_futureWatcher, SIGNAL(finished()), this, SLOT(loadFilesDone()));

    int idx = 0;
    BOOST_FOREACH(const HdrCreationItem& i, m_tmpdata) {
        if ( i.isValid() ) {
            qDebug() << QString("Insert data for %1").arg(i.filename());
            m_data.push_back(i);
            m_previewFrame->getLabel(idx)->setPixmap(QPixmap::fromImage(*(i.qimage())));
            m_previewFrame->getLabel(idx)->setEnabled(true);
        }
        else if (i.filename().isEmpty()) {
            m_data.push_back(i);
            idx++;
            continue;
        }
        else {
            QMessageBox::warning(0,"", tr("Cannot load FITS image %1").arg(i.filename()), QMessageBox::Ok, QMessageBox::NoButton);
            m_data.clear();
            m_tmpdata.clear();
            m_channels.clear();
            m_contents.clear();
            m_qimages.clear();
            m_ui->pushButtonLoad->setEnabled(true);
            QApplication::restoreOverrideCursor();
            return;
        }
        idx++;
    }

    size_t i;
    for (i = 0; i < m_tmpdata.size(); i++)
        if (!m_tmpdata[i].filename().isEmpty())
            break;
    m_previewFrame->labelSelected(i);

    if (!framesHaveSameSize()) {
        QMessageBox::warning(0,"", tr("FITS images have different size"), QMessageBox::Ok, QMessageBox::NoButton);
        m_data.clear();
        m_tmpdata.clear();
        m_channels.clear();
        m_contents.clear();
        m_qimages.clear();
        m_ui->pushButtonLoad->setEnabled(true);
        m_ui->pushButtonPreview->setEnabled(true);
        QApplication::restoreOverrideCursor();
        return;
    }
    m_ui->pushButtonLoad->setEnabled(true);
    // TODO: 
    //m_ui->pushButtonOK->setEnabled(true);
    m_ui->pushButtonClockwise->setEnabled(true);
    m_ui->pushButtonPreview->setEnabled(true);
    //m_ui->dsbRedRed->setEnabled(true);
    //m_ui->dsbRedGreen->setEnabled(true);
    //m_ui->dsbRedBlue->setEnabled(true);
    //m_ui->dsbGreenRed->setEnabled(true);
    //m_ui->dsbGreenGreen->setEnabled(true);
    //m_ui->dsbGreenBlue->setEnabled(true);
    //m_ui->dsbBlueRed->setEnabled(true);
    //m_ui->dsbBlueGreen->setEnabled(true);
    //m_ui->dsbBlueBlue->setEnabled(true);
    //m_ui->hsRedRed->setEnabled(true);
    //m_ui->hsRedGreen->setEnabled(true);
    //m_ui->hsRedBlue->setEnabled(true);
    //m_ui->hsGreenRed->setEnabled(true);
    //m_ui->hsGreenGreen->setEnabled(true);
    //m_ui->hsGreenBlue->setEnabled(true);
    //m_ui->hsBlueRed->setEnabled(true);
    //m_ui->hsBlueGreen->setEnabled(true);
    //m_ui->hsBlueBlue->setEnabled(true);
    m_tmpdata.clear();
    buildContents();
    buildPreview();
    QApplication::restoreOverrideCursor();
}

void FitsImporter::on_pushButtonOK_clicked()
{
    if (m_ui->alignCheckBox->isChecked())
        if (m_ui->ais_radioButton->isChecked()) {
            m_ui->groupBoxMessages->setVisible(true);
            m_ui->progressBar->setVisible(true);
            align_with_ais();
        }
        else
            align_with_mtb();
    else
        buildFrame();
}

bool isValid(HdrCreationItem& item)
{
    return !item.filename().isEmpty();
}

void FitsImporter::buildPreview()
{
    double alphaRed = m_ui->dsbChannelRed->value();
    double alphaGreen = m_ui->dsbChannelGreen->value();
    double alphaBlue = m_ui->dsbChannelBlue->value();
    double alphaH = m_ui->dsbChannelH->value();

    float redRed = m_colorRed.redF() * alphaRed;
    float redGreen = m_colorRed.greenF() * alphaRed;
    float redBlue = m_colorRed.blueF() * alphaRed;
    float greenRed = m_colorGreen.redF() * alphaGreen;
    float greenGreen = m_colorGreen.greenF() * alphaGreen;
    float greenBlue = m_colorGreen.blueF() * alphaGreen;
    float blueRed = m_colorBlue.redF() * alphaBlue;
    float blueGreen = m_colorBlue.greenF() * alphaBlue;
    float blueBlue = m_colorBlue.blueF() * alphaBlue;
    float hRed = m_colorH.redF() * alphaH;
    float hGreen = m_colorH.greenF() * alphaH;
    float hBlue = m_colorH.blueF() * alphaH;

    QImage tempImage(previewWidth,
                     previewHeight,
                     QImage::Format_ARGB32_Premultiplied);
    
    for (int j = 0; j < previewHeight; j++) 
    {
        for (int i = 0; i < previewWidth; i++) 
        {
            float red = qRed(m_qimages[0].pixel(i, j))/255.0f;
            float green = qRed(m_qimages[1].pixel(i, j))/255.0f;
            float blue = qRed(m_qimages[2].pixel(i, j))/255.0f;
            float h_alpha = qRed(m_qimages[4].pixel(i, j))/255.0f;
            float r = redRed * red + greenRed * green + blueRed * blue + hRed * h_alpha;
            float g = redGreen * red + greenGreen * green + blueGreen * blue + hGreen * h_alpha;
            float b = redBlue * red + greenBlue * green + blueBlue * blue + hBlue * h_alpha;

            if (!m_luminosityChannel.isEmpty())
            {
                float luminance = qRed(m_qimages[3].pixel(i, j))/255.0f;
                float h, s, l;
                rgb2hsl(r, g, b, h, s, l);
                hsl2rgb(h, s, luminance, r, g, b);
            }
            QRgb rgb;
            ConvertToQRgb convert;
            convert(r, g, b, rgb); 
            tempImage.setPixel(i, j, rgb);
        }
    } 


    m_ui->previewLabel->setPixmap(QPixmap::fromImage(tempImage));
}

void FitsImporter::buildContents()
{
    HdrCreationItemContainer::iterator it;
    it = std::find_if(m_data.begin(), m_data.end(), isValid);
    m_width = it->frame()->getWidth();
    m_height = it->frame()->getHeight();

    for (size_t i = 0; i < m_data.size(); i++) {
         m_contents.push_back(std::vector<float>(m_width*m_height));
    }

    for (size_t i = 0; i < m_data.size(); i++) {
        if (m_data[i].filename().isEmpty()) {
            std::fill(m_contents[i].begin(), m_contents[i].end(), 0.0f);
            QImage tmpImage(previewWidth, previewHeight, QImage::Format_ARGB32_Premultiplied);
            tmpImage.fill(0);
            m_qimages.push_back(tmpImage);
        }
        else {
            Channel *C = m_data[i].frame()->getChannel("X");
            std::copy(C->begin(), C->end(), m_contents[i].begin()); 
            m_qimages.push_back(m_data[i].qimage()->scaled(previewWidth, previewHeight));
            
        }
    }
}

void FitsImporter::buildFrame()
{
    //float redRed = m_ui->dsbRedRed->value();
    //float redGreen = m_ui->dsbRedGreen->value();
    //float redBlue = m_ui->dsbRedBlue->value();
    //float greenRed = m_ui->dsbGreenRed->value();
    //float greenGreen = m_ui->dsbGreenGreen->value();
    //float greenBlue = m_ui->dsbGreenBlue->value();
    //float blueRed = m_ui->dsbBlueRed->value();
    //float blueGreen = m_ui->dsbBlueGreen->value();
    //float blueBlue = m_ui->dsbBlueBlue->value();

    //m_frame = new Frame(m_width, m_height);
    //Channel *Xc, *Yc, *Zc;
    //m_frame->createXYZChannels(Xc, Yc, Zc);

    //if (m_luminosityChannel != "") {
    //    for (size_t i = 0; i < m_width*m_height; i++) 
    //    {
    //        float r = redRed * m_contents[0][i] + redGreen * m_contents[1][i] + redBlue * m_contents[2][i];
    //        float g = greenRed * m_contents[0][i] + greenGreen * m_contents[1][i] + greenBlue * m_contents[2][i];
    //        float b = blueRed * m_contents[0][i] + blueGreen * m_contents[1][i] + blueBlue * m_contents[2][i];
    //        float h, s, l;
    //        rgb2hsl(r, g, b, h, s, l);
    //        hsl2rgb(h, s, m_contents[3][i], r, g, b);
    //        (*Xc)(i) = r + m_contents[4][i];
    //        (*Yc)(i) = g;
    //        (*Zc)(i) = b;
    //    } 
    //}
    //else {
    //    for (size_t i = 0; i < m_width*m_height; i++) 
    //    {
    //        float r = redRed * m_contents[0][i] + redGreen * m_contents[1][i] + redBlue * m_contents[2][i];
    //        float g = greenRed * m_contents[0][i] + greenGreen * m_contents[1][i] + greenBlue * m_contents[2][i];
    //        float b = blueRed * m_contents[0][i] + blueGreen * m_contents[1][i] + blueBlue * m_contents[2][i];
    //        (*Xc)(i) = r + m_contents[4][i];
    //        (*Yc)(i) = g;
    //        (*Zc)(i) = b;
    //    } 
    //}    
    //accept();
}

void FitsImporter::align_with_ais()
{
    m_contents.clear();
    m_align = new Align(&m_data, false, 2, 0.0f, 65535.0f); 
    connect(m_align, SIGNAL(finishedAligning(int)), this, SLOT(ais_finished(int)));
    connect(m_align, SIGNAL(failedAligning(QProcess::ProcessError)), this, SLOT(ais_failed_slot(QProcess::ProcessError)));
    connect(m_align, SIGNAL(dataReady(QByteArray)), this, SLOT(readData(QByteArray)));
  
    m_align->align_with_ais(m_ui->autoCropCheckBox->isChecked());

    // TODO:
    //m_ui->pushButtonOK->setEnabled(false);
}

void FitsImporter::ais_finished(int exitcode)
{
    m_align->removeTempFiles();
    if (exitcode != 0)
    {
        QApplication::restoreOverrideCursor();
        qDebug() << "align_image_stack exited with exit code " << exitcode;
        QMessageBox::warning(0,"", tr("align_image_stack exited with exit code %1").arg(exitcode), QMessageBox::Ok, QMessageBox::NoButton);
    }
    else {
        QApplication::restoreOverrideCursor();
        buildContents();
        buildFrame();
    }
}

void FitsImporter::ais_failed_slot(QProcess::ProcessError error)
{
    qDebug() << "align_image_stack failed";
    m_align->removeTempFiles();
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(0,"", tr("align_image_stack failed with error"), QMessageBox::Ok, QMessageBox::NoButton);
}

bool FitsImporter::framesHaveSameSize()
{
    HdrCreationItemContainer::iterator it;
    it = std::find_if(m_data.begin(), m_data.end(), isValid);
    const size_t width = it->frame()->getWidth();
    const size_t height = it->frame()->getHeight();
    for ( HdrCreationItemContainer::const_iterator it = m_data.begin() + 1, 
          itEnd = m_data.end(); it != itEnd; ++it) {
        if (it->filename().isEmpty())
            continue;
        if (it->frame()->getWidth() != width || it->frame()->getHeight() != height)
            return false; 
    }
    return true;
}

void FitsImporter::readData(QByteArray data)
{
    qDebug() << data;
    if (data.contains("[1A"))
        data.replace("[1A", "");
    if (data.contains("[2A"))
        data.replace("[2A", "");
    if (data.contains(QChar(0x01B).toAscii()))
        data.replace(QChar(0x01B).toAscii(), "");

    m_ui->textEdit->append(data);
    if (data.contains(": remapping")) {
        QRegExp exp("\\:\\s*(\\d+)\\s*");
        exp.indexIn(QString(data.data()));
        emit setRange(0, 100);
        emit setValue(exp.cap(1).toInt());
    }
}

void FitsImporter::align_with_mtb()
{
    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );
    m_contents.clear();
    // build temporary container...
    vector<FramePtr> frames;
    for (size_t i = 0; i < m_data.size(); ++i) {
        if (!m_data[i].filename().isEmpty())
            frames.push_back( m_data[i].frame() );
    }
    // run MTB
    libhdr::mtb_alignment(frames);

    QApplication::restoreOverrideCursor();
    buildContents();
    buildFrame();
}

void FitsImporter::on_pushButtonClockwise_clicked()
{
    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );
    m_ui->pushButtonClockwise->setEnabled(false);
    int index = m_previewFrame->getSelectedLabel();
    Frame *toRotate = (m_data[index].frame()).get();
    Frame *rotatedHalf = pfs::rotate(toRotate, true);
    Frame *rotated = pfs::rotate(rotatedHalf, true);
    m_data[index].frame()->swap(*rotated);
    delete rotatedHalf;
    RefreshPreview refresh;
    refresh(m_data[index]);
    m_previewFrame->getLabel(index)->setPixmap(QPixmap::fromImage(*(m_data[index].qimage())));
    if (m_ui->pushButtonPreview->isChecked()) {
        m_previewLabel->setPixmap(*m_previewFrame->getLabel(m_previewFrame->getSelectedLabel())->pixmap());
    }
    Channel *C = m_data[index].frame()->getChannel("X");
    std::copy(C->begin(), C->end(), m_contents[index].begin()); 
    QImage tmp = m_data[index].qimage()->scaled(previewWidth, previewHeight);
    m_qimages[index].swap(tmp);
    buildPreview();
    m_ui->pushButtonClockwise->setEnabled(true);
    QApplication::restoreOverrideCursor();
}

void FitsImporter::on_pushButtonPreview_clicked()
{
    if (m_ui->pushButtonPreview->isChecked()) {
        m_previewLabel->setPixmap(*m_previewFrame->getLabel(m_previewFrame->getSelectedLabel())->pixmap());
        m_previewLabel->show();
    }
    else
        m_previewLabel->hide();
}

void FitsImporter::previewLabelSelected(int index)
{
    if (m_ui->pushButtonPreview->isChecked()) {
        m_previewLabel->setPixmap(*m_previewFrame->getLabel(m_previewFrame->getSelectedLabel())->pixmap());
    }
}

void FitsImporter::hScrollValueChanged(int newValue, QDoubleSpinBox* spinBox)
{
    float value = ((float)newValue)/10000.f;
    bool oldState = spinBox->blockSignals(true);
    spinBox->setValue( value );
    spinBox->blockSignals(oldState);
    buildPreview();
}

void FitsImporter::spinBoxValueChanged(int newValue, QSlider* slider)
{
    bool oldState = slider->blockSignals(true);
    slider->setValue( (int)(newValue*10000) );
    slider->blockSignals(oldState);
    buildPreview();
}

void FitsImporter::on_hsChannelRed_valueChanged(int newValue)
{
    hScrollValueChanged(newValue, m_ui->dsbChannelRed);
}

void FitsImporter::on_dsbChannelRed_valueChanged(double newValue)
{
    spinBoxValueChanged(newValue, m_ui->hsChannelRed);
}

void FitsImporter::on_hsChannelGreen_valueChanged(int newValue)
{
    hScrollValueChanged(newValue, m_ui->dsbChannelGreen);
}

void FitsImporter::on_dsbChannelGreen_valueChanged(double newValue)
{
    spinBoxValueChanged(newValue, m_ui->hsChannelGreen);
}

void FitsImporter::on_hsChannelBlue_valueChanged(int newValue)
{
    hScrollValueChanged(newValue, m_ui->dsbChannelBlue);
}

void FitsImporter::on_dsbChannelBlue_valueChanged(double newValue)
{
    spinBoxValueChanged(newValue, m_ui->hsChannelBlue);
}

void FitsImporter::on_hsChannelH_valueChanged(int newValue)
{
    hScrollValueChanged(newValue, m_ui->dsbChannelH);
}

void FitsImporter::on_dsbChannelH_valueChanged(double newValue)
{
    spinBoxValueChanged(newValue, m_ui->hsChannelH);
}


void FitsImporter::on_pushButtonColorRed_clicked()
{
    chooseColor(m_ui->pushButtonColorRed, m_colorRed);
}

void FitsImporter::on_pushButtonColorGreen_clicked()
{
    chooseColor(m_ui->pushButtonColorGreen, m_colorGreen);
}

void FitsImporter::on_pushButtonColorBlue_clicked()
{
    chooseColor(m_ui->pushButtonColorBlue, m_colorBlue);
}

void FitsImporter::on_pushButtonColorH_clicked()
{
    chooseColor(m_ui->pushButtonColorH, m_colorH);
}

void FitsImporter::chooseColor(QWidget* colorWidget, QColor& mColor)
{    
    QColor selectedColor = QColorDialog::getColor();   
    if (selectedColor.isValid())
    {
        bool newColor = mColor != selectedColor;
        mColor = selectedColor;
        QString colorName = selectedColor.name();
        // auto-fill-background of the button must be active
        colorWidget->setStyleSheet("background-color:" + colorName);
        if (newColor)
            buildPreview();
    }
}
