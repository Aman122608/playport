// SPDX-License-Identifier: GPL-3.0-or-later
// The player's launch settings (Settings tab and a game's page): the guest's
// screen, a frame rate limit, the graphics backend and, for one game, extra
// command-line arguments, which steam_api it loads, FEX's x86 memory
// ordering over the game's profile, FEX's block size and x87 precision (FEXProfile).
// A game's own value wins over the global one, which wins over the defaults:
// 720 rows at 60 FPS on DXMT (Vulkan for i386 and detected DX12 games; `defaultScreen`, `defaultFrameLimit`,
// GraphicsBackend.default). A cohort title's screen (LaunchPlan.screen) no
// longer sets it (decision 0034). Native and no limit are a
// choice in Settings or on a game's page. A limit of 0 runs presents free, with
// no vsync lock (TitleScreen.swift).

import Foundation
import SteamClientKit

public struct LaunchSettings: Codable, Equatable, Sendable {
    /// A screen spec (LaunchPlan.validScreen): its aspect ratio and resolution (ScreenAspect); nil inherits.
    public var screen: String?
    /// Frames per second at most; 0 for no limit, nil inherits.
    public var frameLimit: Int?
    /// Which layer runs Direct3D; nil inherits.
    public var graphics: GraphicsBackend?
    /// Added to the game's command line, after any the tested configuration
    /// gives it (per game only), as the player typed them: split by
    /// LaunchSettings.splitArguments. `-DX12`, `-force-d3d12` and the like.
    public var arguments: String
    /// The Steam API emulator or the game's own steam_api (per game only); nil
    /// is the emulator (SteamAPISwap).
    public var steamAPI: SteamAPISwap.Mode?
    /// Sync the game's saves with Steam Cloud (per game only); nil is on.
    public var cloudSync: Bool?
    /// FEX's memory ordering, each switch over the game's profile (per game
    /// only; FEXProfile.launch).
    public var ordering: MemoryOrdering
    /// The most x86 instructions FEX translates as one block (FEX `MaxInst`,
    /// per game only); nil takes the game's profile, Proton's global 500 unless
    /// its entry sets one (decision 0055). FEXProfile.blockSizes are the ones offered.
    public var maxInst: Int?
    /// x87 at 64-bit rather than 80-bit precision (FEX `X87ReducedPrecision`, per
    /// game only); nil takes the game's profile (FEXProfile.defaultX87Reduced).
    public var x87Reduced: Bool?
    /// FEX's disk cache of translated code (FEX `DiskCache`, per game only); nil
    /// takes the default, off for now (FEXProfile.defaultDiskCache, decision 0056).
    public var diskCache: Bool?
    /// madeira.cfg keys over the game's own (per game only; a dev build's), as typed:
    /// `key=value` items split at spaces (`vram-mb=1024 totalphys=6144 inproc-sync=1`).
    /// LaunchSettings.runtimeKeys parses them; TitleConfig writes them after the title's.
    public var runtime: String
    /// The Vulkan layers' options (per game only; a dev build's, decision 0060), as typed:
    /// items split at spaces, each `NAME=value` for an allowlisted variable
    /// (LaunchSettings.graphicsVariables) or a DXVK option (`dxvk.tilerMode=False`), which
    /// goes into DXVK_CONFIG. LaunchSettings.graphicsEnvironment parses them.
    public var graphicsOptions: String

