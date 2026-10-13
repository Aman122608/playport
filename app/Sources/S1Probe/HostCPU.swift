// SPDX-License-Identifier: GPL-3.0-or-later
// The phone's CPU features as the kernel reports them (sysctl
// hw.optional.arm.FEAT_*). FEX cannot read the ID registers from EL0 on iOS
// and builds its feature set by hand (patches/fex-port 0009), so a launch
// passes it what the kernel reports (LaunchCoordinator,
// FEXProfile.hostFeatures): a `disable<x>` for each feature this CPU lacks
// (an A12 has no FlagM, FlagM2, AFP or LRCPC2), and `enablelrcpc2` for
// FEAT_LRCPC2, whose LDAPUR and STLUR let a TSO access keep its offset
// (patches/fex 0008).

import Darwin
import PlayportKit

enum HostCPU {
    /// One feature: nil when this kernel has no such sysctl.
    static func feature(_ name: String) -> Bool? {
        var value: Int32 = 0
        var size = MemoryLayout<Int32>.size
        guard sysctlbyname("hw.optional.arm." + name, &value, &size, nil, 0) == 0 else { return nil }
        return value != 0
    }

    static let features = FEXProfile.HostCPUFeatures(
        flagM: feature("FEAT_FlagM"), flagM2: feature("FEAT_FlagM2"), afp: feature("FEAT_AFP"),
        lse: feature("FEAT_LSE"), lrcpc: feature("FEAT_LRCPC"), lrcpc2: feature("FEAT_LRCPC2"),
        fcma: feature("FEAT_FCMA"), aes: feature("FEAT_AES"), pmull: feature("FEAT_PMULL"),
        sha1: feature("FEAT_SHA1"), sha256: feature("FEAT_SHA256"), crc32: feature("FEAT_CRC32"))

    /// `FEAT_FlagM=1 FEAT_AFP=1 ...`, `-` for a sysctl the kernel lacks.
    static let summary: String = ["FEAT_FlagM", "FEAT_FlagM2", "FEAT_AFP", "FEAT_LSE", "FEAT_LRCPC", "FEAT_LRCPC2", "FEAT_FCMA",
                                  "FEAT_AES", "FEAT_PMULL", "FEAT_SHA1", "FEAT_SHA256", "FEAT_CRC32", "FEAT_LRCPC3", "FEAT_LSE2"].map { name in
        "\(name)=\(feature(name).map { $0 ? "1" : "0" } ?? "-")"
    }.joined(separator: " ")
}
