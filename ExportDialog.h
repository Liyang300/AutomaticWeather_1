// ExportDialog.h
#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QDate>
#include <QProgressBar>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);

    QDate startDate() const;
    QDate endDate() const;
    QString filePath() const;

signals:
    void exportRequested(const QDate& start, const QDate& end, const QString& filePath);

public slots:
    void onExportProgress(int percentage);
    void onExportFinished(bool success, const QString& message);

private slots:
    void onBrowseClicked();
    void onExportClicked();
    void onCancelClicked();
    void closeEvent(QCloseEvent *event);

private:
    QComboBox* m_startDateCombo;
    QComboBox* m_endDateCombo;
    QLineEdit* m_filePathEdit;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_exportButton;
    QPushButton* m_cancelButton;

    void setupUI();
    void populateDateComboBoxes();
};

#endif // EXPORTDIALOG_H
