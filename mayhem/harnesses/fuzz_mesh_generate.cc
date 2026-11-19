// Fuzz mesh generation from a snapshot: the model XML (the "model" item of a .ds3, or the
// clipboard XML pasted with -paste-xml) is parsed by loadSnapshotFromXmlString() and the
// resulting snapshot drives dust3d::MeshGenerator::generate() — the same calls the
// application's MeshGenerator::process() worker makes (application/sources/mesh_generator.cc).
//
// Precondition kept on purpose: in the application, MeshGenerator only ever receives a snapshot
// built by Document::toSnapshot (application/sources/document.cc, Document::generateMesh), which
// writes every numeric field (canvas originX/Y/Z, node x/y/z/radius, part settings) with
// String::fromDouble. A raw XML value that is not a number makes String::toDouble throw
// std::invalid_argument / std::out_of_range — a state the generator cannot be given in the
// application — so those two exceptions (what() == "stod") reject the input (-1) instead of aborting. Every other
// exception, and every memory/UB error, still crashes. For the same reason the snapshot's
// node/edge/part/component ids (map keys) and edge from/to node ids must have the form
// Document::toSnapshot writes them in (dust3d::Uuid::toString, "{8-4-4-4-12}" hex):
// MeshGenerator slices them with fixed substr offsets (mesh_generator.cc, extractRaw).
#include <stdexcept>
#include <string>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <dust3d/base/object.h>
#include <dust3d/base/snapshot.h>
#include <dust3d/base/snapshot_xml.h>
#include <dust3d/mesh/mesh_generator.h>

static bool isUuidString(const std::string& s)
{
    if (s.size() != 38 || s.front() != '{' || s.back() != '}')
        return false;
    for (size_t i = 1; i < 37; ++i) {
        const char c = s[i];
        if (i == 9 || i == 14 || i == 19 || i == 24) {
            if (c != '-')
                return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

static bool idsLikeDocument(const dust3d::Snapshot& s)
{
    for (const auto* m : { &s.nodes, &s.edges, &s.parts, &s.components })
        for (const auto& it : *m)
            if (!isUuidString(it.first))
                return false;
    for (const auto& it : s.edges)
        for (const char* key : { "from", "to" }) {
            auto f = it.second.find(key);
            if (f != it.second.end() && !isUuidString(f->second))
                return false;
        }
    return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::vector<char> xml(data, data + size);
    xml.push_back('\0');
    auto* snapshot = new dust3d::Snapshot;
    dust3d::loadSnapshotFromXmlString(snapshot, xml.data());
    if (!idsLikeDocument(*snapshot)) {
        delete snapshot;
        return -1;
    }
    dust3d::MeshGenerator generator(snapshot); // takes ownership of snapshot
    try {
        generator.generate();
    } catch (const std::logic_error& e) {
        // Only String::toDouble's number-parse failures (what() == "stod"); rethrow the rest
        // (e.g. a std::out_of_range from .at() is a real bug).
        const bool numberParse = (dynamic_cast<const std::invalid_argument*>(&e) || dynamic_cast<const std::out_of_range*>(&e))
            && std::string(e.what()) == "stod";
        if (!numberParse)
            throw;
        return -1;
    }
    delete generator.takeObject();
    return 0;
}
