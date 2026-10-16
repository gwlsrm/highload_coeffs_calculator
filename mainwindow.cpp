#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>

#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>

#include <stdexcept>

QT_CHARTS_USE_NAMESPACE

namespace {
constexpr int kColumnCount = 3;
constexpr int kMinimumRowCount = 2;
const QString HEADER = "load,peak_cps,dpeak_cps";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // table header
    retranslateTableHeaders();
    ui->tableWidget->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    // graph (chart)
    createChart();

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveTableToFile);
    connect(ui->actionLoad, &QAction::triggered, this, &MainWindow::loadTableFromFile);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(ui->btnCalculate, &QPushButton::clicked, this, &MainWindow::calculateCoefficients);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::clearTable);
    connect(ui->tableWidget, &QTableWidget::cellClicked, this, &MainWindow::addNewRow);

    // dynamic language
    // connect(ui->actionEnglish, &QAction::triggered, this, [this] { setLanguage("en"); });
    // connect(ui->actionRussian, &QAction::triggered, this, [this] { setLanguage("ru"); });
    // setLanguage("en");
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ********* dynamic language loading ***********
// void MainWindow::changeEvent(QEvent* event)
// {
//     QMainWindow::changeEvent(event);

//     if (event->type() == QEvent::LanguageChange) {
//         ui->retranslateUi(this);
//     }
// }

// void MainWindow::setLanguage(const QString& languageCode)
// {
//     qApp->removeTranslator(&translator);

//     if (translator.load("app_" + languageCode,
//                         QCoreApplication::applicationDirPath() + "/translations")) {
//         qApp->installTranslator(&translator);
//     }

//     // Update explicitly: table headers are assigned in code rather than by Designer.
//     ui->retranslateUi(this);
//     retranslateTableHeaders();
//     ui->tableWidget->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
// }

void MainWindow::retranslateTableHeaders()
{
    ui->tableWidget->setHorizontalHeaderLabels(
        {tr("load"), tr("peak count rate"), tr("peak cr unc., %")});
}

void MainWindow::createChart() {
    scatter_series_ = new QScatterSeries;
    scatter_series_->setName(tr("Points"));
    scatter_series_->setMarkerSize(9);
    line_series_ = new QLineSeries;
    line_series_->setName("y = ax + b");

    chart_ = new QChart;
    chart_->addSeries(line_series_);
    chart_->addSeries(scatter_series_);
    chart_->createDefaultAxes();

    auto *axisX = qobject_cast<QValueAxis*>(chart_->axes(Qt::Horizontal).first());
    auto *axisY = qobject_cast<QValueAxis*>(chart_->axes(Qt::Vertical).first());
    axisX->setTitleText(tr("load, cps"));
    axisY->setTitleText(tr("peak cr"));

    auto *view = new QChartView(chart_);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumSize(400, 300);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    ui->verticalLayout_3->addWidget(view);
}

void MainWindow::clearTable() {
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(kMinimumRowCount);
}

void MainWindow::addNewRow(int row, [[__maybe_unused__]]int column) {
    if (row + 1 == ui->tableWidget->rowCount()) {
        ui->tableWidget->insertRow(ui->tableWidget->rowCount());
        ui->tableWidget->setItem(ui->tableWidget->rowCount() - 1, 0, new QTableWidgetItem());
        ui->tableWidget->setItem(ui->tableWidget->rowCount() - 1, 1, new QTableWidgetItem());
        ui->tableWidget->setItem(ui->tableWidget->rowCount() - 1, 2, new QTableWidgetItem());
    }
}

void MainWindow::saveTableToFile()
{
    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save records"), QString(), tr("CSV files (*.csv);;All files (*)"));
    if (filename.isEmpty())
        return;

    try {
        saveDataToFile(readTable(), filename.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Save error"), QString::fromStdString(error.what()));
    }
}

void MainWindow::loadTableFromFile()
{
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load records"), QString(), tr("CSV files (*.csv);;All files (*)"));
    if (filename.isEmpty())
        return;

    try {
        writeTable(loadDataFromFile(filename.toStdString()));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Load error"), QString::fromStdString(error.what()));
    }
}

void MainWindow::calculateCoefficients()
{
    try {
        const Matrix tableMatrix = readTable();
        if (tableMatrix.size() < kMinimumRowCount) {
            throw std::invalid_argument("At least two records are required for calculation.");
        }

        Matrix calculationMatrix;
        calculationMatrix.reserve(tableMatrix.size());
        for (const std::vector<double>& row : tableMatrix) {
            calculationMatrix.push_back({row[0], row[1], row[2]});
        }

        const CalculationResults results = calculateHighLoadCoeffs(calculationMatrix);
        ui->edtAValue->setText(QString::number(results.coeff, 'e', 3));
        ui->edtAUncValue->setText(QString::number(results.d_coeff / results.coeff * 100.0, 'g', 3));
        plotChart(calculationMatrix, results.coeff, results.cps0);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, tr("Calculation error"), QString::fromStdString(error.what()));
    }
}

