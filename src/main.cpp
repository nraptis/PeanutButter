#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStackedLayout>
#include <QString>
#include <QToolButton>
#include <QThread>
#include <QUrl>
#include <QWidget>

#include <filesystem>
#include <fstream>
#include <array>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <functional>
#include <vector>

#include "ConfigLoader.hpp"
#include "FileCompare.hpp"
#include "Globals.hpp"
#include "IO/FileReaderImplementation.hpp"
#include "PeanutButterDelegate.hpp"
#include "PeanutButterImplementation.hpp"
#include "SnowStorm/SnowStormEngine.hpp"

namespace fs = std::filesystem;

namespace {

#if !defined(NDEBUG)
inline constexpr bool kInferRelativePathsFromCwd = true;
#else
inline constexpr bool kInferRelativePathsFromCwd = false;
#endif

fs::path inferredBaseDirectory() {
  if (kInferRelativePathsFromCwd) {
    return fs::current_path();
  }
  return fs::path(QCoreApplication::applicationDirPath().toStdString());
}

fs::path resolveConfigPath() {
  const fs::path aBase = inferredBaseDirectory();
  const fs::path aPrimary = aBase / "config.json";
  if (fs::exists(aPrimary)) {
    return aPrimary;
  }
  const fs::path aFallback = aBase / "PeanutButter" / "config.json";
  if (fs::exists(aFallback)) {
    return aFallback;
  }
  return aPrimary;
}

fs::path resolveUserPath(const QString& pText) {
  const fs::path aRaw = fs::path(pText.trimmed().toStdString());
  if (aRaw.empty()) {
    return aRaw;
  }
  if (aRaw.is_absolute()) {
    return aRaw;
  }
  return (inferredBaseDirectory() / aRaw).lexically_normal();
}

class QtPeanutButterDelegate final : public PeanutButterDelegate {
public:
  QtPeanutButterDelegate(QWidget* pWindow,
                         std::function<void(const QString&)> pAppendLog,
                         std::function<void()> pOnWorkStart)
      : mWindow(pWindow),
        mAppendLog(std::move(pAppendLog)),
        mOnWorkStart(std::move(pOnWorkStart)) {}

  void logMessage(const std::string& pMessage) override {
    const QString aLine = QString::fromStdString(pMessage);
    if (!mWorkStarted && pMessage.find("started.") != std::string::npos) {
      mWorkStarted = true;
      if (mOnWorkStart) {
        const auto aOnWorkStart = mOnWorkStart;
        if (QThread::currentThread() == mWindow->thread()) {
          aOnWorkStart();
        } else {
          QMetaObject::invokeMethod(mWindow, [aOnWorkStart]() { aOnWorkStart(); }, Qt::QueuedConnection);
        }
      }
    }
    const auto aAppendLog = mAppendLog;
    if (QThread::currentThread() == mWindow->thread()) {
      aAppendLog(aLine);
      return;
    }
    QMetaObject::invokeMethod(
        mWindow,
        [aAppendLog, aLine]() { aAppendLog(aLine); },
        Qt::QueuedConnection);
  }

  bool showOkCancelDialog(const std::string& pTitle, const std::string& pMessage) override {
    bool aAccepted = false;
    auto aDialogLambda = [&]() {
      const auto aDecision = QMessageBox::question(
          mWindow,
          QString::fromStdString(pTitle),
          QString::fromStdString(pMessage),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel);
      aAccepted = (aDecision == QMessageBox::Ok);
    };
    if (QThread::currentThread() == mWindow->thread()) {
      aDialogLambda();
    } else {
      QMetaObject::invokeMethod(mWindow, aDialogLambda, Qt::BlockingQueuedConnection);
    }
    return aAccepted;
  }

private:
  QWidget* mWindow = nullptr;
  std::function<void(const QString&)> mAppendLog;
  std::function<void()> mOnWorkStart;
  bool mWorkStarted = false;
};

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  if ((gArchiveSize % BLOCK_SIZE_LAYER_3) != 0) {
    QMessageBox::critical(nullptr,
                          "snowstorm",
                          QString("FATAL: gArchiveSize (%1) must be an exact multiple of BLOCK_SIZE_LAYER_3.")
                              .arg(QString::number(static_cast<qulonglong>(gArchiveSize))));
    return 1;
  }

