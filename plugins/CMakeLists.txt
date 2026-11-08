cet_build_plugin(SuperaWriter art::module BASENAME_ONLY
  LIBRARIES PRIVATE larsoft2supera::spacepoint supera::processing larsoft2supera::optical larsoft2supera::auxiliary larsoft2supera::generator art::Framework_Core
  art::Framework_Principal canvas::canvas fhiclcpp::fhiclcpp
  supera::larcv2 messagefacility::MF_MessageLogger)
if(BUILD_TESTING)
  cet_build_plugin(OpticalFixture art::module BASENAME_ONLY NO_INSTALL
    SOURCE ../tests/OpticalFixture_module.cc
    LIBRARIES PRIVATE art::Framework_Core art::Framework_Principal
    lardataobj::RecoBase sbnobj::Common_CRT nusimdata::SimulationBase)
endif()
target_compile_options(SuperaWriter_module PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(BUILD_TESTING)
  cet_build_plugin(SpacePointFixture art::module BASENAME_ONLY NO_INSTALL
    SOURCE ../tests/SpacePointFixture_module.cc
    LIBRARIES PRIVATE art::Framework_Core art::Framework_Principal lardataobj::RecoBase canvas::canvas)
  cet_build_plugin(SpacePointCheck art::module BASENAME_ONLY NO_INSTALL
    SOURCE ../tests/SpacePointCheck_module.cc
    LIBRARIES PRIVATE larsoft2supera::spacepoint larsoft2supera::optical larsoft2supera::auxiliary larsoft2supera::generator art::Framework_Core art::Framework_Principal)
endif()