Matrix MainWindow::readTable() const
{
    Matrix matrix;
    matrix.reserve(ui->tableWidget->rowCount());

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        std::vector<double> values;
        values.reserve(kColumnCount);

        bool is_emtpy_row = true;
        for (int column = 0; column < kColumnCount; ++column) {
            const QTableWidgetItem* item = ui->tableWidget->item(row, column);
            const QString text = item == nullptr ? QString() : item->text().trimmed();
            if (text.isEmpty()) continue;
            is_emtpy_row = false;
            bool isNumber = false;
            const double value = text.toDouble(&isNumber);
            if (!isNumber)
                throw std::invalid_argument(
                    QString("Cell (%1, %2) must contain a number.").arg(row + 1).arg(column + 1).toStdString());
            values.push_back(value);
        }
        if (is_emtpy_row) break;
        matrix.push_back(values);
    }

    return matrix;
}

void MainWindow::writeTable(const Matrix& matrix)
{
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(static_cast<int>(matrix.size()));
    ui->tableWidget->setColumnCount(kColumnCount);

    for (int row = 0; row < static_cast<int>(matrix.size()); ++row) {
        if (matrix[row].size() != kColumnCount)
            throw std::invalid_argument("Every record in the file must have exactly three values.");
        for (int column = 0; column < kColumnCount; ++column)
            ui->tableWidget->setItem(row, column,
                                     new QTableWidgetItem(QString::number(matrix[row][column], 'g', 17)));
    }
}

void MainWindow::saveDataToFile(const Matrix& matrix, const std::string& filename) const
{
    QFile file(QString::fromStdString(filename));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw std::runtime_error("Cannot open the selected file for writing.");

    QTextStream stream(&file);
    stream << HEADER << "\n";
    for (const std::vector<double>& row : matrix) {
        if (row.size() != kColumnCount)
            throw std::invalid_argument("Every table row must have exactly three values.");
        stream << QString::number(row[0], 'g', 17) << ','
               << QString::number(row[1], 'g', 17) << ','
               << QString::number(row[2], 'g', 17) << '\n';
    }
}

Matrix MainWindow::loadDataFromFile(const std::string& filename) const
{
    QFile file(QString::fromStdString(filename));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw std::runtime_error("Cannot open the selected file for reading.");

    Matrix matrix;
    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        if (lineNumber == 1 && line == HEADER)
            continue;

        const QStringList fields = line.split(',');
        if (fields.size() != kColumnCount) {
            throw std::invalid_argument("Line " + std::to_string(lineNumber)
                                        + " must contain exactly three comma-separated values.");
        }

        std::vector<double> values;
        values.reserve(kColumnCount);
        for (const QString& field : fields) {
            bool isNumber = false;
            const double value = field.trimmed().toDouble(&isNumber);
            if (!isNumber) {
                throw std::invalid_argument("Line " + std::to_string(lineNumber)
                                            + " contains a non-numeric value.");
            }
            values.push_back(value);
        }
        if (!values.empty())
            matrix.push_back(values);
    }

    if (matrix.empty())
        throw std::invalid_argument("The selected file does not contain any records.");

    return matrix;
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(this, tr("Highload coefficient calculator"),
        tr("Highload coefficient calculator calculates 'A' coefficient for the live-time correction "\
            "of highloaded spectra in LSRM SpectraLine"));
}

void MainWindow::plotChart(const Matrix& matrix, double a_coeff, double cr_at_0) {
    if (matrix.empty()) {
        QMessageBox::critical(this, tr("Plot points"), tr("Empty input data"));
        return;
    }
    if (matrix[0].size() != 3) {
        QMessageBox::critical(this, tr("Plot points"), tr("Invalid input data"));
        return;
    }

    // fill points vector
    QVector<QPointF> points;
    points.reserve(matrix.size());
    for (const auto &row : matrix) {
        points.push_back(QPointF(row[0], row[1]));
    }

    // x range using points
    double xmin = 0;
    double xmax = points.first().x();
    for (const auto &p : points) {
        xmax = std::max(xmax, p.x());
    }
    const double xMargin = (xmax > xmin) ? (xmax - xmin) * 0.01 : 1.0;
    xmax += xMargin;

    // add points and line
    for (const auto &p : points) {
        scatter_series_->append(p);
    }
    double y_line_max = corrected_cps(cr_at_0, a_coeff, xmax);
    line_series_->append(0, cr_at_0);
    line_series_->append(xmax, y_line_max);

    // y range using points and line
    double ymin = std::min(cr_at_0, y_line_max);
    double ymax = std::max(cr_at_0, y_line_max);
    for (const auto &p : points) {
        ymin = std::min(ymin, p.y());
        ymax = std::max(ymax, p.y());
    }
    const double yMargin = (ymax > ymin) ? (ymax - ymin) * 0.05 : 1.0;

    // set axis range
    auto *axisX = qobject_cast<QValueAxis*>(chart_->axes(Qt::Horizontal).first());
    auto *axisY = qobject_cast<QValueAxis *>(chart_->axes(Qt::Vertical).first());
    axisX->setRange(xmin, xmax);
    axisY->setRange(ymin - yMargin, ymax + yMargin);
}
