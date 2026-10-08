// SPDX-License-Identifier: GPL-3.0-or-later
// What a setting's row shows (Game options, Settings › Graphics;
// docs/design/2026-09-28-gamepad-ui/GameOptions.dc.html): "Default · 720p"
// while it follows the level above it (a game follows Settings, Settings
// follows Playport's defaults), the value alone with the changed mark once it
// is set there. Y puts it back: the value becomes nil again.

public struct OptionValue: Equatable, Sendable {
    public var text: String
    /// Set at this level: the row's amber dot, and Y has something to reset.
    public var changed: Bool

    public init(text: String, changed: Bool) {
        self.text = text
        self.changed = changed
    }

    /// The value comes from the game's compatibility default (CompatDefaults): the row's marker.
    public var compat = false

    /// `own` is this level's value (nil follows), `compat` the game's compatibility default
    /// (nil for none), `inherited` what both follow.
    public static func of<T>(own: T?, compat: T? = nil, inherited: T, name: (T) -> String) -> OptionValue {
        if let own { return OptionValue(text: name(own), changed: true) }
        if let compat {
            var v = OptionValue(text: "\(compatMarker) For this game · " + name(compat), changed: false)
            v.compat = true
            return v
        }
        return OptionValue(text: "Default · " + name(inherited), changed: false)
    }

    /// The marker before a value set for the game's compatibility.
    public static let compatMarker = "ⓘ"
}
