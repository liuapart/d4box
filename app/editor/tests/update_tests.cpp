#include "EditorUpdates.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool passed, const char *message) {
        if (!passed) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    const auto asset = EditorUpdates::assetName(QStringLiteral("%1"), QStringLiteral("cocoa"),
                                                QStringLiteral("arm64"));
    check(asset == QStringLiteral("D4BoxEditor-%1-macos-arm64.zip"), "Mac asset name");
    check(EditorUpdates::assetName(QStringLiteral("1.2.3"), QStringLiteral("windows"),
                                   QStringLiteral("x86_64")) ==
          QStringLiteral("D4BoxEditor-1.2.3-windows-x64.zip"), "Windows portable asset name");
    check(EditorUpdates::assetName(QStringLiteral("1.2.3"), QStringLiteral("windows"),
                                   QStringLiteral("x86_64"), true) ==
          QStringLiteral("D4BoxEditor-1.2.3-windows-x64-setup.exe"), "Windows installer asset name");
    check(EditorUpdates::assetName(QStringLiteral("1.2.3"), QStringLiteral("windows"),
                                   QStringLiteral("arm64")).isEmpty(), "unsupported architecture has no asset");
    const auto curlArgs = EditorUpdates::publicDownloadArgs(
        QStringLiteral("https://github.com/liuapart/d4box/releases/download/editor-v1.2.3/SHA256SUMS.txt"),
        QStringLiteral("/tmp/checksums.txt"), 131072, 120);
    check(curlArgs.first() == QStringLiteral("--disable") &&
          curlArgs.contains(QStringLiteral("--location")) &&
          curlArgs.contains(QStringLiteral("--fail")) &&
          curlArgs.contains(QStringLiteral("--proto-redir")) &&
          curlArgs.contains(QStringLiteral("=https")) &&
          curlArgs.contains(QStringLiteral("131072")) &&
          curlArgs.contains(QStringLiteral("/tmp/checksums.txt")),
          "public download follows HTTPS redirects with size and time bounds");
    check(!curlArgs.contains(QStringLiteral("Authorization")) &&
          !curlArgs.contains(QStringLiteral("gh")), "public download needs no GitHub credentials");
    QString reason;
    check(!EditorUpdates::isPackaged(&reason) && !reason.isEmpty(),
          "development test executable cannot be overwritten by a release");
    auto makeRelease = [](const QString &version, bool prerelease = false, bool draft = false,
                          bool badUrl = false, bool missingSums = false) {
        const auto tag = QStringLiteral("editor-v") + version;
        const auto name = QStringLiteral("D4BoxEditor-%1-macos-arm64.zip").arg(version);
        const auto prefix = QStringLiteral("https://github.com/liuapart/d4box/releases/download/") + tag + '/';
        QJsonArray assets{QJsonObject{{QStringLiteral("name"), name},
                                      {QStringLiteral("browser_download_url"),
                                       (badUrl ? QStringLiteral("https://example.invalid/") : prefix) + name}}};
        if (!missingSums)
            assets.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("SHA256SUMS.txt")},
                                      {QStringLiteral("browser_download_url"), prefix + QStringLiteral("SHA256SUMS.txt")}});
        return QJsonObject{{QStringLiteral("tag_name"), tag}, {QStringLiteral("prerelease"), prerelease},
                           {QStringLiteral("draft"), draft}, {QStringLiteral("assets"), assets}};
    };
    // Publication order and GitHub's "latest" flag are deliberately irrelevant.
    const QJsonArray releases{makeRelease(QStringLiteral("1.2.5"), true),
                              makeRelease(QStringLiteral("1.2.4")),
                              makeRelease(QStringLiteral("1.3.0"), false, true),
                              makeRelease(QStringLiteral("1.2.7"), false, false, true),
                              makeRelease(QStringLiteral("1.2.6"), false, false, false, true),
                              makeRelease(QStringLiteral("1.2.3")),
                              makeRelease(QStringLiteral("1.2.8-rc.1"), true)};
    const auto json = QJsonDocument(releases).toJson(QJsonDocument::Compact);
    QString error;
    const auto found = EditorUpdates::newest(json, QStringLiteral("1.2.2"), asset, &error);
    check(found && found->version == QStringLiteral("1.2.4") && error.isEmpty(),
          "choose highest complete stable release, ignoring bad URLs and missing checksums");
    check(found && found->assetUrl.endsWith(QStringLiteral("/D4BoxEditor-1.2.4-macos-arm64.zip")),
          "choose matching architecture URL");
    check(!EditorUpdates::newest(json, QStringLiteral("1.2.4"), asset), "no downgrade or same-version update");
    check(EditorUpdates::newest(json, QStringLiteral("1.2.4-rc.1"), asset).has_value(),
          "stable release replaces same-number prerelease");
    check(!EditorUpdates::newest(QByteArrayLiteral("{}"), QStringLiteral("1.0.0"), asset, &error)
              && !error.isEmpty(), "reject non-array API response");
    check(!EditorUpdates::newest(QByteArrayLiteral("not json"), QStringLiteral("1.0.0"), asset, &error)
              && !error.isEmpty(), "reject malformed API response");
    const auto digest = QByteArray(64, 'a');
    const auto sums = digest + QByteArrayLiteral("  D4BoxEditor-1.2.4-macos-arm64.zip\r\n") +
                      QByteArray(64, 'b') + QByteArrayLiteral("  other.zip\n");
    check(EditorUpdates::expectedHash(sums, QStringLiteral("D4BoxEditor-1.2.4-macos-arm64.zip")) == digest,
          "select exact SHA-256 entry with CRLF support");
    check(EditorUpdates::expectedHash(sums, QStringLiteral("D4BoxEditor-1.2.4-macos-x64.zip")).isEmpty(),
          "do not accept another architecture checksum");
    check(EditorUpdates::expectedHash(QByteArray(64, 'z') +
          QByteArrayLiteral("  D4BoxEditor-1.2.4-macos-arm64.zip\n"),
          QStringLiteral("D4BoxEditor-1.2.4-macos-arm64.zip")).isEmpty(),
          "reject invalid checksum text");
    std::cout << "Editor updates: " << failures << " failures\n";
    return failures ? 1 : 0;
}
