package com.rainey.frontend.domain

import com.rainey.frontend.data.entity.System

object DefaultSystems {
    val all = listOf(
        System("nes", "NES", "NES", 0, "8-BIT", "neon grid"),
        System("snes", "SNES", "SNES", 1, "16-BIT", "violet arcade"),
        System("genesis", "Genesis", "GEN", 2, "BLAST", "blue speed lines"),
        System("playstation", "PlayStation", "PS1", 3, "CD", "disc shimmer"),
        System("psp", "PSP", "PSP", 4, "WIDE", "portable glow"),
        System("dreamcast", "Dreamcast", "DC", 5, "SWIRL", "white console light"),
        System("arcade", "Arcade", "ARC", 6, "COIN", "cabinet lights"),
        System("gb", "Game Boy", "GB", 7, "8-BIT", "green pixel glow"),
        System("gba", "Game Boy Advance", "GBA", 8, "ADV", "handheld color"),
        System("n64", "Nintendo 64", "N64", 9, "64", "polygon haze"),
        System("ds", "Nintendo DS", "DS", 10, "DUAL", "dual screen glow"),
        System("android", "Android Games", "APK", 11, "PLAY", "native Android")
    )
}
