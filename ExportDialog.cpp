// ExportDialog.cpp
#include "ExportDialog.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDebug>
#include <QLineEdit>

ExportDialog::ExportDialog(QWidget *parent)
    : QDialog(parent)
    , m_startDateCombo(new QComboBox(this))
    , m_endDateCombo(new QComboBox(this))
    , m_filePathEdit(new QLineEdit(this))
    , m_progressBar(new QProgressBar(this))
    , m_statusLabel(new QLabel("准备导出...", this))
    , m_exportButton(new QPushButton("导出", this))
    , m_cancelButton(new QPushButton("取消", this))
{
    setWindowTitle("导出数据");
    setMinimumSize(400, 300);

    setupUI();
    populateDateComboBoxes();

    // 设置默认文件路径
    QString defaultPath = QDir::currentPath() + "/weather_export_" +
                          QDate::currentDate().toString("yyyyMMdd") + ".csv";
    m_filePathEdit->setText(defaultPath);
}

void ExportDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 表单布局
    QFormLayout* formLayout = new QFormLayout();

    QPushButton* browseButton = new QPushButton("浏览...", this);
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);

    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(browseButton);

    formLayout->addRow("开始日期:", m_startDateCombo);
    formLayout->addRow("结束日期:", m_endDateCombo);
    formLayout->addRow("保存路径:", fileLayout);

    mainLayout->addLayout(formLayout);

    // 进度条
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // 状态标签
    mainLayout->addWidget(m_statusLabel);

    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(m_exportButton, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ExportDialog::onCancelClicked);
}

void ExportDialog::populateDateComboBoxes()
{
    QVector<QDate> dates = DatabaseManager::instance().getAvailableDates();

    // 按升序排序（从早到晚）
    std::sort(dates.begin(), dates.end());

    m_startDateCombo->clear();
    m_endDateCombo->clear();

    for (const QDate& date : std::as_const(dates))
    {  // 或者 for (const QDate& date : dates) 但用qAsConst更安全
        QString dateStr = date.toString("yyyy-MM-dd");
        m_startDateCombo->addItem(dateStr, date);
        m_endDateCombo->addItem(dateStr, date);
    }

    // 默认选择最近30天的数据
    if (!dates.isEmpty()) {
        int total = dates.size();
        int startIndex = qMax(0, total - 30);   // 从倒数第30天开始
        int endIndex = total - 1;               // 最新一天
        m_startDateCombo->setCurrentIndex(startIndex);
        m_endDateCombo->setCurrentIndex(endIndex);
    }
}

QDate ExportDialog::startDate() const
{
    return m_startDateCombo->currentData().toDate();
}

QDate ExportDialog::endDate() const
{
    return m_endDateCombo->currentData().toDate();
}

QString ExportDialog::filePath() const
{
    return m_filePathEdit->text();
}

void ExportDialog::onBrowseClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存文件",
        QDir::currentPath(),
        "CSV文件 (*.csv);;所有文件 (*.*)"
        );

    if (!fileName.isEmpty()) {
        m_filePathEdit->setText(fileName);
    }
}

void ExportDialog::onExportClicked()
{
    QDate start = startDate();
    QDate end = endDate();
    QString filePath = this->filePath();

    if (!start.isValid() || !end.isValid()) {
        QMessageBox::warning(this, "错误", "请选择有效的日期范围");
        return;
    }

    if (start > end) {
        QMessageBox::warning(this, "错误", "开始日期不能晚于结束日期");
        return;
    }

    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择保存路径");
        return;
    }

    m_exportButton->setEnabled(false);
    m_statusLabel->setText("正在导出数据...");

    emit exportRequested(start, end, filePath);
}

void ExportDialog::onCancelClicked()
{
    reject();
}

void ExportDialog::onExportProgress(int percentage)
{
    m_progressBar->setValue(percentage);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    m_exportButton->setEnabled(true);

    if (success) {
        m_statusLabel->setText("导出完成!");
        QMessageBox::information(this, "成功", message);
        accept();
    } else {
        m_statusLabel->setText("导出失败!");
        QMessageBox::critical(this, "错误", message);
    }
}
