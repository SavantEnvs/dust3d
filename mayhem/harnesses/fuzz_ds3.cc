// Fuzz the .ds3 document container and the snapshot XML it carries, the way
// DocumentWindow::openPathDataAs (application/sources/document_window.cc) opens a file:
// Ds3FileReader over the raw bytes, loadItem() for every item, and for "model" items a
// NUL-terminated copy fed to loadSnapshotFromXmlString(), then the snapshot is saved back
// to XML (the document's save path) and re-parsed.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <dust3d/base/ds3_file.h>
#include <dust3d/base/snapshot.h>
#include <dust3d/base/snapshot_xml.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    dust3d::Ds3FileReader reader(data, size);
    for (const auto& item : reader.items()) {
        std::vector<std::uint8_t> bytes;
        reader.loadItem(item.name, &bytes);
        if (item.type != "model")
            continue;
        // Same cap as the application (document_window.cc maxXmlSize).
        static constexpr size_t maxXmlSize = 256 * 1024 * 1024;
        if (bytes.size() > maxXmlSize)
            continue;
        bytes.push_back('\0');
        dust3d::Snapshot snapshot;
        dust3d::loadSnapshotFromXmlString(&snapshot, reinterpret_cast<char*>(bytes.data()));
        std::string xml;
        dust3d::saveSnapshotToXmlString(snapshot, xml);
        std::vector<char> copy(xml.begin(), xml.end());
        copy.push_back('\0');
        dust3d::Snapshot reloaded;
        dust3d::loadSnapshotFromXmlString(&reloaded, copy.data());
    }
    return 0;
}
