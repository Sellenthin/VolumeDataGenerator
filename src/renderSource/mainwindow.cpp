/**
 * \file
 *
 * \author Valentin Bruder
 *
 * \copyright Copyright (C) 2018 Valentin Bruder
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QString>
#include <QUrl>
#include <QFuture>
#include <QtConcurrentRun>
#include <QThread>
#include <QColorDialog>
#include <QtGlobal>
#include <QDoubleSpinBox>
#include <QComboBox>

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , _fileName("No volume data loaded yet.")
{
    QCoreApplication::setOrganizationName("VISUS");
    QCoreApplication::setOrganizationDomain("www.visus.uni-stuttgart.de");
    QCoreApplication::setApplicationName("Lightweight Volume Raycaster");
    _settings = new QSettings();

    setAcceptDrops( true );
    ui->setupUi(this);
    ui->gbTimeSeries->setVisible(false);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->gbTimeSeries, &QGroupBox::setVisible);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->sbTimeStep, &QSpinBox::setMaximum);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->sldTimeStep, &QSlider::setMaximum);
    connect(ui->sldTimeStep, &QSlider::valueChanged,
            ui->volumeRenderWidget, &VolumeRenderWidget::setTimeStep);

    // menu bar actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openVolumeFile);
    connect(ui->actionSaveTff, &QAction::triggered, this, &MainWindow::saveTff);
    connect(ui->actionSaveRawTff, &QAction::triggered, this, &MainWindow::saveRawTff);
    connect(ui->actionLoadTff, &QAction::triggered, this, &MainWindow::loadTff);
	connect(ui->actionRandomData, &QAction::triggered, this, &MainWindow::randomData);

    // future watcher for concurrent data loading
    _watcher = new QFutureWatcher<void>(this);
    connect(_watcher, &QFutureWatcher<void>::finished, this, &MainWindow::finishedLoading);
    // loading progress bar
    _progBar.setRange(0, 100);
    _progBar.setTextVisible(true);
    _progBar.setAlignment(Qt::AlignCenter);
    connect(&_timer, &QTimer::timeout, this, &MainWindow::addProgress);

    // connect settings UI
    connect(ui->dsbSamplingRate, 
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::updateSamplingRate);
    connect(ui->dsbImgSampling, 
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::setImageSamplingRate);
    connect(ui->chbLinear, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setLinearInterpolation);
    connect(ui->chbIllum, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setIllumination);
    connect(ui->chbAmbientOcclusion, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setAmbientOcclusion);
    connect(ui->chbBox, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setDrawBox);
    connect(ui->chbOrtho, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setCamOrtho);
    connect(ui->pbBgColor, &QPushButton::released, this, &MainWindow::chooseBackgroundColor);
    // connect tff editor
    connect(ui->transferFunctionEditor->getEditor(), &TransferFunctionEditor::gradientStopsChanged,
            ui->volumeRenderWidget, &VolumeRenderWidget::updateTransferFunction);
    connect(ui->pbResetTff, &QPushButton::clicked,
            ui->transferFunctionEditor, &TransferFunctionWidget::resetTransferFunction);
    connect(ui->cbInterpolation, 
            static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::setTffInterpolation);
    connect(ui->cbInterpolation, SIGNAL(currentIndexChanged(QString)),
            ui->transferFunctionEditor, SLOT(setInterpolation(QString)));
//    connect(ui->cbInterpolation, qOverload<const QString &>(&QComboBox::currentIndexChanged),
//            ui->transferFunctionEditor, &TransferFunctionEditor::setInterpolation);

    connect(ui->transferFunctionEditor->getEditor(), &TransferFunctionEditor::selectedPointChanged,
            ui->colorWheel, &colorwidgets::ColorWheel::setColor);
    connect(ui->colorWheel, &colorwidgets::ColorWheel::colorChanged,
            ui->transferFunctionEditor, &TransferFunctionWidget::setColorSelected);

	connect(ui->pbSetConfig, &QPushButton::clicked, this, &MainWindow::setConfig);

    ui->statusBar->addPermanentWidget(&_statusLabel);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::frameSizeChanged,
            this, &MainWindow::setStatusText);

    // TODO: custom double slider for tff editor zoom
    // Load an application style
//    QFile styleFile( "../RaycastLight/style-rangeSlider.qss" );
//    styleFile.open(QFile::ReadOnly);
    // Apply the loaded stylesheet
//    QString style = QLatin1String( styleFile.readAll() );
//    ui->rsldTffZoomX->setStyleSheet(style);
//    ui->rsldTffZoomX->style()->unpolish(ui->rsldTffZoomX);
//    ui->rsldTffZoomX->style()->polish(ui->rsldTffZoomX);

    // restore settings
    readSettings();
}


/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
    delete _watcher;
    delete _settings;
    delete ui;
}


/**
 * @brief MainWindow::closeEvent
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_P && event->modifiers() == Qt::CTRL)
        ui->volumeRenderWidget->saveFrame();
    else if (event->key() == Qt::Key_V && event->modifiers() == Qt::CTRL)
        ui->volumeRenderWidget->toggleVideoRecording();

    event->accept();
}


/**
 * @brief MainWindow::writeSettings
 */
