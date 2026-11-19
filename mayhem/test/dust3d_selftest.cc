// Functional oracle for dust3d's core (built with normal flags by mayhem/build.sh, run by
// mayhem/test.sh which diffs every output line against mayhem/test/expected.txt).
//   model <xml>   : parse a snapshot XML, generate the mesh, print node/part counts and the
//                   generated vertex/triangle counts plus a rounded bounding box.
//   ds3 <xml>     : write the XML (and a binary asset) into a .ds3 with Ds3FileWriter, read it
//                   back with Ds3FileReader, and print item names/types/sizes and a checksum.
//   xmlrt <xml>   : snapshot -> saveSnapshotToXmlString -> reload, print counts on both sides.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <dust3d/base/ds3_file.h>
#include <dust3d/base/object.h>
#include <dust3d/base/snapshot.h>
#include <dust3d/base/snapshot_xml.h>
#include <dust3d/mesh/mesh_generator.h>

static std::vector<char> readFile(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return data;
}

static unsigned long fnv(const std::uint8_t* p, size_t n)
{
    unsigned long h = 2166136261u;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ p[i]) * 16777619u & 0xffffffffu;
    return h;
}

static void counts(const char* tag, const dust3d::Snapshot& s)
{
    std::printf("%s nodes=%zu edges=%zu parts=%zu components=%zu canvas=%zu\n", tag, s.nodes.size(),
        s.edges.size(), s.parts.size(), s.components.size(), s.canvas.size());
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s model|ds3|xmlrt <file.xml>\n", argv[0]);
        return 2;
    }
    std::string mode = argv[1];
    std::vector<char> xml = readFile(argv[2]);
    if (xml.empty()) {
        std::fprintf(stderr, "cannot read %s\n", argv[2]);
        return 2;
    }
    if (mode == "model") {
        xml.push_back('\0');
        auto* snapshot = new dust3d::Snapshot;
        dust3d::loadSnapshotFromXmlString(snapshot, xml.data());
        counts("snapshot", *snapshot);
        dust3d::MeshGenerator generator(snapshot);
        generator.generate();
        dust3d::Object* object = generator.takeObject();
        if (!object) {
            std::printf("mesh none\n");
            return 0;
        }
        double minv[3] = { 1e30, 1e30, 1e30 }, maxv[3] = { -1e30, -1e30, -1e30 };
        for (const auto& v : object->vertices) {
            for (int i = 0; i < 3; ++i) {
                minv[i] = std::min(minv[i], v[i]);
                maxv[i] = std::max(maxv[i], v[i]);
            }
        }
        std::printf("mesh vertices=%zu triangles=%zu\n", object->vertices.size(), object->triangles.size());
        std::printf("bbox %.2f %.2f %.2f %.2f %.2f %.2f\n", minv[0], minv[1], minv[2], maxv[0], maxv[1], maxv[2]);
        delete object;
        return 0;
    }
    if (mode == "xmlrt") {
        xml.push_back('\0');
        dust3d::Snapshot snapshot;
        dust3d::loadSnapshotFromXmlString(&snapshot, xml.data());
        counts("before", snapshot);
        std::string out;
        dust3d::saveSnapshotToXmlString(snapshot, out);
        std::vector<char> copy(out.begin(), out.end());
        copy.push_back('\0');
        dust3d::Snapshot again;
        dust3d::loadSnapshotFromXmlString(&again, copy.data());
        counts("after", again);
        return 0;
    }
    if (mode == "ds3") {
        std::vector<std::uint8_t> asset;
        for (int i = 0; i < 1000; ++i)
            asset.push_back((std::uint8_t)(i * 7));
        dust3d::Ds3FileWriter writer;
        writer.add("model.xml", "model", xml.data(), xml.size());
        writer.add("canvas.png", "asset", asset.data(), asset.size());
        std::vector<std::uint8_t> file;
        writer.save(file);
        dust3d::Ds3FileReader reader(file.data(), file.size());
        std::printf("ds3 items=%zu\n", reader.items().size());
        for (const auto& item : reader.items()) {
            std::vector<std::uint8_t> bytes;
            reader.loadItem(item.name, &bytes);
            std::printf("item %s %s size=%zu fnv=%08lx\n", item.type.c_str(), item.name.c_str(), bytes.size(),
                fnv(bytes.data(), bytes.size()));
        }
        std::printf("source fnv=%08lx asset fnv=%08lx\n", fnv((const std::uint8_t*)xml.data(), xml.size()),
            fnv(asset.data(), asset.size()));
        return 0;
    }
    std::fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 2;
}