  QWidget window;
  window.setWindowTitle("peanut butter");
  window.setMinimumSize(570, 572);

  auto* layout = new QGridLayout(&window);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);
  layout->setColumnStretch(0, 0);
  layout->setColumnStretch(1, 1);
  layout->setColumnStretch(2, 1);
  layout->setColumnStretch(3, 1);
  layout->setColumnStretch(4, 1);
  layout->setColumnStretch(5, 1);
  layout->setColumnStretch(6, 1);
  layout->setColumnStretch(7, 0);
  auto* source_edit = new QLineEdit(&window);
  auto* archive_edit = new QLineEdit(&window);
  auto* unarchive_edit = new QLineEdit(&window);
  auto* recovery_edit = new QLineEdit(&window);
  auto* file_prefix_edit = new QLineEdit(&window);
  auto* file_suffix_edit = new QLineEdit(&window);
  auto* password1_edit = new QLineEdit(&window);
  auto* password2_edit = new QLineEdit(&window);
  auto* archive_size_combo = new QComboBox(&window);
  auto* clear_logs_button = new QPushButton("Clear Logs", &window);
  auto* source_clear_button = new QToolButton(&window);
  auto* source_pick_button = new QToolButton(&window);
  auto* archive_clear_button = new QToolButton(&window);
  auto* archive_pick_button = new QToolButton(&window);
  auto* unarchive_clear_button = new QToolButton(&window);
  auto* unarchive_pick_button = new QToolButton(&window);
  auto* recovery_clear_button = new QToolButton(&window);
  auto* recovery_pick_button = new QToolButton(&window);
  auto* divider1 = new QFrame(&window);
  auto* divider_inputs = new QFrame(&window);
  auto* divider_passwords = new QFrame(&window);
  auto* divider2 = new QFrame(&window);
  auto* spacer_before_actions = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* pack_button = new QPushButton("Bundle", &window);
  auto* unpack_button = new QPushButton("Unbundle", &window);
  auto* sanity_button = new QPushButton("Sanity", &window);
  auto* recover_button = new QPushButton("Recover", &window);
  auto* action_spinner = new QProgressBar(&window);
  auto* action_row_host = new QWidget(&window);
  auto* action_buttons_row = new QWidget(action_row_host);
  auto* action_spinner_row = new QWidget(action_row_host);
  auto* action_row_stack = new QStackedLayout(action_row_host);
  auto* action_buttons_layout = new QHBoxLayout(action_buttons_row);
  auto* action_spinner_layout = new QHBoxLayout(action_spinner_row);
  auto* debug_console = new QPlainTextEdit(&window);

  source_edit->setPlaceholderText("pack source folder -> archive folder");
  archive_edit->setPlaceholderText("archive folder -> unarchive folder");
  unarchive_edit->setPlaceholderText("unarchive folder");
  recovery_edit->setPlaceholderText("recovery start file");
  file_prefix_edit->setPlaceholderText("file_prefix");
  file_suffix_edit->setPlaceholderText("file_suffix");
  password1_edit->setPlaceholderText("Password1");
  password2_edit->setPlaceholderText("Password2");
  archive_size_combo->setToolTip("Select archive size in bytes");
  password1_edit->setEchoMode(QLineEdit::Password);
  password2_edit->setEchoMode(QLineEdit::Password);
  source_edit->setTextMargins(0, 4, 0, 4);
  archive_edit->setTextMargins(0, 4, 0, 4);
  unarchive_edit->setTextMargins(0, 4, 0, 4);
  recovery_edit->setTextMargins(0, 4, 0, 4);
  file_prefix_edit->setTextMargins(0, 4, 0, 4);
  file_suffix_edit->setTextMargins(0, 4, 0, 4);
  password1_edit->setTextMargins(0, 4, 0, 4);
  password2_edit->setTextMargins(0, 4, 0, 4);

  source_clear_button->setText("X");
  archive_clear_button->setText("X");
  unarchive_clear_button->setText("X");
  recovery_clear_button->setText("X");
  source_pick_button->setText("Folder...");
  archive_pick_button->setText("Folder...");
  unarchive_pick_button->setText("Folder...");
  recovery_pick_button->setText("File...");

  source_clear_button->setToolTip("Clear pack source folder");
  archive_clear_button->setToolTip("Clear archive folder");
  unarchive_clear_button->setToolTip("Clear unarchive folder");
  recovery_clear_button->setToolTip("Clear recovery start file");
  source_pick_button->setToolTip("Pick pack source folder");
  archive_pick_button->setToolTip("Pick archive folder");
  unarchive_pick_button->setToolTip("Pick unarchive folder");
  recovery_pick_button->setToolTip("Pick recovery start file");

  constexpr int kInputHeight = 44;
  source_edit->setFixedHeight(kInputHeight);
  archive_edit->setFixedHeight(kInputHeight);
  unarchive_edit->setFixedHeight(kInputHeight);
  recovery_edit->setFixedHeight(kInputHeight);
  file_prefix_edit->setFixedHeight(kInputHeight);
  file_suffix_edit->setFixedHeight(kInputHeight);
  password1_edit->setFixedHeight(kInputHeight);
  password2_edit->setFixedHeight(kInputHeight);
  archive_size_combo->setFixedHeight(kInputHeight);
  source_clear_button->setFixedSize(54, kInputHeight);
  archive_clear_button->setFixedSize(54, kInputHeight);
  unarchive_clear_button->setFixedSize(54, kInputHeight);
  recovery_clear_button->setFixedSize(54, kInputHeight);
  source_pick_button->setFixedSize(72, kInputHeight);
  archive_pick_button->setFixedSize(72, kInputHeight);
  unarchive_pick_button->setFixedSize(72, kInputHeight);
  recovery_pick_button->setFixedSize(72, kInputHeight);
  clear_logs_button->setFixedHeight(kInputHeight);

  divider1->setFrameShape(QFrame::HLine);
  divider1->setFrameShadow(QFrame::Sunken);
  divider_inputs->setFrameShape(QFrame::HLine);
  divider_inputs->setFrameShadow(QFrame::Sunken);
  divider_passwords->setFrameShape(QFrame::HLine);
  divider_passwords->setFrameShadow(QFrame::Sunken);
  divider2->setFrameShape(QFrame::HLine);
  divider2->setFrameShadow(QFrame::Sunken);
  divider_inputs->setStyleSheet("margin-left: -12px; margin-right: -12px;");
  divider_passwords->setStyleSheet("margin-left: -12px; margin-right: -12px;");
  divider1->setStyleSheet("margin-left: -12px; margin-right: -12px;");
  divider2->setStyleSheet("margin-left: -12px; margin-right: -12px;");

  pack_button->setObjectName("packButton");
  unpack_button->setObjectName("unpackButton");
  sanity_button->setObjectName("sanityButton");
  recover_button->setObjectName("recoverButton");
  clear_logs_button->setObjectName("clearLogsButton");
  window.setStyleSheet(
      "QLineEdit { border-radius: 8px; font-size: 15px; font-weight: 700; padding: 8px 10px; background-color: #0f0f0f; color: #e8e8e8; }"
      "QComboBox { border-radius: 10px; font-size: 15px; font-weight: 700; padding: 8px 10px; background-color: #0f0f0f; color: #e8e8e8; }"
      "QComboBox::drop-down { border-top-right-radius: 10px; border-bottom-right-radius: 10px; width: 28px; }"
      "QComboBox::down-arrow { width: 0px; height: 0px; border-left: 6px solid transparent; border-right: 6px solid transparent; border-top: 8px solid #d8d8d8; margin-right: 8px; }"
      "QComboBox QAbstractItemView { border-radius: 10px; }"
      "QPushButton { border-radius: 8px; font-size: 14px; font-weight: 700; }"
      "QToolButton { border-radius: 8px; font-size: 13px; font-weight: 700; background-color: #000000; color: #ffffff; }"
      "QPlainTextEdit { border-radius: 8px; }"
      "QPushButton#packButton { background-color: #660000; color: #ffffff; }"
      "QPushButton#unpackButton { background-color: #00008b; color: #ffffff; }"
      "QPushButton#sanityButton { background-color: #ff8c00; color: #000000; }"
      "QPushButton#recoverButton { background-color: #800080; color: #ffffff; }"
      "QPushButton#clearLogsButton { background-color: #000000; color: #ffffff; border-radius: 10px; }");

  constexpr int kActionHeight = 54;
  pack_button->setFixedHeight(kActionHeight);
  unpack_button->setFixedHeight(kActionHeight);
  sanity_button->setFixedHeight(kActionHeight);
  recover_button->setFixedHeight(kActionHeight);
  pack_button->setMinimumHeight(kActionHeight);
  pack_button->setMaximumHeight(kActionHeight);
  unpack_button->setMinimumHeight(kActionHeight);
  unpack_button->setMaximumHeight(kActionHeight);
  sanity_button->setMinimumHeight(kActionHeight);
  sanity_button->setMaximumHeight(kActionHeight);
  recover_button->setMinimumHeight(kActionHeight);
  recover_button->setMaximumHeight(kActionHeight);
  pack_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  unpack_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  sanity_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  recover_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  action_spinner->setRange(0, 0);
  action_spinner->setTextVisible(false);
  action_spinner->setFixedHeight(kActionHeight);
  action_spinner->setMinimumHeight(kActionHeight);
  action_spinner->setMaximumHeight(kActionHeight);
  action_spinner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  action_row_stack->setContentsMargins(0, 0, 0, 0);
  action_row_stack->setSpacing(0);
  action_buttons_layout->setContentsMargins(0, 0, 0, 0);
  action_buttons_layout->setSpacing(0);
  action_buttons_layout->addWidget(pack_button, 1);
  action_buttons_layout->addSpacing(8);
  action_buttons_layout->addWidget(unpack_button, 1);
  action_buttons_layout->addSpacing(8);
  action_buttons_layout->addWidget(sanity_button, 1);
  action_buttons_layout->addSpacing(8);
  action_buttons_layout->addWidget(recover_button, 1);
  action_spinner_layout->setContentsMargins(0, 0, 0, 0);
  action_spinner_layout->setSpacing(0);
  action_spinner_layout->addWidget(action_spinner, 1);
  action_row_stack->addWidget(action_buttons_row);
  action_row_stack->addWidget(action_spinner_row);
  action_row_stack->setCurrentWidget(action_buttons_row);

  debug_console->setReadOnly(true);
  debug_console->setPlaceholderText("Debug console");
  debug_console->setStyleSheet(
      "background-color: #0a0a0a;"
      "color: #00ff41;"
      "font-family: Menlo, Monaco, monospace;"
      "padding: 4px;");

  auto append_log = [&](const QString& message) {
    debug_console->appendPlainText(message);
    debug_console->verticalScrollBar()->setValue(debug_console->verticalScrollBar()->maximum());
  };

  auto fallback_archive_size = [&]() {
    const std::uint64_t aTarget = BLOCK_SIZE_LAYER_3 * 200ULL;
    for (const ArchiveSizeOption& aOption : archive_size_options) {
      if (aOption.mBytes == aTarget) {
        return aOption.mBytes;
      }
    }
    if (!archive_size_options.empty()) {
      return archive_size_options.front().mBytes;
    }
    return BLOCK_SIZE_LAYER_3;
  };

  QJsonObject config_defaults;
  std::uint64_t configured_archive_size = BLOCK_SIZE_LAYER_3;
  try {
    const AppConfig aConfig = loadConfig(resolveConfigPath());
    config_defaults.insert("default_source_path", QString::fromStdString(aConfig.mDefaultSourcePath));
    config_defaults.insert("default_archive_path", QString::fromStdString(aConfig.mDefaultArchivePath));
    config_defaults.insert("default_unarchive_path", QString::fromStdString(aConfig.mDefaultUnarchivePath));
    config_defaults.insert("default_recovery_path", QString::fromStdString(aConfig.mDefaultRecoveryPath));
    config_defaults.insert("default_file_prefix", QString::fromStdString(aConfig.mDefaultFilePrefix));
    config_defaults.insert("default_file_suffix", QString::fromStdString(aConfig.mDefaultFileSuffix));
    config_defaults.insert("default_password_1", QString::fromStdString(aConfig.mDefaultPassword1));
    config_defaults.insert("default_password_2", QString::fromStdString(aConfig.mDefaultPassword2));
    configured_archive_size = aConfig.mDefaultArchiveSize;
  } catch (...) {
    QFile config_file("config.json");
    if (config_file.open(QIODevice::ReadOnly)) {
      const QJsonDocument config_doc = QJsonDocument::fromJson(config_file.readAll());
      if (config_doc.isObject()) {
        config_defaults = config_doc.object();
        const QJsonValue aArchiveSizeValue = config_defaults.value("default_archive_size");
        if (aArchiveSizeValue.isDouble()) {
          configured_archive_size = static_cast<std::uint64_t>(aArchiveSizeValue.toDouble());
        }
      }
    }
  }

  auto config_value = [&](const char* key, const char* fallback) {
    const auto v = config_defaults.value(QString::fromUtf8(key));
    return v.isString() ? v.toString() : QString::fromUtf8(fallback);
  };
  source_edit->setText(config_value("default_source_path", "input"));
  archive_edit->setText(config_value("default_archive_path", "archive"));
  unarchive_edit->setText(config_value("default_unarchive_path", "unzipped"));
  recovery_edit->setText(config_value("default_recovery_path", "folder.archive_0047.jag"));
  file_prefix_edit->setText(config_value("default_file_prefix", "snowstorm_"));
  file_suffix_edit->setText(config_value("default_file_suffix", ".jag"));
  password1_edit->setText(config_value("default_password_1", "banana"));
  password2_edit->setText(config_value("default_password_2", "apple"));

  int archive_default_index = 0;
  bool archive_size_found = false;
  for (std::size_t i = 0; i < archive_size_options.size(); ++i) {
    const ArchiveSizeOption& option = archive_size_options[i];
    archive_size_combo->addItem(QString::fromUtf8(option.mLabel),
                                QVariant::fromValue(static_cast<qulonglong>(option.mBytes)));
    if (option.mBytes == configured_archive_size) {
      archive_default_index = static_cast<int>(i);
      archive_size_found = true;
    }
  }
  if (!archive_size_found) {
    const std::uint64_t aFallback = fallback_archive_size();
    append_log(QString("Config event: default_archive_size=%1 is not in available sizes. Falling back to %2.")
                   .arg(QString::number(static_cast<qulonglong>(configured_archive_size)))
                   .arg(QString::number(static_cast<qulonglong>(aFallback))));
    for (std::size_t i = 0; i < archive_size_options.size(); ++i) {
      if (archive_size_options[i].mBytes == aFallback) {
        archive_default_index = static_cast<int>(i);
        archive_size_found = true;
        break;
      }
    }
  }
  if (!archive_size_found) {
    append_log("Config event: fallback archive size is missing from available sizes. Falling back to first option.");
  }
  archive_size_combo->setCurrentIndex(archive_default_index);

  struct FolderDropFilter : QObject {
    explicit FolderDropFilter(QLineEdit* target_edit, bool pAllowFiles = false)
        : edit(target_edit), allowFiles(pAllowFiles) {}
    QLineEdit* edit = nullptr;
    bool allowFiles = false;

    bool eventFilter(QObject* watched, QEvent* event) override {
      if (watched != edit) {
        return QObject::eventFilter(watched, event);
      }
      if (event->type() == QEvent::DragEnter) {
        auto* drag = static_cast<QDragEnterEvent*>(event);
        const QMimeData* mime = drag->mimeData();
        if (mime != nullptr && mime->hasUrls()) {
          for (const QUrl& url : mime->urls()) {
            if (!url.isLocalFile()) {
              continue;
            }
            const fs::path p = url.toLocalFile().toStdString();
            if (fs::is_directory(p) || (allowFiles && fs::is_regular_file(p))) {
              drag->acceptProposedAction();
              return true;
            }
          }
        }
        return false;
      }
      if (event->type() == QEvent::Drop) {
        auto* drop = static_cast<QDropEvent*>(event);
        const QMimeData* mime = drop->mimeData();
        if (mime != nullptr && mime->hasUrls()) {
          for (const QUrl& url : mime->urls()) {
            if (!url.isLocalFile()) {
              continue;
            }
            const QString local = url.toLocalFile();
            const fs::path p = local.toStdString();
            if (fs::is_directory(p) || (allowFiles && fs::is_regular_file(p))) {
              edit->setText(local);
              drop->acceptProposedAction();
              return true;
            }
          }
        }
        return false;
      }
      return QObject::eventFilter(watched, event);
    }
  };

  source_edit->setAcceptDrops(true);
  archive_edit->setAcceptDrops(true);
  unarchive_edit->setAcceptDrops(true);
  recovery_edit->setAcceptDrops(true);
  auto* source_drop_filter = new FolderDropFilter(source_edit);
  auto* archive_drop_filter = new FolderDropFilter(archive_edit);
  auto* unarchive_drop_filter = new FolderDropFilter(unarchive_edit);
  auto* recovery_drop_filter = new FolderDropFilter(recovery_edit, true);
  source_drop_filter->setParent(&window);
  archive_drop_filter->setParent(&window);
  unarchive_drop_filter->setParent(&window);
  recovery_drop_filter->setParent(&window);
  source_edit->installEventFilter(source_drop_filter);
  archive_edit->installEventFilter(archive_drop_filter);
  unarchive_edit->installEventFilter(unarchive_drop_filter);
  recovery_edit->installEventFilter(recovery_drop_filter);

  auto compact_error = [](QString message) {
    message.replace('\r', ' ');
    if (message.size() > 420) {
      message = message.left(420) + " ... [truncated]";
    }
    return message;
  };


  layout->addWidget(source_clear_button, 0, 0);
  layout->addWidget(source_edit, 0, 1, 1, 6);
  layout->addWidget(source_pick_button, 0, 7);

  layout->addWidget(archive_clear_button, 1, 0);
  layout->addWidget(archive_edit, 1, 1, 1, 6);
  layout->addWidget(archive_pick_button, 1, 7);

  layout->addWidget(unarchive_clear_button, 2, 0);
  layout->addWidget(unarchive_edit, 2, 1, 1, 6);
  layout->addWidget(unarchive_pick_button, 2, 7);

  layout->addWidget(recovery_clear_button, 3, 0);
  layout->addWidget(recovery_edit, 3, 1, 1, 6);
  layout->addWidget(recovery_pick_button, 3, 7);

  layout->addWidget(divider_inputs, 4, 0, 1, 8);

  layout->addWidget(password1_edit, 5, 0, 1, 4);
  layout->addWidget(password2_edit, 5, 4, 1, 4);

  layout->addWidget(file_prefix_edit, 6, 0, 1, 4);
  layout->addWidget(file_suffix_edit, 6, 4, 1, 4);

  layout->addWidget(divider_passwords, 7, 0, 1, 8);

  layout->addWidget(archive_size_combo, 8, 0, 1, 4);
  layout->addWidget(clear_logs_button, 8, 4, 1, 4);
  layout->addWidget(divider1, 9, 0, 1, 8);
  layout->addItem(spacer_before_actions, 10, 0, 1, 8);
  layout->addWidget(action_row_host, 11, 0, 1, 8);
  layout->addWidget(divider2, 12, 0, 1, 8);
  layout->addWidget(debug_console, 13, 0, 1, 8);
  layout->setRowStretch(13, 1);

  QObject::connect(source_clear_button, &QToolButton::clicked, source_edit, &QLineEdit::clear);
  QObject::connect(archive_clear_button, &QToolButton::clicked, archive_edit, &QLineEdit::clear);
  QObject::connect(unarchive_clear_button, &QToolButton::clicked, unarchive_edit, &QLineEdit::clear);
  QObject::connect(recovery_clear_button, &QToolButton::clicked, recovery_edit, &QLineEdit::clear);
  QObject::connect(source_pick_button, &QToolButton::clicked, [&]() {
    const QString folder = QFileDialog::getExistingDirectory(&window, "Pick pack source folder");
    if (!folder.isEmpty()) {
      source_edit->setText(folder);
    }
  });
  QObject::connect(archive_pick_button, &QToolButton::clicked, [&]() {
    const QString folder = QFileDialog::getExistingDirectory(&window, "Pick archive folder");
    if (!folder.isEmpty()) {
      archive_edit->setText(folder);
    }
  });
  QObject::connect(unarchive_pick_button, &QToolButton::clicked, [&]() {
    const QString folder = QFileDialog::getExistingDirectory(&window, "Pick unarchive folder");
    if (!folder.isEmpty()) {
      unarchive_edit->setText(folder);
    }
  });
  QObject::connect(recovery_pick_button, &QToolButton::clicked, [&]() {
    const QString file = QFileDialog::getOpenFileName(&window, "Pick recovery start file");
    if (!file.isEmpty()) {
      recovery_edit->setText(file);
    }
  });
  QObject::connect(clear_logs_button, &QPushButton::clicked, debug_console, &QPlainTextEdit::clear);

  auto run_with_guard = [&](auto fn) {
    try {
      fn();
    } catch (const std::exception& ex) {
      QMessageBox::critical(&window, "snowstorm", ex.what());
    } catch (...) {
      QMessageBox::critical(&window, "snowstorm", "Unhandled non-standard exception.");
    }
  };
  auto apply_archive_naming_from_ui = [&]() {
    gBundleFilePrefix = file_prefix_edit->text().toStdString();
    gBundleFileSuffix = file_suffix_edit->text().toStdString();
    if (gBundleFilePrefix.empty() || gBundleFileSuffix.empty()) {
      throw std::runtime_error("file_prefix and file_suffix must not be empty.");
    }
  };

  std::vector<QWidget*> interactive_widgets = {
      source_edit,            archive_edit,            unarchive_edit,          recovery_edit,
      password1_edit,         password2_edit,          file_prefix_edit,        file_suffix_edit,
      archive_size_combo,     source_clear_button,
      source_pick_button,     archive_clear_button,    archive_pick_button,     unarchive_clear_button,
      unarchive_pick_button,  recovery_clear_button,   recovery_pick_button,    clear_logs_button,
      pack_button,            unpack_button,           sanity_button,           recover_button,
      debug_console};
  auto* pack_opacity = new QGraphicsOpacityEffect(pack_button);
  auto* unpack_opacity = new QGraphicsOpacityEffect(unpack_button);
  auto* sanity_opacity = new QGraphicsOpacityEffect(sanity_button);
  auto* recover_opacity = new QGraphicsOpacityEffect(recover_button);
  pack_button->setGraphicsEffect(pack_opacity);
  unpack_button->setGraphicsEffect(unpack_opacity);
  sanity_button->setGraphicsEffect(sanity_opacity);
  recover_button->setGraphicsEffect(recover_opacity);
  pack_opacity->setOpacity(1.0);
  unpack_opacity->setOpacity(1.0);
  sanity_opacity->setOpacity(1.0);
  recover_opacity->setOpacity(1.0);
  bool is_busy = false;
  auto set_ui_busy_state = [&](bool busy, bool show_spinner) {
    is_busy = busy;
    for (auto* widget : interactive_widgets) {
      widget->setEnabled(!busy);
    }
    const qreal aOpacity = busy ? 0.45 : 1.0;
    pack_opacity->setOpacity(aOpacity);
    unpack_opacity->setOpacity(aOpacity);
    sanity_opacity->setOpacity(aOpacity);
    recover_opacity->setOpacity(aOpacity);
    action_row_stack->setCurrentWidget(show_spinner ? action_spinner_row : action_buttons_row);
    if (busy && show_spinner) {
      QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
      QApplication::restoreOverrideCursor();
    }
  };
  std::thread worker_thread;
  auto reset_worker_thread_handle = [&]() {
    if (worker_thread.joinable()) {
      worker_thread.detach();
    }
  };
  QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  });
  auto launch_delegate_operation = [&](const QString& pFailurePrefix,
                                       std::function<void(QtPeanutButterDelegate&)> pWork) {
    set_ui_busy_state(true, false);
    reset_worker_thread_handle();
    worker_thread = std::thread([&, pFailurePrefix, pWork = std::move(pWork)]() mutable {
      try {
        QtPeanutButterDelegate aDelegate(
            &window,
            [&](const QString& pLine) { append_log(pLine); },
            [&]() { set_ui_busy_state(true, true); });
        pWork(aDelegate);
        QMetaObject::invokeMethod(
            &window,
            [&]() { set_ui_busy_state(false, false); },
            Qt::QueuedConnection);
      } catch (const std::exception& ex) {
        const QString message = compact_error(QString::fromUtf8(ex.what()));
        const QString lower = message.toLower();
        QMetaObject::invokeMethod(
            &window,
            [&, message, lower, pFailurePrefix]() {
              if (lower.contains("canceled by user")) {
                append_log(QString("%1 canceled by user.").arg(pFailurePrefix));
              } else {
                append_log(QString("%1 failed: %2").arg(pFailurePrefix, message));
              }
              set_ui_busy_state(false, false);
            },
            Qt::QueuedConnection);
      } catch (...) {
        QMetaObject::invokeMethod(
            &window,
            [&, pFailurePrefix]() {
              append_log(QString("%1 failed: unhandled non-standard exception.").arg(pFailurePrefix));
              set_ui_busy_state(false, false);
            },
            Qt::QueuedConnection);
      }
    });
  };
  
  QObject::connect(pack_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      apply_archive_naming_from_ui();
      const fs::path input = resolveUserPath(source_edit->text());
      const fs::path output = resolveUserPath(archive_edit->text());
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      launch_delegate_operation("Pack", [=](QtPeanutButterDelegate& pDelegate) {
        PeanutButterImplementation aImplementation(pDelegate, archive_size);
        const SnowStormBundleStats aStats = aImplementation.pack(input, output);
        pDelegate.logMessage("Packed " + std::to_string(aStats.mFileCount) + " files into " +
                             std::to_string(aStats.mArchiveCount) + " archives.");
      });
    });
  });

  QObject::connect(unpack_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      apply_archive_naming_from_ui();
      const fs::path input = resolveUserPath(archive_edit->text());
      const fs::path output = resolveUserPath(unarchive_edit->text());
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      launch_delegate_operation("Unpack", [=](QtPeanutButterDelegate& pDelegate) {
        PeanutButterImplementation aImplementation(pDelegate, archive_size);
        const SnowStormUnbundleStats aStats = aImplementation.unpack(input, output);
        pDelegate.logMessage("Unpacked " + std::to_string(aStats.mFilesUnbundled) + " files from " +
                             std::to_string(aStats.mArchivesTouched) + " archives.");
      });
    });
  });

  QObject::connect(sanity_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      const fs::path source = resolveUserPath(source_edit->text());
      const fs::path destination = resolveUserPath(unarchive_edit->text());
      if (!fs::exists(source) || !fs::is_directory(source)) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Sanity source directory is missing:\n%1")
                                  .arg(QString::fromStdString(source.string())));
        return;
      }
      if (!fs::exists(destination) || !fs::is_directory(destination)) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Sanity destination directory is missing:\n%1")
                                  .arg(QString::fromStdString(destination.string())));
        return;
      }
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      launch_delegate_operation("Sanity", [=](QtPeanutButterDelegate& pDelegate) {
        PeanutButterImplementation aImplementation(pDelegate, archive_size);
        std::string aError;
        const bool aSuccess = aImplementation.sanity(source, destination, &aError);
        pDelegate.logMessage(aSuccess ? "Sanity passed." : "Sanity failed.");
        if (!aSuccess && !aError.empty()) {
          pDelegate.logMessage(aError);
        }
      });
    });
  });

  QObject::connect(recover_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      apply_archive_naming_from_ui();
      const fs::path recovery_start_file = resolveUserPath(recovery_edit->text());
      const fs::path output = resolveUserPath(unarchive_edit->text());
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      const PeanutButterImplementation::RecoveryStart aRecoveryStart =
          PeanutButterImplementation::resolveRecoveryStartFile(
              recovery_start_file, gBundleFilePrefix, gBundleFileSuffix, FileReaderImplementation{});
      launch_delegate_operation("Recover", [=](QtPeanutButterDelegate& pDelegate) {
        PeanutButterImplementation aImplementation(pDelegate, archive_size);
        const SnowStormUnbundleStats aStats = aImplementation.recover(aRecoveryStart, output);
        pDelegate.logMessage("Recovered " + std::to_string(aStats.mFilesUnbundled) + " files from " +
                             std::to_string(aStats.mArchivesTouched) + " archives.");
      });
    });
  });

  window.resize(720, 600);
  window.show();
  return app.exec();
}
