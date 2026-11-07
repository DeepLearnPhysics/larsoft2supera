# Legacy ICARUS processor inventory

This inventory describes the validated **ICARUS overlay configuration**, not every
possible LArSoft Supera configuration. Runtime order is not the proposed porting order.

## Authoritative baseline

- ICARUS: `v10_27_00`, commit `ee06f3022034c77fd1e9389e09277ea4709524e2`.
- SBN: `v10_27_00`, commit `9ad34f677588e7623c3159b23d639db585258549`.
- Supera: `9e7b533d44236c64ddc17748e92dbd9d2fbe0fae`, before v1.0.0.
- LArCV2: `v2_2_6`.
- art entry point: [stage1_run2_larcv_icarus_overlay.fcl][entry].
- LArCV process configuration: [supera_icarus_MC_all_cryo_PMT_CRT_overlay.fcl][config].

There are two scheduling layers. art reconstructs hits and space points, runs
`mcreco`, and invokes `LArSoftSuperaDriver` through the `superaMC` analyzer. The
driver then runs the following LArCV ProcessDriver sequence. `mcreco` and
cluster3D are prerequisites, not entries in this processor list.

## Processors in execution order

There are **16 top-level instances**, with **13 distinct processor types**.

| Order | Instance (type when different) | Configured behavior | Proposed responsibility |
| --- | --- | --- | --- |
| 1 | `MultiPartVrtx` (`SuperaMCTruth`) | Read `generator` and optional-in-practice `cosmgen` MCTruth; emit interaction/neutrino and generator-particle records under `mpv`. | Adapter extraction; shared representation and explicit selection policy. |
| 2 | `SuperaBBoxInteraction` | Establish the common fixed 3D grid and assign metadata to configured products. | Detector/configuration input in the adapter; grid semantics in shared code. |
| 3 | `SuperaMCParticleCluster` | Read `simplemerge` MCParticles and `mcreco` tracks/showers; build particle groups, energy clusters, semantic labels, and diagnostic products. Uses nested true-to-reco matching. | Split LArSoft extraction from shared grouping and labeling. Hardest migration stage. |
| 4 | `SuperaSimEnergyDeposit` | Voxelize `filtersed` SimEnergyDepositLite into `sed`, using the already-produced particle groups and `mcreco` shower daughters. | Adapter reads deposits; shared code owns grouping and aggregation. |
| 5 | `SuperaSpacePointCryoE` (`SuperaSpacePoint`) | Read east cluster3D points and hit associations; produce reconstructed charge and auxiliary tensors. | Adapter extracts points/hits; shared code handles voxel collisions and selection. |
| 6 | `SuperaSpacePointCryoW` (`SuperaSpacePoint`) | The corresponding west-cryostat conversion. | Same implementation, configured for another input collection. |
| 7 | `Tensor3DFromCluster3D` | Convert `pcluster` and `sed` cluster collections into sparse tensors, retaining input voxel values (`PITypeList: [1,1]`). | Shared tensor transformation. |
| 8 | `CombineTensor3DGhost` (`CombineTensor3D`) | Combine east/west reconstructed tensors into `pcluster_semantics_ghost`, with min pooling (`PoolType: 0`). | Shared tensor transformation. |
| 9 | `ThresholdTensor3D` | Paint the reconstructed voxel support with semantic value `5`, the configured ghost value. | Shared labeling policy. |
| 10 | `CombineTensor3D` | Combine the painted tensor with `pcluster_semantics`, again with min pooling, to form the final semantic/ghost tensor. | Shared labeling policy; explicitly test overlap behavior. |
| 11 | `ParticleCorrector` | Derive `corrected` particle records from `pcluster` particles and `pcluster_highE` clusters; includes cluster-based endpoint correction. Exact changed fields/defaults need review. | Shared post-processing. |
| 12 | `EmptyTensorFilter` | Require at least one voxel in `pcluster_semantics_ghost`. Filtering is enabled; rejection can prevent subsequent processors from running. | Pipeline selection policy. |
| 13 | `RescaleChargeTensor3DCryoE` (`RescaleChargeTensor3D`) | Use three planes of hit keys/charges and the `pcluster` reference to produce `reco_cryoE_rescaled`. Review shared-hit charge apportionment. | Shared post-processing using adapter-provided associations. |
| 14 | `RescaleChargeTensor3DCryoW` (`RescaleChargeTensor3D`) | The corresponding west-cryostat charge rescaling. | Same shared implementation, separate inputs. |
| 15 | `SuperaOptical` | Translate `opflashCryoE` and `opflashCryoW` into flash records: times, photoelectrons, positions/widths, flags, wire information, and collection-local IDs. | Adapter extraction; independent output serialization. |
| 16 | `SuperaCRT` | Translate `overlayCRTHit` into CRT records, including timestamps, photoelectrons, detector identifiers, positions, and uncertainties. | Experiment-specific extraction behind a shared record contract. |

