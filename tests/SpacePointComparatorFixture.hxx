#include "larcv/core/DataFormat/EventVoxel3D.h"

namespace supera_sparse_fixture {
void write(const std::string &path, const std::string &mutation) {
  larcv::IOManager writer(larcv::IOManager::kWRITE);
  writer.set_out_file(path);
  if (!writer.initialize())
    throw std::runtime_error("Cannot initialize comparator fixture");
  writer.set_id(1, 2, mutation == "event" ? 4 : 3);
  for (const auto &side : {std::string("E"), std::string("W")}) {
    for (const auto &suffix : {"", "_chi2", "_cryo", "_tpc", "_hit_charge0", "_hit_charge1",
                               "_hit_charge2", "_hit_key0", "_hit_key1", "_hit_key2"}) {
      const auto name = "reco_cryo" + side + suffix;
      if (mutation == "missing" && name == "reco_cryoE")
        continue;
      larcv::Voxel3DMeta meta;
      meta.set(0, 0, 0, mutation == "bounds" ? 3 : 2, 2, 2, mutation == "counts" ? 3 : 2, 2, 2);
      larcv::VoxelSet voxels;
      voxels.emplace(mutation == "id" ? 1 : 0, mutation == "value" ? 2.F : 1.F, false);
      writer.get_data<larcv::EventSparseTensor3D>(name).emplace(std::move(voxels), meta);
    }
  }
  if (mutation == "extra")
    writer.get_data<larcv::EventSparseTensor3D>("unexpected");
  if (!writer.save_entry())
    throw std::runtime_error("Cannot write comparator fixture");
  if (mutation == "entries") {
    writer.set_id(1, 2, 4);
    writer.save_entry();
  }
  writer.finalize();
}
} // namespace supera_sparse_fixture
