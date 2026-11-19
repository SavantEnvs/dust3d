// Fuzz the GLB (binary glTF) importer: GlbReader::read (application/sources/glb_reader.cc)
// parses the GLB header and chunks, the JSON scene (nlohmann json), accessors/bufferViews
// and the embedded texture. The application calls it on every models/<uuid>.glb asset of an
// opened .ds3 file (MeshGenerator::parseImportedModelData) and on imported model files.
#include <cstddef>
#include <cstdint>

#include <QByteArray>
#include <QImage>

#include "glb_reader.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    QByteArray bytes(reinterpret_cast<const char*>(data), static_cast<int>(size));
    dust3d::MeshGenerator::ImportedModelData result;
    QImage texture;
    GlbReader::read(bytes, result, &texture);
    return 0;
}