    public init(screen: String? = nil, frameLimit: Int? = nil, graphics: GraphicsBackend? = nil,
                arguments: String = "", steamAPI: SteamAPISwap.Mode? = nil,
                cloudSync: Bool? = nil, ordering: MemoryOrdering = MemoryOrdering(), maxInst: Int? = nil,
                x87Reduced: Bool? = nil, diskCache: Bool? = nil, runtime: String = "", graphicsOptions: String = "") {
        self.screen = screen
        self.frameLimit = frameLimit
        self.graphics = graphics
        self.arguments = arguments
        self.steamAPI = steamAPI
        self.cloudSync = cloudSync
        self.ordering = ordering
        self.maxInst = maxInst
        self.x87Reduced = x87Reduced
        self.diskCache = diskCache
        self.runtime = runtime
        self.graphicsOptions = graphicsOptions
    }

    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        screen = try c.decodeIfPresent(String.self, forKey: .screen)
        frameLimit = try c.decodeIfPresent(Int.self, forKey: .frameLimit)
        // A backend this build does not know (saved by a newer one) inherits
        // rather than losing the rest of the settings.
        graphics = try c.decodeIfPresent(String.self, forKey: .graphics).flatMap(GraphicsBackend.init(rawValue:))
        arguments = try c.decodeIfPresent(String.self, forKey: .arguments) ?? ""
        steamAPI = try c.decodeIfPresent(String.self, forKey: .steamAPI).flatMap(SteamAPISwap.Mode.init(rawValue:))
        cloudSync = try c.decodeIfPresent(Bool.self, forKey: .cloudSync)
        ordering = try c.decodeIfPresent(MemoryOrdering.self, forKey: .ordering) ?? MemoryOrdering()
        maxInst = try c.decodeIfPresent(Int.self, forKey: .maxInst)
        x87Reduced = try c.decodeIfPresent(Bool.self, forKey: .x87Reduced)
        diskCache = try c.decodeIfPresent(Bool.self, forKey: .diskCache)
        runtime = try c.decodeIfPresent(String.self, forKey: .runtime) ?? ""
        graphicsOptions = try c.decodeIfPresent(String.self, forKey: .graphicsOptions) ?? ""
    }

    public var isEmpty: Bool {
        screen == nil && frameLimit == nil && graphics == nil
            && arguments.trimmingCharacters(in: .whitespaces).isEmpty && steamAPI == nil && cloudSync == nil
            && ordering.isEmpty && maxInst == nil && x87Reduced == nil && diskCache == nil
            && runtime.trimmingCharacters(in: .whitespaces).isEmpty
            && graphicsOptions.trimmingCharacters(in: .whitespaces).isEmpty
    }

    /// Arguments as a command line splits them: at spaces, except inside
    /// double quotes, which are removed.
    public static func splitArguments(_ line: String) -> [String] {
        var out: [String] = [], cur = "", quoted = false, any = false
        for ch in line {
            if ch == "\"" { quoted.toggle(); any = true }
            else if ch == " " && !quoted { if any { out.append(cur) }; cur = ""; any = false }
            else { cur.append(ch); any = true }
        }
        if any { out.append(cur) }
        return out
    }

    /// Runtime keys as typed (`vram-mb=1024 inproc-sync=1`): `key=value` items at spaces,
    /// a later item for a key winning. nil when an item has no `=` or TitleConfig refuses it.
    public static func runtimeKeys(_ line: String) -> [String: String]? {
        var out: [String: String] = [:]
        for item in splitArguments(line) {
            guard let eq = item.firstIndex(of: "=") else { return nil }
            out[String(item[..<eq])] = String(item[item.index(after: eq)...])
        }
        return (try? TitleConfig.validate(out)) == nil ? nil : out
    }

    /// The variables a game's Graphics options may set (decision 0060): DXVK's and
    /// vkd3d-proton's option strings and KosmicKrisp's debug, experimental and
    /// workaround switches. Nothing else reaches a game's environment this way.
    public static let graphicsVariables = ["DXVK_CONFIG", "VKD3D_CONFIG", "MESA_KK_DEBUG",
                                           "MESA_KK_EXPERIMENTAL", "MESA_KK_DISABLE_WORKAROUNDS"]
    /// The sections of a DXVK option an item may name without DXVK_CONFIG= (dxvk.conf's).
    public static let dxvkSections = ["dxvk", "dxgi", "d3d11", "d3d10", "d3d9", "d3d8"]

    /// Graphics options as typed (`dxvk.tilerMode=False VKD3D_CONFIG=one_time_submit`):
    /// items at spaces (double quotes keep spaces), each `NAME=value` for a variable in
    /// graphicsVariables or `section.option=value` for DXVK. DXVK options join DXVK_CONFIG
    /// with `;`, as DXVK splits it; a repeated other variable joins with `,`, as vkd3d-proton
    /// and Mesa split theirs. nil when an item is neither, or names another variable.
    public static func graphicsEnvironment(_ line: String) -> [String: String]? {
        var out: [String: String] = [:]
        func add(_ name: String, _ value: String) {
            guard !value.isEmpty else { return }
            out[name] = out[name].map { $0 + (name == "DXVK_CONFIG" ? ";" : ",") + value } ?? value
        }
        for item in splitArguments(line) {
            guard let eq = item.firstIndex(of: "="), eq != item.startIndex else { return nil }
            let name = String(item[..<eq]), value = String(item[item.index(after: eq)...])
            if graphicsVariables.contains(name) {
                add(name, value)
            } else if let dot = name.firstIndex(of: "."), dxvkSections.contains(String(name[..<dot])),
                      name.index(after: dot) < name.endIndex {
                add("DXVK_CONFIG", "\(name) = \(value)")
            } else {
                return nil
            }
        }
        return out
    }

    /// What one launch uses.
    public struct Effective: Equatable, Sendable {
        /// nil for the panel's native pixels.
        public var screen: String?
        /// 0 for no limit.
        public var frameLimit: Int
        public var graphics: GraphicsBackend
        public var arguments: [String]
        public var steamAPI: SteamAPISwap.Mode
        /// The game's own ordering switches; FEXProfile.launch puts them over its profile.
        public var ordering: MemoryOrdering
        /// The game's own block size, nil for its profile's (FEXProfile.launch).
        public var maxInst: Int?
        /// The game's own x87 precision, nil for its profile's (FEXProfile.launch).
        public var x87Reduced: Bool?
        /// The game's own disk cache choice, nil for the default (FEXProfile.launch).
        public var diskCache: Bool?
        /// madeira.cfg keys over the title's own (config(over:)); empty for none.
        public var runtime: [String: String]
        /// The Vulkan layers' variables from the game's Graphics options; empty for none.
        public var graphicsEnvironment: [String: String]

        public init(screen: String?, frameLimit: Int, graphics: GraphicsBackend = .default,
                    arguments: [String] = [], steamAPI: SteamAPISwap.Mode = .emulated,
                    ordering: MemoryOrdering = MemoryOrdering(), maxInst: Int? = nil,
                    x87Reduced: Bool? = nil, diskCache: Bool? = nil, runtime: [String: String] = [:],
                    graphicsEnvironment: [String: String] = [:]) {
            self.screen = screen
            self.frameLimit = frameLimit
            self.graphics = graphics
            self.arguments = arguments
            self.steamAPI = steamAPI
            self.ordering = ordering
            self.maxInst = maxInst
            self.x87Reduced = x87Reduced
            self.diskCache = diskCache
            self.runtime = runtime
            self.graphicsEnvironment = graphicsEnvironment
        }

        /// The title's madeira.cfg keys (its cohort entry's) with these over them.
        public func config(over base: [String: String]) -> [String: String] {
            base.merging(runtime) { _, own in own }
        }
    }

    /// A screen and a limit nobody set: 720 rows at 60 FPS. On the reference phone
    /// Hollow Knight holds 60 there for ten minutes at 2.8 W for the whole phone,
    /// with no thermal pressure, where native and free-running fall to 76 FPS on the
    /// E cores (docs/evidence/2026-09-29-hk-gameplay-baseline.md).
    public static let defaultScreen = "720"
    public static let defaultFrameLimit = 60

    /// The game's value, else the global one, else the defaults (720 rows,
    /// 60 FPS, Vulkan for an i386 executable (decision 0047) or DX12 (0044), DXMT
    /// otherwise). Explicit graphics choices still win. `arguments` are the title's
    /// base arguments, before the player's; they do not move an i386 title off Vulkan,
    /// since DXMT has no i386 build. Invalid values are skipped as if unset.
    public static func resolve(game: LaunchSettings?, global: LaunchSettings,
                               importsDirect3D12: Bool = false, i386: Bool = false,
                               arguments: [String] = []) -> Effective {
        let screen = [game?.screen, global.screen, defaultScreen].lazy.compactMap { $0 }
            .first(where: LaunchPlan.validScreen)
        let limit = [game?.frameLimit, global.frameLimit].lazy.compactMap { $0 }.first { $0 >= 0 } ?? defaultFrameLimit
        let dx12 = Direct3D12.requested(arguments: arguments + splitArguments(game?.arguments ?? ""),
                                       imported: importsDirect3D12)
        let graphics = game?.graphics ?? global.graphics ?? (dx12 || i386 ? .vulkan : .default)
        return Effective(screen: screen, frameLimit: limit, graphics: graphics,
                         arguments: splitArguments(game?.arguments ?? ""), steamAPI: game?.steamAPI ?? .emulated,
                         ordering: game?.ordering ?? MemoryOrdering(),
                         maxInst: game?.maxInst.flatMap { FEXProfile.validBlockSize($0) ? $0 : nil },
                         x87Reduced: game?.x87Reduced, diskCache: game?.diskCache,
                         runtime: runtimeKeys(game?.runtime ?? "") ?? [:],
                         graphicsEnvironment: graphicsEnvironment(game?.graphicsOptions ?? "") ?? [:])
    }

    /// The limits the pickers offer for a panel refreshing at most `maxRate` times a
    /// second: the rates that divide it evenly, so each frame stays up equally long.
    public static func frameLimits(maxRate: Int) -> [Int] {
        [30, 40, 60].filter { $0 < maxRate && maxRate % $0 == 0 }
    }
}

