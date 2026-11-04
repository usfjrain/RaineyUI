package com.rainey.frontend.settings

import android.content.Context
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.rainey.frontend.domain.DefaultSystems
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore("rainey_settings")

data class UserSettings(
    val scanlinesEnabled: Boolean = true,
    val theme: String = "Neon Arcade",
    val borderStyle: String = BorderStyles.OFF,
    val nativeEmulationEnabled: Boolean = true,
    val touchControlsEnabled: Boolean = false,
    val touchControlsOpacity: Float = 0.72f,
    val controllerLayout: String = "Nintendo",
    val videoScale: Int = 0,
    val retroArchEnabled: Boolean = false,
    val retroArchPackageName: String = "com.retroarch",
    val retroArchCoresFolderUri: String = "",
    val biosFolderUri: String = "",
    val hiddenSystemIds: Set<String> = emptySet(),
    val retroArchCoreBySystem: Map<String, String> = emptyMap()
) {
    fun retroArchCoreFor(systemId: String): String = retroArchCoreBySystem[systemId].orEmpty()
    fun isSystemHidden(systemId: String): Boolean = systemId in hiddenSystemIds
}

class SettingsStore(private val context: Context) {
    private val scanlinesKey = booleanPreferencesKey("scanlines_enabled")
    private val themeKey = stringPreferencesKey("theme")
    private val borderStyleKey = stringPreferencesKey("border_style")
    private val nativeEmulationEnabledKey = booleanPreferencesKey("native_emulation_enabled")
    private val touchControlsEnabledKey = booleanPreferencesKey("touch_controls_enabled")
    private val touchControlsOpacityKey = floatPreferencesKey("touch_controls_opacity")
    private val controllerLayoutKey = stringPreferencesKey("controller_layout")
    private val videoScaleKey = intPreferencesKey("video_scale")
    private val retroArchEnabledKey = booleanPreferencesKey("retroarch_enabled")
    private val retroArchPackageKey = stringPreferencesKey("retroarch_package_name")
    private val retroArchCoresFolderKey = stringPreferencesKey("retroarch_cores_folder_uri")
    private val biosFolderKey = stringPreferencesKey("bios_folder_uri")
    private val hiddenSystemIdsKey = stringPreferencesKey("hidden_system_ids")

    val settings: Flow<UserSettings> = context.dataStore.data.map { preferences ->
        UserSettings(
            scanlinesEnabled = preferences[scanlinesKey] ?: true,
            theme = preferences[themeKey] ?: "Neon Arcade",
            borderStyle = preferences[borderStyleKey]?.takeIf { it in BorderStyles.options } ?: BorderStyles.OFF,
            nativeEmulationEnabled = preferences[nativeEmulationEnabledKey] ?: true,
            touchControlsEnabled = preferences[touchControlsEnabledKey] ?: false,
            touchControlsOpacity = preferences[touchControlsOpacityKey] ?: 0.72f,
            controllerLayout = preferences[controllerLayoutKey] ?: "Nintendo",
            videoScale = preferences[videoScaleKey] ?: 0,
            retroArchEnabled = preferences[retroArchEnabledKey] ?: false,
            retroArchPackageName = preferences[retroArchPackageKey] ?: "com.retroarch",
            retroArchCoresFolderUri = preferences[retroArchCoresFolderKey] ?: "",
            biosFolderUri = preferences[biosFolderKey] ?: "",
            hiddenSystemIds = parseHiddenSystemIds(preferences[hiddenSystemIdsKey].orEmpty()),
            retroArchCoreBySystem = DefaultSystems.all.associate { system ->
                system.id to (preferences[retroArchCoreKey(system.id)] ?: defaultCoreFor(system.id))
            }
        )
    }

    suspend fun setScanlinesEnabled(enabled: Boolean) {
        context.dataStore.edit { it[scanlinesKey] = enabled }
    }

    suspend fun setTheme(theme: String) {
        context.dataStore.edit { it[themeKey] = theme }
    }

    suspend fun setBorderStyle(style: String) {
        context.dataStore.edit { it[borderStyleKey] = style.takeIf { value -> value in BorderStyles.options } ?: BorderStyles.OFF }
    }

    suspend fun setNativeEmulationEnabled(enabled: Boolean) {
        context.dataStore.edit { it[nativeEmulationEnabledKey] = enabled }
    }

    suspend fun setTouchControlsEnabled(enabled: Boolean) {
        context.dataStore.edit { it[touchControlsEnabledKey] = enabled }
    }

    suspend fun setTouchControlsOpacity(opacity: Float) {
        context.dataStore.edit { it[touchControlsOpacityKey] = opacity.coerceIn(0.35f, 1f) }
    }

    suspend fun setControllerLayout(layout: String) {
        context.dataStore.edit { it[controllerLayoutKey] = layout }
    }

    suspend fun setVideoScale(scale: Int) {
        context.dataStore.edit { it[videoScaleKey] = scale.coerceIn(0, 4) }
    }

    suspend fun setRetroArchEnabled(enabled: Boolean) {
        context.dataStore.edit { it[retroArchEnabledKey] = enabled }
    }

    suspend fun setRetroArchPackageName(packageName: String) {
        context.dataStore.edit { it[retroArchPackageKey] = packageName.trim() }
    }

    suspend fun setRetroArchCoresFolder(uri: String) {
        context.dataStore.edit { it[retroArchCoresFolderKey] = uri }
    }

    suspend fun setBiosFolder(uri: String) {
        context.dataStore.edit { it[biosFolderKey] = uri }
    }

    suspend fun setRetroArchCore(systemId: String, coreNameOrPath: String) {
        context.dataStore.edit { it[retroArchCoreKey(systemId)] = coreNameOrPath.trim() }
    }

    suspend fun setSystemHidden(systemId: String, hidden: Boolean) {
        context.dataStore.edit { preferences ->
            val current = parseHiddenSystemIds(preferences[hiddenSystemIdsKey].orEmpty()).toMutableSet()
            if (hidden) current += systemId else current -= systemId
            preferences[hiddenSystemIdsKey] = current.sorted().joinToString(",")
        }
    }

    suspend fun currentSettings(): UserSettings = settings.first()

    private fun retroArchCoreKey(systemId: String) = stringPreferencesKey("retroarch_core_$systemId")

    private fun parseHiddenSystemIds(value: String): Set<String> {
        return value.split(',')
            .map { it.trim() }
            .filter { it.isNotBlank() }
            .toSet()
    }

    private fun defaultCoreFor(systemId: String): String {
        return when (systemId) {
            "nes" -> "fceumm_libretro_android.so"
            "snes" -> "snes9x_libretro_android.so"
            "genesis" -> "genesis_plus_gx_libretro_android.so"
            "playstation" -> "pcsx_rearmed_libretro_android.so"
            "psp" -> "ppsspp_libretro_android.so"
            "dreamcast" -> "flycast_libretro_android.so"
            "arcade" -> "fbneo_libretro_android.so"
            "gb" -> "gambatte_libretro_android.so"
            "gba" -> "mgba_libretro_android.so"
            "n64" -> "mupen64plus_next_gles3_libretro_android.so"
            "ds" -> "melondsds_libretro_android.so"
            "android" -> ""
            else -> ""
        }
    }
}
