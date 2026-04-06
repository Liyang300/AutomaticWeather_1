#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QFutureWatcher>
#include <QVector>
#include <QDate>

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(QWidget *parent = nullptr);
    ~ExportDialog();

    QDate startDate() const;
    QDate endDate() const;
    QString filePath() const;

signals:
    void exportRequested(const QDate& start, const QDate& end, const QString& path);

public slots:
    void onExportProgress(int percentage);
    void onExportFinished(bool success, const QString& message);

private slots:
    void onBrowseClicked();
    void onExportClicked();
    void onCancelClicked();
    void onDatesLoaded();
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();

    QComboBox* m_startDateCombo;
    QComboBox* m_endDateCombo;
    QLineEdit* m_filePathEdit;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_exportButton;
    QPushButton* m_cancelButton;

    QFutureWatcher<QVector<QDate>>* m_dateWatcher;
    bool m_datesLoaded;
    bool m_closing;      // 关闭标志，防止关闭后更新控件
};

#endif // EXPORTDIALOG_H
