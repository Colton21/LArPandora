include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( $ENV{PANDORA_INC} )
include_directories( $ENV{LARPANDORACONTENT_INC} )

art_make( 
          LIB_LIBRARIES larcore_Geometry
                        larcore_Geometry_Geometry_service
                        larsim_Simulation lardataobj_Simulation
                        lardataobj_RawData
                        lardataobj_RecoBase
                        lardata_Utilities
                        larreco_RecoAlg_ClusterRecoUtil
                        larreco_RecoAlg
                        larreco_ClusterFinder
                        larreco_Calorimetry
                        ${PANDORASDK}
                        ${PANDORAMONITORING}
                        LArPandoraContent
                        larpandora_LArPandoraShowers
                        nusimdata_SimulationBase
                        ${ART_FRAMEWORK_CORE}
                        ${ART_FRAMEWORK_PRINCIPAL}
                        ${ART_FRAMEWORK_SERVICES_REGISTRY}
                        ${ART_FRAMEWORK_SERVICES_BASIC}
                        ${ART_FRAMEWORK_SERVICES_OPTIONAL}
                        ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
                        art_Persistency_Common
                        art_Persistency_Provenance
                        art_Utilities
                        canvas
                        ${MF_MESSAGELOGGER}
                        ${MF_UTILITIES}
                        ${FHICLCPP}
                        cetlib cetlib_except
                        ${Boost_SYSTEM_LIBRARY}
                        ${ROOT_GEOM}
                        ${ROOT_BASIC_LIB_LIST}
                        MODULE_LIBRARIES larpandora_LArPandoraInterface
          )

install_headers()
install_fhicl()
install_source()

add_subdirectory(scripts)

