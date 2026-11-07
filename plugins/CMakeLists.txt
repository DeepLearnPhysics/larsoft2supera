cet_build_plugin(SuperaWriter art::module BASENAME_ONLY
  LIBRARIES PRIVATE larsoft2supera::optical larsoft2supera::auxiliary larsoft2supera::generator art::Framework_Core
  art::Framework_Principal canvas::canvas fhiclcpp::fhiclcpp
  supera::larcv2)
if(BUILD_TESTING)
  cet_build_plugin(OpticalFixture art::module BASENAME_ONLY NO_INSTALL
    SOURCE ../tests/OpticalFixture_module.cc
    LIBRARIES PRIVATE art::Framework_Core art::Framework_Principal
    lardataobj::RecoBase sbnobj::Common_CRT nusimdata::SimulationBase)
endif()
target_compile_options(SuperaWriter_module PRIVATE -Wall -Wextra -Wpedantic -Werror)