void MainWindow::writeSettings()
{
    _settings->beginGroup("MainWindow");
    _settings->setValue("size", size());
    _settings->setValue("pos", pos());
    _settings->endGroup();

    _settings->beginGroup("Settings");
    // TODO
    _settings->endGroup();
}


/**
 * @brief MainWindow::readSettings
 */
void MainWindow::readSettings()
{
    _settings->beginGroup("MainWindow");
    resize(_settings->value("size", QSize(400, 400)).toSize());
    move(_settings->value("pos", QPoint(200, 200)).toPoint());
    _settings->endGroup();

    _settings->beginGroup("Settings");
    // todo
    _settings->endGroup();
}


/**
 * @brief MainWindow::setVolumeData
 * @param fileName
 */
void MainWindow::setVolumeData(const QString &fileName)
{
    ui->volumeRenderWidget->setVolumeData(fileName);
}


/**
 * @brief MainWindow::readVolumeFile
 * @param fileName
 * @return
 */
bool MainWindow::readVolumeFile(const QString &fileName)
{
    _progBar.setFormat("Loading volume file: " + fileName);
    _progBar.setValue(1);
    _progBar.show();
    ui->statusBar->addPermanentWidget(&_progBar, 2);
    ui->statusBar->updateGeometry();
    QApplication::processEvents();

    if (fileName.isEmpty())
    {
        _progBar.deleteLater();
        throw std::invalid_argument("Invalid volume data file name.");
    }
    else
    {
        _fileName = fileName;
        QFuture<void> future = QtConcurrent::run(this, &MainWindow::setVolumeData, fileName);
        _watcher->setFuture(future);
        _timer.start(100);
    }
    return true;
}


/**
 * @brief MainWindow::readTff
 * @param fileName
 * @return
 */
void MainWindow::readTff(const QString &fileName)
{
    if (fileName.isEmpty())
    {
        throw std::invalid_argument("Invalid trtansfer function file name.");
    }
    else
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open transfer function file "
                                        + fileName.toStdString());
        }
        else
        {
            QTextStream in(&file);
            QGradientStops stops;
            while (!in.atEnd())
            {
                QStringList line = in.readLine().split(QRegExp("\\s"));
                if (line.size() < 5)
                    continue;
                QGradientStop stop(line.at(0).toDouble(),
                                   QColor(line.at(1).toInt(), line.at(2).toInt(),
                                          line.at(3).toInt(), line.at(4).toInt()));
                stops.push_back(stop);
            }
            if (!stops.isEmpty())
            {
                ui->transferFunctionEditor->getEditor()->setGradientStops(stops);
                ui->transferFunctionEditor->getEditor()->pointsUpdated();
            }
            else
                qCritical() << "Empty transfer function file.";
            file.close();
        }
    }
}


/**
 * @brief MainWindow::saveTff
 * @param fileName
 */