/// The layer that turns a game's Direct3D into Metal (decision 0014).
public enum GraphicsBackend: String, Codable, CaseIterable, Sendable {
    /// DXMT: Direct3D 10 and 11 straight to Metal. What every tested game runs on.
    case dxmt
    /// DXVK (Direct3D 8 to 11) and vkd3d-proton (Direct3D 12) on Vulkan, on
    /// KosmicKrisp, Mesa's Vulkan driver on Metal.
    case vulkan

    public static let `default`: GraphicsBackend = .dxmt

    /// The directory under the runtime whose <arch>-windows builtins go
    /// before the runtime's own for this backend (WINEDLLPATH); nil for none.
    public var runtimeOverlay: String? {
        switch self {
        case .dxmt: nil
        case .vulkan: "vulkan"
        }
    }

    /// Environment this backend's launches start with.
    ///
    /// None for DXMT. Vulkan turns DXVK's tiler mode off (decision 0066): on
    /// KosmicKrisp it recorded each render pass into a secondary command buffer
    /// that the driver copied and replayed on DXVK's frame thread, for no GPU time
    /// saved. DXVK still picks its own compiler thread count (decision 0024).
    public var runtimeEnvironment: [String: String] {
        switch self {
        case .dxmt: [:]
        case .vulkan: ["DXVK_CONFIG": "dxvk.tilerMode = False"]
        }
    }

