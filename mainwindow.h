#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTranslator>

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>

#include <string>
#include <vector>

#include "highload_coeffs_calculator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void clearTable();
    void saveTableToFile();
    void loadTableFromFile();
    void calculateCoefficients();
    void addNewRow(int row, int column);
    void showAboutDialog();
    // void setLanguage(const QString& languageCode);

// protected:
//     void changeEvent(QEvent* event) override;

private:
    Ui::MainWindow *ui;
    QTranslator translator;
    std::string cur_langugage_ = "en";
    QScatterSeries* scatter_series_ = nullptr;
    QLineSeries* line_series_ = nullptr;
    QChart* chart_ = nullptr;

    void retranslateTableHeaders();
    Matrix readTable() const;
    void writeTable(const Matrix& matrix);
    void saveDataToFile(const Matrix& matrix, const std::string& filename) const;
    Matrix loadDataFromFile(const std::string& filename) const;
    void createChart();
    void plotChart(const Matrix& matrix, double a_coeff, double cr_at_0);
};
#endif // MAINWINDOW_H