void MainWindow::saveTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastTffFile" ).toString();
    QString pickedFile = dia.getSaveFileName(
                this, tr("Save Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));

    if (!pickedFile.isEmpty())
    {
        QFile file(pickedFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open file "
                                        + pickedFile.toStdString());
        }
        else
        {
            QTextStream out(&file);
            const QGradientStops stops =
                    ui->transferFunctionEditor->getEditor()->getGradientStops();
            foreach (QGradientStop s, stops)
            {
                out << s.first << " " << s.second.red() << " " << s.second.green()
                    << " " << s.second.blue() << " " << s.second.alpha() << "\n";
            }
            file.close();
        }
    }
}

/**
 * @brief MainWindow::saveRawTff
 */
void MainWindow::saveRawTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastTffFile" ).toString();
    QString pickedFile = dia.getSaveFileName(
                this, tr("Save Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));

    if (!pickedFile.isEmpty())
    {
        QFile file(pickedFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open file "
                                        + pickedFile.toStdString());
        }
        else
        {
            QTextStream out(&file);
            const QGradientStops stops =
                    ui->transferFunctionEditor->getEditor()->getGradientStops();
            const std::vector<unsigned char> tff = ui->volumeRenderWidget->getRawTransferFunction(stops);
            foreach (unsigned char c, tff)
            {
                out << (int)c << " ";
            }
            file.close();
        }
    }
}


/**
 * @brief MainWindow::getStatus
 * @return
 */
void MainWindow::setStatusText()
{
    QString status = "No data loaded yet.";
    if (ui->volumeRenderWidget->hasData())
    {
        status = "File: ";
        status += _fileName;
        status += " | Volume: ";
        status += QString::number(ui->volumeRenderWidget->getVolumeResolution().x());
        status += "x";
        status += QString::number(ui->volumeRenderWidget->getVolumeResolution().y());
        status += "x";
        status += QString::number(ui->volumeRenderWidget->getVolumeResolution().z());
        status += " | Frame: ";
        status += QString::number(ui->volumeRenderWidget->size().width());
        status += "x";
        status += QString::number(ui->volumeRenderWidget->size().height());
        status += " ";
    }
    _statusLabel.setText(status);
}


/**
 * @brief MainWindow::finishedLoading
 */
void MainWindow::finishedLoading()
{
    _progBar.setValue(100);
    _progBar.hide();
    _timer.stop();
	qDebug() << "finished.";
    this->setStatusText();

//    const QVector3D volRes = ui->volumeRenderWidget->getVolumeResolution();

    ui->volumeRenderWidget->setLoadingFinished(true);
    ui->volumeRenderWidget->updateView();
}


/**
 * @brief MainWindow::addProgress
 */
void MainWindow::addProgress()
{
    if (_progBar.value() < _progBar.maximum() - 5)
    {
        _progBar.setValue(_progBar.value() + 1);
    }
}


/**
 * @brief MainWindow::loadTff
 */
