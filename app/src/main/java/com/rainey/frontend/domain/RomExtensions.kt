package com.rainey.frontend.domain

object RomExtensions {
    val bySystem = mapOf(
        "nes" to setOf("nes", "zip"),
        "snes" to setOf("sfc", "smc", "zip"),
        "genesis" to setOf("gen", "md", "bin", "zip"),
        "playstation" to setOf("cue", "bin", "chd", "iso", "pbp", "img", "ccd", "mdf", "mds"),
        "psp" to setOf("iso", "cso"),
        "dreamcast" to setOf("gdi", "chd", "cdi", "cue"),
        "arcade" to setOf("zip", "7z"),
        "gb" to setOf("gb", "gbc", "zip"),
        "gba" to setOf("gba", "zip"),
        "n64" to setOf("z64", "n64", "v64", "zip"),
        "ds" to setOf("nds", "zip", "7z"),
        "android" to emptySet()
    )
}
