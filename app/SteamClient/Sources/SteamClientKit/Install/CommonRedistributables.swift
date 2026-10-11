// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// Steamworks Common Redistributables (app 228980): the shared depots of DirectX, VC++, .NET,
/// OpenAL and PhysX installers that a game lists as `depotfromapp` 228980 and Steam installs once
/// into `Steamworks Shared`, each depot with its own `installscript.vdf`. Playport downloads the
/// ones a game lists into the same one folder, shared by every game, as Steam does (decision 0081).
public enum CommonRedistributables {
    public static let appID: UInt32 = 228980
    /// The folder under games, as Steam names it (`config/installdir`).
    public static let installDir = "Steamworks Shared"
}

extension InstallReceipt {
    /// The depots of app 228980 the title lists, from its record's skipped depots.
    public var commonRedistributableDepots: [UInt32] {
        skippedDepots.compactMap { DepotSelection.sharedInstall(skipped: $0) }
            .filter { $0.appID == CommonRedistributables.appID }.map(\.depotID)
    }
}

extension TitleInstaller {
    /// The install-script files of an install, by depot: the files its retained manifests flag
    /// (DepotFileFlag.installScript), as Steam lists them in its app manifest's InstallScripts.
    /// Paths are relative to the install folder, `/`-separated. Empty with no record or manifests.
    public func installScripts(appID: UInt32) -> [(depotID: UInt32, path: String)] {
        guard let receipt = loadReceipt(appID) else { return [] }
        var out: [(depotID: UInt32, path: String)] = []
        for ref in receipt.depots {
            guard let m = try? RetainedManifest.load(layout.manifestFile(ref)) else { continue }
            for f in m.files where f.isInstallScript { out.append((ref.depotID, f.path)) }
        }
        return out
    }

    /// Of `depots` (app 228980's), those Playport's install of it does not hold.
    public func missingCommonRedistributables(_ depots: [UInt32]) -> [UInt32] {
        let have = Set(loadReceipt(CommonRedistributables.appID)?.depots.map(\.depotID) ?? [])
        return depots.filter { !have.contains($0) }
    }

    /// Adds `depots` of app 228980 to `Games/Steamworks Shared`: an install of exactly the depots it
    /// held plus these, so a depot another game needed stays. Already there, nothing is fetched.
    public func installCommonRedistributables(_ depots: [UInt32], engine: InstallEngine.Options = .init(),
                                              say: @escaping @Sendable (String) -> Void) async throws -> Outcome? {
        guard !missingCommonRedistributables(depots).isEmpty else { return nil }
        let have = loadReceipt(CommonRedistributables.appID)?.depots.map(\.depotID) ?? []
        let wanted = Array(Set(have + depots)).sorted()
        return try await install(.init(appID: CommonRedistributables.appID, selection: .init(explicit: wanted),
                                       destName: CommonRedistributables.installDir, branch: Branch.publicName, engine: engine),
                                 say: say)
    }
}
