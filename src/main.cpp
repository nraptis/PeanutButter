#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
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
#include <QString>
#include <QToolButton>
#include <QUrl>
#include <QWidget>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "ConfigLoader.hpp"
#include "Globals.hpp"
#include "SandStorm/SandStorm.hpp"
#include "SnowStorm/SnowStorm.hpp"

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

std::u32string to_u32(const QString& s) {
  const QVector<uint> aUcs4 = s.toUcs4();
  std::u32string aOutput;
  aOutput.reserve(static_cast<std::size_t>(aUcs4.size()));
  for (const uint aCodePoint : aUcs4) {
    aOutput.push_back(static_cast<char32_t>(aCodePoint));
  }
  return aOutput;
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

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  if (gBlockSize != kPermanentBlockSize) {
    QMessageBox::critical(nullptr,
                          "snowstorm",
                          "FATAL: block size is permanently fixed at 500,000 bytes.");
    return 1;
  }
  if ((gArchiveSize % kPermanentBlockSize) != 0) {
    QMessageBox::critical(nullptr,
                          "snowstorm",
                          QString("FATAL: gArchiveSize (%1) must be an exact multiple of 500,000 bytes.")
                              .arg(QString::number(static_cast<qulonglong>(gArchiveSize))));
    return 1;
  }

  QWidget window;
  window.setWindowTitle("peanut butter");
  window.setMinimumSize(480, 500);

  auto* layout = new QGridLayout(&window);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);
  auto* source_edit = new QLineEdit(&window);
  auto* archive_edit = new QLineEdit(&window);
  auto* unarchive_edit = new QLineEdit(&window);
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
  auto* divider1 = new QFrame(&window);
  auto* divider_inputs = new QFrame(&window);
  auto* divider_passwords = new QFrame(&window);
  auto* divider2 = new QFrame(&window);
  auto* divider_to_buttons_spacer = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* spacer_before_inputs_divider = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* spacer_after_inputs_divider = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* spacer_before_password_divider = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* spacer_after_password_divider = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
  auto* pack_button = new QPushButton("Pack", &window);
  auto* unpack_button = new QPushButton("Unpack", &window);
  auto* pack_spinner = new QProgressBar(&window);
  auto* unpack_spinner = new QProgressBar(&window);
  auto* debug_console = new QPlainTextEdit(&window);

  source_edit->setPlaceholderText("pack source folder -> archive folder");
  archive_edit->setPlaceholderText("archive folder -> unarchive folder");
  unarchive_edit->setPlaceholderText("unarchive folder");
  password1_edit->setPlaceholderText("Password1");
  password2_edit->setPlaceholderText("Password2");
  archive_size_combo->setToolTip("Select archive size in bytes");
  password1_edit->setEchoMode(QLineEdit::Password);
  password2_edit->setEchoMode(QLineEdit::Password);
  source_edit->setTextMargins(0, 4, 0, 4);
  archive_edit->setTextMargins(0, 4, 0, 4);
  unarchive_edit->setTextMargins(0, 4, 0, 4);
  password1_edit->setTextMargins(0, 4, 0, 4);
  password2_edit->setTextMargins(0, 4, 0, 4);

  source_clear_button->setText("X");
  archive_clear_button->setText("X");
  unarchive_clear_button->setText("X");
  source_pick_button->setText("Pick");
  archive_pick_button->setText("Pick");
  unarchive_pick_button->setText("Pick");

  source_clear_button->setToolTip("Clear pack source folder");
  archive_clear_button->setToolTip("Clear archive folder");
  unarchive_clear_button->setToolTip("Clear unarchive folder");
  source_pick_button->setToolTip("Pick pack source folder");
  archive_pick_button->setToolTip("Pick archive folder");
  unarchive_pick_button->setToolTip("Pick unarchive folder");

  constexpr int kInputHeight = 44;
  source_edit->setFixedHeight(kInputHeight);
  archive_edit->setFixedHeight(kInputHeight);
  unarchive_edit->setFixedHeight(kInputHeight);
  password1_edit->setFixedHeight(kInputHeight);
  password2_edit->setFixedHeight(kInputHeight);
  archive_size_combo->setFixedHeight(kInputHeight);
  source_clear_button->setFixedSize(54, kInputHeight);
  archive_clear_button->setFixedSize(54, kInputHeight);
  unarchive_clear_button->setFixedSize(54, kInputHeight);
  source_pick_button->setFixedSize(72, kInputHeight);
  archive_pick_button->setFixedSize(72, kInputHeight);
  unarchive_pick_button->setFixedSize(72, kInputHeight);
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
      "QPushButton#clearLogsButton { background-color: #000000; color: #ffffff; border-radius: 10px; }");

  constexpr int kActionHeight = 54;
  pack_button->setFixedHeight(kActionHeight);
  unpack_button->setFixedHeight(kActionHeight);
  pack_button->setMinimumHeight(kActionHeight);
  pack_button->setMaximumHeight(kActionHeight);
  unpack_button->setMinimumHeight(kActionHeight);
  unpack_button->setMaximumHeight(kActionHeight);
  pack_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  unpack_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  pack_spinner->setRange(0, 0);
  unpack_spinner->setRange(0, 0);
  pack_spinner->setTextVisible(false);
  unpack_spinner->setTextVisible(false);
  pack_spinner->setFixedHeight(kActionHeight);
  unpack_spinner->setFixedHeight(kActionHeight);
  pack_spinner->setMinimumHeight(kActionHeight);
  pack_spinner->setMaximumHeight(kActionHeight);
  unpack_spinner->setMinimumHeight(kActionHeight);
  unpack_spinner->setMaximumHeight(kActionHeight);
  pack_spinner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  unpack_spinner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  pack_spinner->setVisible(false);
  unpack_spinner->setVisible(false);

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

  QJsonObject config_defaults;
  try {
    const AppConfig aConfig = loadConfig(resolveConfigPath());
    config_defaults.insert("default_source_path", QString::fromStdString(aConfig.mDefaultSourcePath));
    config_defaults.insert("default_archive_path", QString::fromStdString(aConfig.mDefaultArchivePath));
    config_defaults.insert("default_unarchive_path", QString::fromStdString(aConfig.mDefaultUnarchivePath));
    config_defaults.insert("default_password_1", QString::fromStdString(aConfig.mDefaultPassword1));
    config_defaults.insert("default_password_2", QString::fromStdString(aConfig.mDefaultPassword2));
  } catch (...) {
    QFile config_file("config.json");
    if (config_file.open(QIODevice::ReadOnly)) {
      const QJsonDocument config_doc = QJsonDocument::fromJson(config_file.readAll());
      if (config_doc.isObject()) {
        config_defaults = config_doc.object();
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
  password1_edit->setText(config_value("default_password_1", "banana"));
  password2_edit->setText(config_value("default_password_2", "apple"));

  struct ArchiveSizeOption {
    const char* mLabel = "";
    std::uint64_t mBytes = 0;
  };
  const std::vector<ArchiveSizeOption> archive_size_options = {
      {"1 MB (1,000,000)", 1000000ULL},
      {"10 MB (10,000,000)", 10000000ULL},
      {"50 MB (50,000,000)", 50000000ULL},
      {"100 MB (100,000,000)", 100000000ULL},
      {"250 MB (250,000,000)", 250000000ULL},
      {"500 MB (500,000,000)", 500000000ULL},
      {"750 MB (750,000,000)", 750000000ULL},
      {"1 GB (1,000,000,000)", 1000000000ULL},
      {"2 GB (2,000,000,000)", 2000000000ULL},
  };
  int archive_default_index = 0;
  for (std::size_t i = 0; i < archive_size_options.size(); ++i) {
    const ArchiveSizeOption& option = archive_size_options[i];
    archive_size_combo->addItem(QString::fromUtf8(option.mLabel),
                                QVariant::fromValue(static_cast<qulonglong>(option.mBytes)));
    if (option.mBytes == gArchiveSize) {
      archive_default_index = static_cast<int>(i);
    }
  }
  archive_size_combo->setCurrentIndex(archive_default_index);

  struct FolderDropFilter : QObject {
    explicit FolderDropFilter(QLineEdit* target_edit) : edit(target_edit) {}
    QLineEdit* edit = nullptr;

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
            if (fs::is_directory(p)) {
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
            if (fs::is_directory(p)) {
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
  auto* source_drop_filter = new FolderDropFilter(source_edit);
  auto* archive_drop_filter = new FolderDropFilter(archive_edit);
  auto* unarchive_drop_filter = new FolderDropFilter(unarchive_edit);
  source_drop_filter->setParent(&window);
  archive_drop_filter->setParent(&window);
  unarchive_drop_filter->setParent(&window);
  source_edit->installEventFilter(source_drop_filter);
  archive_edit->installEventFilter(archive_drop_filter);
  unarchive_edit->installEventFilter(unarchive_drop_filter);

  auto compact_error = [](QString message) {
    message.replace('\r', ' ');
    if (message.size() > 420) {
      message = message.left(420) + " ... [truncated]";
    }
    return message;
  };

  auto is_hidden_file = [](const fs::path& p) {
    const std::string name = p.filename().string();
    return !name.empty() && name[0] == '.';
  };

  auto has_meaningful_entries = [&](const fs::path& dir) {
    std::vector<fs::path> stack;
    stack.push_back(dir);

    while (!stack.empty()) {
      const fs::path current = stack.back();
      stack.pop_back();

      for (const auto& entry : fs::directory_iterator(current)) {
        const fs::path entry_path = entry.path();
        if (entry.is_regular_file()) {
          return true;
        }
        if (entry.is_directory()) {
          stack.push_back(entry_path);
        }
      }
    }
    return false;
  };

  auto scan_hidden_files = [&](const fs::path& dir, std::size_t pLimit) {
    std::vector<fs::path> hidden_files;
    std::vector<fs::path> stack;
    stack.push_back(dir);

    while (!stack.empty()) {
      const fs::path current = stack.back();
      stack.pop_back();
      for (const auto& entry : fs::directory_iterator(current)) {
        const fs::path entry_path = entry.path();
        if (entry.is_directory()) {
          stack.push_back(entry_path);
          continue;
        }
        if (!entry.is_regular_file()) {
          continue;
        }
        if (is_hidden_file(entry_path)) {
          hidden_files.push_back(entry_path);
          if (hidden_files.size() >= pLimit) {
            return hidden_files;
          }
        }
      }
    }

    return hidden_files;
  };

  auto clear_directory_contents = [&](const fs::path& dir) {
    if (!fs::exists(dir)) {
      return;
    }
    for (const auto& entry : fs::directory_iterator(dir)) {
      std::error_code ec;
      fs::remove_all(entry.path(), ec);
      if (ec) {
        throw std::runtime_error("Failed to clear destination entry: " + entry.path().string() + " (" + ec.message() + ")");
      }
    }
  };

  layout->addWidget(source_clear_button, 0, 0);
  layout->addWidget(source_edit, 0, 1, 1, 2);
  layout->addWidget(source_pick_button, 0, 3);

  layout->addWidget(archive_clear_button, 1, 0);
  layout->addWidget(archive_edit, 1, 1, 1, 2);
  layout->addWidget(archive_pick_button, 1, 3);

  layout->addWidget(unarchive_clear_button, 2, 0);
  layout->addWidget(unarchive_edit, 2, 1, 1, 2);
  layout->addWidget(unarchive_pick_button, 2, 3);

  layout->addItem(spacer_before_inputs_divider, 3, 0, 1, 4);
  layout->addWidget(divider_inputs, 4, 0, 1, 4);
  layout->addItem(spacer_after_inputs_divider, 5, 0, 1, 4);

  layout->addWidget(password1_edit, 6, 0, 1, 2);
  layout->addWidget(password2_edit, 6, 2, 1, 2);

  layout->addItem(spacer_before_password_divider, 7, 0, 1, 4);
  layout->addWidget(divider_passwords, 8, 0, 1, 4);
  layout->addItem(spacer_after_password_divider, 9, 0, 1, 4);

  layout->addWidget(archive_size_combo, 10, 0, 1, 2);
  layout->addWidget(clear_logs_button, 10, 2, 1, 2);
  layout->addWidget(divider1, 11, 0, 1, 4);
  layout->addItem(divider_to_buttons_spacer, 12, 0, 1, 4);
  layout->addWidget(pack_button, 13, 0, 1, 2);
  layout->addWidget(unpack_button, 13, 2, 1, 2);
  layout->addWidget(pack_spinner, 13, 0, 1, 2);
  layout->addWidget(unpack_spinner, 13, 2, 1, 2);
  layout->addWidget(divider2, 14, 0, 1, 4);
  layout->addWidget(debug_console, 15, 0, 1, 4);
  layout->setRowStretch(15, 1);

  QObject::connect(source_clear_button, &QToolButton::clicked, source_edit, &QLineEdit::clear);
  QObject::connect(archive_clear_button, &QToolButton::clicked, archive_edit, &QLineEdit::clear);
  QObject::connect(unarchive_clear_button, &QToolButton::clicked, unarchive_edit, &QLineEdit::clear);
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
  QObject::connect(clear_logs_button, &QPushButton::clicked, debug_console, &QPlainTextEdit::clear);

  auto run_with_guard = [&](auto fn) {
    try {
      fn();
    } catch (const std::exception& ex) {
      QMessageBox::critical(&window, "snowstorm", ex.what());
    }
  };

  std::vector<QWidget*> interactive_widgets = {
      source_edit,            archive_edit,            unarchive_edit,        password1_edit,
      password2_edit,         archive_size_combo,      source_clear_button,   source_pick_button,
      archive_clear_button,   archive_pick_button,     unarchive_clear_button, unarchive_pick_button, clear_logs_button, pack_button,
      unpack_button,          debug_console};
  bool is_busy = false;
  auto set_busy = [&](bool busy) {
    is_busy = busy;
    for (auto* widget : interactive_widgets) {
      widget->setEnabled(!busy);
    }
    pack_button->setVisible(!busy);
    unpack_button->setVisible(!busy);
    pack_spinner->setVisible(busy);
    unpack_spinner->setVisible(busy);
    if (busy) {
      QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
      QApplication::restoreOverrideCursor();
    }
  };
  std::thread worker_thread;
  QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  });
  
  QObject::connect(pack_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      const fs::path input = resolveUserPath(source_edit->text());
      const fs::path output = resolveUserPath(archive_edit->text());
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      if (!fs::exists(input) || !fs::is_directory(input)) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Pack source directory is missing:\n%1")
                                  .arg(QString::fromStdString(input.string())));
        return;
      }
      if ((archive_size % kPermanentBlockSize) != 0) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Archive size must be an exact multiple of %1 bytes.")
                                  .arg(QString::number(static_cast<qulonglong>(kPermanentBlockSize))));
        return;
      }

      const std::u32string password_1 = to_u32(password1_edit->text());
      const std::u32string password_2 = to_u32(password2_edit->text());
      SandStorm preflight_crypt(gBlockSize, password_1, password_2);
      SnowStorm preflight_snowstorm(gBlockSize, archive_size, &preflight_crypt);
      const ShouldBundleResult should = preflight_snowstorm.shouldBundle(input, output);
      if (should.mDecision == ShouldBundleDecision::No) {
        QMessageBox::critical(&window, "snowstorm", QString::fromStdString(should.mMessage));
        return;
      }
      if (should.mDecision == ShouldBundleDecision::Prompt) {
        const auto decision = QMessageBox::question(
            &window,
            "snowstorm",
            QString("%1\n\nPress OK to clear destination and continue.")
                .arg(QString::fromStdString(should.mMessage)),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (decision != QMessageBox::Ok) {
          append_log("Pack canceled by user.");
          return;
        }
      }
      set_busy(true);
      append_log("Pack started.");
      append_log("BLOCK SIZE ALERT: 500,000 bytes is permanently fixed and must never be changed.");
      const bool clear_destination = (should.mDecision == ShouldBundleDecision::Prompt);
      const fs::path clear_destination_path = should.mResolvedDestination;
      if (worker_thread.joinable()) {
        worker_thread.join();
      }

      worker_thread = std::thread([&, input, output, password_1, password_2, clear_destination, clear_destination_path, archive_size]() {
        try {
          if (!has_meaningful_entries(input)) {
            throw std::runtime_error("Pack source directory is missing or empty: " + input.string());
          }
          const std::vector<fs::path> hidden_files = scan_hidden_files(input, 200);
          for (const fs::path& hidden_path : hidden_files) {
            const QString line = "Packing hidden file: " + QString::fromStdString(hidden_path.string());
            QMetaObject::invokeMethod(
                &window,
                [&, line]() { append_log(line); },
                Qt::QueuedConnection);
          }
          if (hidden_files.size() >= 200) {
            const QString line =
                "Packing hidden files: output truncated after 200 entries; remaining hidden files are omitted from logs.";
            QMetaObject::invokeMethod(
                &window,
                [&, line]() { append_log(line); },
                Qt::QueuedConnection);
          }
          if (clear_destination) {
            clear_directory_contents(clear_destination_path);
            const QString line = "Cleared destination directory: " + QString::fromStdString(clear_destination_path.string());
            QMetaObject::invokeMethod(
                &window,
                [&, line]() { append_log(line); },
                Qt::QueuedConnection);
          }

          SandStorm worker_crypt(gBlockSize, password_1, password_2);
          SnowStorm worker_snowstorm(gBlockSize, archive_size, &worker_crypt);
          const BundleStats stats = worker_snowstorm.bundle(
              input,
              output,
              [&](std::uint64_t archive_idx, std::uint64_t archive_total, std::uint64_t files_done, std::uint64_t files_total) {
                const QString line = QString("Packing %1 of %2 Archives, %3 of %4 Files")
                                         .arg(QString::number(static_cast<qulonglong>(archive_idx)))
                                         .arg(QString::number(static_cast<qulonglong>(archive_total)))
                                         .arg(QString::number(static_cast<qulonglong>(files_done)))
                                         .arg(QString::number(static_cast<qulonglong>(files_total)));
                QMetaObject::invokeMethod(
                    &window,
                    [&, line]() { append_log(line); },
                    Qt::QueuedConnection);
              });

          QMetaObject::invokeMethod(
              &window,
              [&, file_count = static_cast<qulonglong>(stats.mFileCount), archive_count = static_cast<qulonglong>(stats.mArchiveCount)]() {
                const QString summary = QString("Packed %1 files into %2 archives.")
                                            .arg(QString::number(file_count))
                                            .arg(QString::number(archive_count));
                append_log(summary);
                set_busy(false);
              },
              Qt::QueuedConnection);
        } catch (const std::exception& ex) {
          const QString message = compact_error(QString::fromUtf8(ex.what()));
          QMetaObject::invokeMethod(
              &window,
              [&, message]() {
                append_log(QString("Pack failed: %1").arg(message));
                set_busy(false);
                QMessageBox::critical(&window, "snowstorm", message);
              },
              Qt::QueuedConnection);
        }
      });
    });
  });

  QObject::connect(unpack_button, &QPushButton::clicked, [&]() {
    run_with_guard([&]() {
      if (is_busy) {
        return;
      }
      const fs::path input = resolveUserPath(archive_edit->text());
      const fs::path output = resolveUserPath(unarchive_edit->text());
      const std::uint64_t archive_size =
          static_cast<std::uint64_t>(archive_size_combo->currentData().toULongLong());
      if (!fs::exists(input) || !fs::is_directory(input)) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Unpack source directory is missing:\n%1")
                                  .arg(QString::fromStdString(input.string())));
        return;
      }
      if ((archive_size % kPermanentBlockSize) != 0) {
        QMessageBox::critical(&window,
                              "snowstorm",
                              QString("Archive size must be an exact multiple of %1 bytes.")
                                  .arg(QString::number(static_cast<qulonglong>(kPermanentBlockSize))));
        return;
      }

      const std::u32string password_1 = to_u32(password1_edit->text());
      const std::u32string password_2 = to_u32(password2_edit->text());
      SandStorm preflight_crypt(gBlockSize, password_1, password_2);
      SnowStorm preflight_snowstorm(gBlockSize, archive_size, &preflight_crypt);
      const ShouldBundleResult should = preflight_snowstorm.shouldUnbundle(input, output);
      if (should.mDecision == ShouldBundleDecision::No) {
        QMessageBox::critical(&window, "snowstorm", QString::fromStdString(should.mMessage));
        return;
      }
      if (should.mDecision == ShouldBundleDecision::Prompt) {
        const auto decision = QMessageBox::question(
            &window,
            "snowstorm",
            QString("%1\n\nPress OK to clear destination and continue.")
                .arg(QString::fromStdString(should.mMessage)),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (decision != QMessageBox::Ok) {
          append_log("Unpack canceled by user.");
          return;
        }
      }
      set_busy(true);
      append_log("Unpack started.");
      append_log("BLOCK SIZE ALERT: 500,000 bytes is permanently fixed and must never be changed.");
      const bool clear_destination = (should.mDecision == ShouldBundleDecision::Prompt);
      const fs::path clear_destination_path = should.mResolvedDestination;
      if (worker_thread.joinable()) {
        worker_thread.join();
      }

      worker_thread = std::thread([&, input, output, password_1, password_2, clear_destination, clear_destination_path, archive_size]() {
        try {
          if (!has_meaningful_entries(input)) {
            throw std::runtime_error("Unpack source directory is missing or empty: " + input.string());
          }
          if (clear_destination) {
            clear_directory_contents(clear_destination_path);
            const QString line = "Cleared destination directory: " + QString::fromStdString(clear_destination_path.string());
            QMetaObject::invokeMethod(
                &window,
                [&, line]() { append_log(line); },
                Qt::QueuedConnection);
          }

          SandStorm worker_crypt(gBlockSize, password_1, password_2);
          SnowStorm worker_snowstorm(gBlockSize, archive_size, &worker_crypt);
          const UnbundleStats stats = worker_snowstorm.unbundle(
              input,
              output,
              [&](std::uint64_t archive_idx, std::uint64_t archive_total, std::uint64_t files_done, std::uint64_t /*files_total*/) {
                const QString line = QString("Unpacking %1 of %2 Archives, %3 Files")
                                         .arg(QString::number(static_cast<qulonglong>(archive_idx)))
                                         .arg(QString::number(static_cast<qulonglong>(archive_total)))
                                         .arg(QString::number(static_cast<qulonglong>(files_done)));
                QMetaObject::invokeMethod(
                    &window,
                    [&, line]() { append_log(line); },
                    Qt::QueuedConnection);
              });

          QMetaObject::invokeMethod(
              &window,
              [&, files_unpacked = static_cast<qulonglong>(stats.mFilesUnbundled), archives_total = static_cast<qulonglong>(stats.mArchivesTotal)]() {
                const QString summary = QString("Unpacked %1 files from %2 archives.")
                                            .arg(QString::number(files_unpacked))
                                            .arg(QString::number(archives_total));
                append_log(summary);
                set_busy(false);
              },
              Qt::QueuedConnection);
        } catch (const std::exception& ex) {
          const QString message = compact_error(QString::fromUtf8(ex.what()));
          QMetaObject::invokeMethod(
              &window,
              [&, message]() {
                append_log(QString("Unpack failed: %1").arg(message));
                set_busy(false);
                QMessageBox::critical(&window, "snowstorm", message);
              },
              Qt::QueuedConnection);
        }
      });
    });
  });

  window.resize(720, 600);
  window.show();
  return app.exec();
}
