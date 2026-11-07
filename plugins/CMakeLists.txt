cet_build_plugin(OpticalWriter art::module BASENAME_ONLY
  LIBRARIES PRIVATE larsoft2supera::optical art::Framework_Core
  art::Framework_Principal canvas::canvas fhiclcpp::fhiclcpp
  supera::larcv2)
if(BUILD_TESTING)
  cet_build_plugin(OpticalFixture art::module BASENAME_ONLY NO_INSTALL
    SOURCE ../tests/OpticalFixture_module.cc
    LIBRARIES PRIVATE art::Framework_Core art::Framework_Principal
    lardataobj::RecoBase)
endif()
target_compile_options(OpticalWriter_module PRIVATE -Wall -Wextra -Wpedantic -Werror)
