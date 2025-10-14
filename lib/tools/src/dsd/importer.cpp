#include "daltools/dsd/importer.hpp"

#include <sung/basic/byte_arr.hpp>


namespace {

    dal::dsd::Header& to_header(std::vector<uint8_t>& data) {
        return *reinterpret_cast<dal::dsd::Header*>(data.data());
    }

    void enlarge(std::vector<uint8_t>& data, size_t size) {
        data.resize(data.size() + size);
    }

}  // namespace


namespace dal::dsd {

    bool convert(
        std::vector<uint8_t>& output, const dal::SceneIntermediate& scene
    ) {
        {
            output.resize(sizeof(Header));
        }

        {
            const auto loc = output.size();
            ::enlarge(output, scene.meshes_.size() * sizeof(Mesh));

            auto& header = ::to_header(output);
            header.meshes_.byte_offset_ = loc;
            header.meshes_.item_count_ = scene.meshes_.size();
        }

        {
            const auto loc = output.size();
            ::enlarge(output, scene.materials_.size() * sizeof(Material));

            auto& header = ::to_header(output);
            header.materials_.byte_offset_ = loc;
            header.materials_.item_count_ = scene.materials_.size();
        }

        {
            const auto loc = output.size();
            ::enlarge(output, scene.mesh_actors_.size() * sizeof(MeshActor));

            auto& header = ::to_header(output);
            header.mesh_actors_.byte_offset_ = loc;
            header.mesh_actors_.item_count_ = scene.mesh_actors_.size();
        }

        {
            auto& header = ::to_header(output);
            header.set_magic();
            header.version_ = 1;
            header.transform_ = scene.root_transform_;
        }

        return false;
    }

}  // namespace dal::dsd
