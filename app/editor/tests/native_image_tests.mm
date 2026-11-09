#include <QColorSpace>
#include <QCoreApplication>
#include <QImage>
#include <QTextStream>
#include <CoreGraphics/CoreGraphics.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    // A custom ICC profile takes the non-system-owned colorspace path that
    // Qt 6.11.1 released before CGImageCreate (QTBUG-147602).
    const QColorSpace colorSpace(QColorSpace::Primaries::SRgb, 1.8f);
    if (!colorSpace.isValid() || colorSpace.iccProfile().isEmpty()) return 1;
    for (int i = 0; i < 64; ++i) {
        CGImageRef native = nullptr;
        {
            QImage image(16, 16, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::red);
            image.setColorSpace(colorSpace);
            native = image.toCGImage();
        }
        if (!native) return 2;
        CGColorSpaceRef actual = CGImageGetColorSpace(native);
        CFDataRef profile = actual ? CGColorSpaceCopyICCData(actual) : nullptr;
        const bool valid = profile && CFDataGetLength(profile) > 0
            && CGColorSpaceGetModel(actual) == kCGColorSpaceModelRGB
            && CGImageGetWidth(native) == 16 && CGImageGetHeight(native) == 16;
        if (profile) CFRelease(profile);
        CGImageRelease(native);
        if (!valid) return 3;
    }
    QTextStream(stdout) << "PASS: 64 native CGImage custom-colorspace lifetime conversions\n";
    return 0;
}