    /// The backend's environment with a game's Graphics options over it (decision
    /// 0060): an option's variable replaces the backend's, except DXVK_CONFIG, where
    /// the options follow the backend's after `;`, so DXVK takes an option's value for
    /// a key both name and keeps the backend's others.
    public func environment(graphicsOptions options: [String: String]) -> [String: String] {
        var env = runtimeEnvironment
        for (name, value) in options {
            if name == "DXVK_CONFIG", let own = env[name] {
                env[name] = own + ";" + value
            } else {
                env[name] = value
            }
        }
        return env
    }
}

/// The shape of a game's screen, the first of the screen setting's two choices; the
/// resolution within it is the second. Both are one screen spec in LaunchSettings.screen
/// (`native` and `<rows>` for Phone, `WxH` for the others), so a saved spec from before
/// the aspect ratio choice keeps its meaning: `720` is Phone at 720p. A screen of another
/// shape than the panel's is aspect-fitted, with bars at the sides or above and below
/// (HostIOKit Display.aspectFit). Some games draw nothing on a screen wider than 16:9
/// (Super Meat Boy, docs/evidence/2026-10-08-super-meat-boy.md).
public enum ScreenAspect: String, CaseIterable, Sendable {
    /// The panel's own shape (the default).
    case phone
    case wide16x9 = "16:9"
    case wide16x10 = "16:10"
    case standard4x3 = "4:3"