The LArCV2 installed headers identify `PoolType: 0` as min pooling and `PIType: 1`
as input-voxel values. Full collision, in-place update, correction, and charge
rescaling behavior still needs implementation-level review and small tests.
Configuration names alone are not sufficient specifications.

### Nested processor: SuperaTrue2RecoVoxel3D

This runs inside `SuperaMCParticleCluster`; it is not a seventeenth top-level entry.
It connects `merge` SimChannels with both cluster3D point collections and their
hit associations. The baseline enables twofold matching, true positions, and
post-averaging. It uses a 100-electron threshold, a 15-tick window, a 3-sigma
window setting, a 0.425 cm post-averaging threshold, and a voxel-distance setting
of 3.0. Units and interactions between cuts must be checked against the code.

The adapter must expose the relevant evidence and detector context; the matching
policy needs a separately reviewed home. Do not hide this algorithm in a reader.

## Details that matter for the port

- The grid has bottom corner `[-412.89, -181.86, -894.951]` cm, side lengths
  `[1843.2, 1843.2, 1843.2]` cm, and `[0.3, 0.3, 0.3]` cm voxels.
- `SuperaMCParticleCluster` sets both `UseSimEnergyDeposit` and
  `UseSimEnergyDepositLite` to false. Its main filling path uses SimChannels.
  The separate `SuperaSimEnergyDeposit` processor uses SEDLite. These are
  different inputs and cannot be substituted silently.
- The particle semantic priority is `[1,2,0,3,4]`; the configuration identifies
  labels 0 through 4 as shower, track, Michel, delta, and low-energy scattering.
- Space-point charge selection is configured as `[-1000,50000]`. Wire information
  is enabled. Hit amplitude/time/RMS/multiplicity, hit count, and occupancy outputs
  are dropped; hit keys and charges remain for rescaling.
- `StoreAbsTime: true` is configured for SEDLite, but the selected source helper
  fills only energy clusters. See [F-002](findings.md#f-002-sedlite-absolute-time-output).
- This input has `generator` MCTruth and no `cosmgen` product. The MCTruth
  processor reports a missing handle and continues. Missing and empty products
  must remain distinguishable in the new contract.
- Real-data activity in overlay has no complete simulation ancestry. A ghost
  label describes this pipeline's matching result; it is not proof that a
  reconstructed point is physically spurious.

## Source entry points

The [pinned Supera tree][supera] contains `SuperaMCTruth.cxx`,
`SuperaBBoxInteraction.cxx`, `SuperaMCParticleCluster.cxx`,
`SuperaTrue2RecoVoxel3D.cxx`, `SuperaSimEnergyDeposit.cxx`,
`SuperaSpacePoint.cxx`, `SuperaOptical.cxx`, and `SuperaCRT.cxx`.
The remaining processor implementations belong to LArCV2, not this Supera tree.
For review, use the installed `v2_2_6` interfaces and the corresponding source
revision; do not silently inspect a newer LArCV implementation.

These ownership assignments are proposals. The current Atomic
[event model][atomic-event] contains particle inputs and unassociated deposits;
it does not provide a complete schema for reconstructed hits, flashes, and CRT
records. Establish that interface explicitly before implementing conversions.

[entry]: https://github.com/SBNSoftware/icaruscode/blob/ee06f3022034c77fd1e9389e09277ea4709524e2/fcl/reco/Stage1/overlay/stage1_run2_larcv_icarus_overlay.fcl
[config]: https://github.com/SBNSoftware/icaruscode/blob/ee06f3022034c77fd1e9389e09277ea4709524e2/fcl/supera/supera_icarus_MC_all_cryo_PMT_CRT_overlay.fcl
[supera]: https://github.com/DeepLearnPhysics/Supera/tree/9e7b533d44236c64ddc17748e92dbd9d2fbe0fae
[atomic-event]: https://github.com/DeepLearnPhysics/SuperaAtomic/blob/a2953b373abd4ad8cb3bf57c027d6910acdca72f/src/supera/data/Event.h