void MainWindow::loadTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastTffFile" ).toString();
    QString pickedFile = dia.getOpenFileName(
                this, tr("Open Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));
    if (!pickedFile.isEmpty())
    {
        readTff(pickedFile);
        _settings->setValue( "LastTffFile", pickedFile );
    }
}


/**
 * @brief MainWindow::openVolumeFile
 */
void MainWindow::openVolumeFile()
{
	std::vector<DataConfig> configs;

    QFileDialog dia;
    QString defaultPath = _settings->value( "LastVolumeFile" ).toString();
    QString pickedFile = dia.getOpenFileName(
                this, tr("Open Volume Data"), defaultPath, tr("Volume data files (*.dat);;XML files (*.xml);; JSON files (*.json)"));
    if (!pickedFile.isEmpty())
    {
		if (pickedFile.contains(".xml", Qt::CaseInsensitive)) {
			_getSetData.getXMLConfig(pickedFile.toUtf8().constData(), _cfg, configs);
			setUI();
			randomData();
		}
		else if (pickedFile.contains(".json", Qt::CaseInsensitive)) {
			_getSetData.getJSONConfig(pickedFile.toUtf8().constData(), _cfg, configs);
			setUI();
			randomData();
		}
        else if (!readVolumeFile(pickedFile))
        {
            QMessageBox msgBox;
            msgBox.setIcon( QMessageBox::Critical );
            msgBox.setText( "Error while trying to create OpenCL memory objects." );
            msgBox.exec();
        }
        else
        {
            _settings->setValue( "LastVolumeFile", pickedFile );
        }
    }
}


/**
* @brief MainWindow::randomData
*/
void MainWindow::randomData() {
	// generate volume
	std::cout << "Generating volume data... " << std::endl;
	_dataGenerator.generateVolume(_cfg);
	std::cout << "done." << std::endl;

	// generate scalar data
	std::cout << "Generating scalar data... " << std::endl;
	_dataGenerator.generateScalarData(_cfg);
	std::cout << "done." << std::endl;

	ui->volumeRenderWidget->setRandomVolumeData(_cfg, _dataGenerator.data());

	_progBar.setFormat("Generating volume data...");
	_progBar.setValue(1);
	_progBar.show();
	ui->statusBar->addPermanentWidget(&_progBar, 2);
	ui->statusBar->updateGeometry();
	QApplication::processEvents();

	// MainWindow::finishedLoading
	////////////////////////////////////////////
	_progBar.setValue(100);
	_progBar.hide();
	_timer.stop();
	qDebug() << "finished.";
	////////////////////////////////////////////

	// MainWindow::setStatusText
	////////////////////////////////////////////////////////////////////////////////////////
	QString status = "No data loaded yet.";
	if (ui->volumeRenderWidget->hasData())
	{
		status = "Volume data successful generated.";
		status += " | Volume: ";
		status += QString::number(ui->volumeRenderWidget->getVolumeResolution().x());
		status += "x";
		status += QString::number(ui->volumeRenderWidget->getVolumeResolution().y());
		status += "x";
		status += QString::number(ui->volumeRenderWidget->getVolumeResolution().z());
		status += " | Frame: ";
		status += QString::number(ui->volumeRenderWidget->size().width());
		status += "x";
		status += QString::number(ui->volumeRenderWidget->size().height());
		status += " ";
	}
	_statusLabel.setText(status);
	////////////////////////////////////////////////////////////////////////////////////////

	ui->volumeRenderWidget->setLoadingFinished(true);
	ui->volumeRenderWidget->updateView();
}


/**
* @brief MainWindow::setConfig
*/
void MainWindow::setConfig() {
	if (ui->cbPrecision->currentText().compare("UCHAR") == 0) _cfg.precision = 0;
	else if (ui->cbPrecision->currentText().compare("USHORT") == 0) _cfg.precision = 1;
	else if (ui->cbPrecision->currentText().compare("FLOAT") == 0) _cfg.precision = 2;
	else if (ui->cbPrecision->currentText().compare("DOUBLE") == 0) _cfg.precision = 3;
	else _cfg.precision = 0;
	_cfg.res = { ui->lEResX->text().toUInt(), ui->lEResY->text().toUInt(), ui->lEResZ->text().toUInt() };
	_cfg.coverage = { ui->lECovX->text().toDouble(), ui->lECovY->text().toDouble(), ui->lECovZ->text().toDouble() };
	if (ui->cbShape->currentText().compare("Cube") == 0) _cfg.shape = cube;
	else if (ui->cbShape->currentText().compare("Sphere") == 0) _cfg.shape = sphere;
	else _cfg.shape = cube;
	_cfg.numBodies = ui->sbnumBodies->value();
	_cfg.dimBodies = { ui->lEdimX->text().toUInt(), ui->lEdimY->text().toUInt(), ui->lEdimZ->text().toUInt() };
	if (ui->cbLayout->currentText().compare("Structured") == 0) _cfg.randomBodyLayout = 0;
	else if (ui->cbLayout->currentText().compare("Pseudorandom") == 0) _cfg.randomBodyLayout = 1;
	else if (ui->cbLayout->currentText().compare("Halton") == 0) _cfg.randomBodyLayout = 2;
	else if (ui->cbLayout->currentText().compare("Perlin") == 0) _cfg.randomBodyLayout = 3;
	else _cfg.randomBodyLayout = 0;
	_cfg.frequency = { ui->lEFreqX->text().toUInt(), ui->lEFreqY->text().toUInt(), ui->lEFreqZ->text().toUInt() };
	_cfg.magnitude = { ui->lEMagX->text().toDouble(), ui->lEMagY->text().toDouble(), ui->lEMagZ->text().toDouble() };
	_cfg.slice_thickness = { ui->lESliceX->text().toDouble(), ui->lESliceY->text().toDouble(), ui->lESliceZ->text().toDouble() };
}


/**
* @brief MainWindow::setUI
*/
void MainWindow::setUI() {
	ui->cbPrecision->setCurrentIndex(_cfg.precision);
	ui->lEResX->setText(QString::number(_cfg.res.x));
	ui->lEResY->setText(QString::number(_cfg.res.y));
	ui->lEResZ->setText(QString::number(_cfg.res.z));
	ui->lECovX->setText(QString::number(_cfg.coverage.x));
	ui->lECovY->setText(QString::number(_cfg.coverage.y));
	ui->lECovZ->setText(QString::number(_cfg.coverage.z));
	if (_cfg.shape == cube) ui->cbShape->setCurrentIndex(0);
	else if (_cfg.shape == sphere) ui->cbShape->setCurrentIndex(1);
	ui->sbnumBodies->setValue(_cfg.numBodies);
	ui->lEdimX->setText(QString::number(_cfg.dimBodies.x));
	ui->lEdimY->setText(QString::number(_cfg.dimBodies.y));
	ui->lEdimZ->setText(QString::number(_cfg.dimBodies.z));
	ui->cbLayout->setCurrentIndex(_cfg.randomBodyLayout);
	ui->lEFreqX->setText(QString::number(_cfg.frequency.x));
	ui->lEFreqY->setText(QString::number(_cfg.frequency.y));
	ui->lEFreqZ->setText(QString::number(_cfg.frequency.z));
	ui->lEMagX->setText(QString::number(_cfg.magnitude.x));
	ui->lEMagY->setText(QString::number(_cfg.magnitude.y));
	ui->lEMagZ->setText(QString::number(_cfg.magnitude.z));
	ui->lESliceX->setText(QString::number(_cfg.slice_thickness.x));
	ui->lESliceY->setText(QString::number(_cfg.slice_thickness.y));
	ui->lESliceZ->setText(QString::number(_cfg.slice_thickness.z));
}

/**
 * @brief MainWindow::dragEnterEvent
 * @param ev
 */
void MainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasUrls())
    {
        bool valid = false;
        foreach(QUrl url, ev->mimeData()->urls())
        {
            if (!url.fileName().isEmpty())
            {
                QString sFileEnding = url.fileName().split(".", QString::SkipEmptyParts).at(1);
                if (sFileEnding == "dat")
                {
                    valid = true;
                }
            }
        }
        if (valid)
        {
            ev->acceptProposedAction();
        }
    }
}


/**
 * @brief MainWindow::dropEvent
 * @param ev
 */
void MainWindow::dropEvent(QDropEvent *ev)
{
    foreach(QUrl url, ev->mimeData()->urls())
    {
        QString sFileEnding = url.fileName().split(".", QString::SkipEmptyParts).at(1);
        if (sFileEnding == "dat")
        {
            // extract path and remove leading '/'
            QString fileName = url.path(); //.remove( 0, 1 );
            qDebug() << "Loading volume data file" << fileName;
            readVolumeFile(fileName);
        }
    }
}


/**
 * @brief MainWindow::chooseBackgroundColor
 */
void MainWindow::chooseBackgroundColor()
{
    QColorDialog dia;
    QColor col = dia.getColor();
    if (col.isValid())
        ui->volumeRenderWidget->setBackgroundColor(col);
}