    /// The resolutions offered for this shape, most pixels first. Phone's follow the
    /// panel (rows at its aspect; the phone keeps those it can show).
    public var resolutions: [String] {
        switch self {
        case .phone: ["native", "1080", "900", "720", "540"]
        case .wide16x9: ["1920x1080", "1600x900", "1280x720", "960x540"]
        case .wide16x10: ["1920x1200", "1680x1050", "1440x900", "1280x800"]
        case .standard4x3: ["1600x1200", "1280x960", "1024x768", "800x600"]
        }
    }

    /// The resolution choosing this shape gives: the one nearest 720 rows (LaunchSettings.defaultScreen).
    public var defaultResolution: String {
        switch self {
        case .phone: LaunchSettings.defaultScreen
        case .wide16x9: "1280x720"
        case .wide16x10: "1280x800"
        case .standard4x3: "1024x768"
        }
    }

    /// The shape of a screen spec: nil (nothing set), `native` and `<rows>` are Phone, `4:3`
    /// (the runtime's old 1024×768) and a `WxH` of an offered shape are that shape. nil for a
    /// `WxH` of another shape (a custom size set as --settings JSON).
    public static func of(_ spec: String?) -> ScreenAspect? {
        guard let spec else { return .phone }
        let s = spec.lowercased()
        if s == "native" || Int(s) != nil { return .phone }
        if s == "4:3" { return .standard4x3 }
        let parts = s.split(separator: "x")
        guard parts.count == 2, let w = Int(parts[0]), let h = Int(parts[1]), w > 0, h > 0 else { return nil }
        return [ScreenAspect.wide16x9, .wide16x10, .standard4x3].first { a in
            let (aw, ah) = a.ratio
            return w * ah == h * aw
        }
    }

    /// The spec to store when a person picks this shape over `current` (a level's own spec):
    /// unchanged for the same shape, else the shape's default resolution. At the global level
    /// (`storesDefault` false) Phone's default is nil, Playport's 720p; a game stores it, so it
    /// overrides a global shape.
    public func spec(over current: String?, storesDefault: Bool) -> String? {
        if let current, ScreenAspect.of(current) == self { return current }
        return self == .phone && !storesDefault ? nil : defaultResolution
    }

    /// The spec to store for a resolution picked in this shape: a stepper's nil (its default)
    /// is nothing for Phone (Playport's 720p) and the default resolution for another shape,
    /// which must be stored to keep the shape.
    public func spec(resolution picked: String?) -> String? {
        picked ?? (self == .phone ? nil : defaultResolution)
    }

    /// A spec as the resolution pickers name it: `4:3` is their 1024x768.
    public static func canonical(_ spec: String?) -> String? {
        spec?.lowercased() == "4:3" ? "1024x768" : spec
    }

    private var ratio: (Int, Int) {
        switch self {
        case .phone: (0, 0)
        case .wide16x9: (16, 9)
        case .wide16x10: (16, 10)
        case .standard4x3: (4, 3)
        }
    }
}
