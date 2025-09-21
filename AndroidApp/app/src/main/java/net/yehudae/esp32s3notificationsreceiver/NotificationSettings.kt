package net.yehudae.esp32s3notificationsreceiver

import android.content.Context
import android.content.SharedPreferences
import android.util.Log

class NotificationSettings(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("notification_settings", Context.MODE_PRIVATE)
    
    companion object {
        private const val TAG = "NotificationSettings"
        private const val KEY_ENABLED_APPS = "enabled_apps"
        private const val KEY_BLOCKED_APPS = "blocked_apps"
        private const val KEY_PRIORITY_APPS = "priority_apps"
        private const val KEY_FILTER_KEYWORDS = "filter_keywords"
        private const val KEY_QUIET_HOURS_ENABLED = "quiet_hours_enabled"
        private const val KEY_QUIET_HOURS_START = "quiet_hours_start"
        private const val KEY_QUIET_HOURS_END = "quiet_hours_end"
        private const val KEY_MAX_NOTIFICATIONS = "max_notifications"
        
        // Default blocked apps
        private val DEFAULT_BLOCKED_APPS = setOf(
            "android",
            "com.android.systemui", 
            "com.android.settings",
            "net.yehudae.esp32s3notificationsreceiver"
        )
    }
    
    fun isAppEnabled(packageName: String): Boolean {
        if (DEFAULT_BLOCKED_APPS.contains(packageName)) return false
        
        val blockedApps = getBlockedApps()
        if (blockedApps.contains(packageName)) return false
        
        val enabledApps = getEnabledApps()
        return enabledApps.isEmpty() || enabledApps.contains(packageName)
    }
    
    fun getEnabledApps(): Set<String> {
        return prefs.getStringSet(KEY_ENABLED_APPS, emptySet()) ?: emptySet()
    }
    
    fun setEnabledApps(apps: Set<String>) {
        prefs.edit().putStringSet(KEY_ENABLED_APPS, apps).apply()
        Log.d(TAG, "Enabled apps updated: ${apps.size} apps")
    }
    
    fun getBlockedApps(): Set<String> {
        return prefs.getStringSet(KEY_BLOCKED_APPS, DEFAULT_BLOCKED_APPS) ?: DEFAULT_BLOCKED_APPS
    }
    
    fun setBlockedApps(apps: Set<String>) {
        prefs.edit().putStringSet(KEY_BLOCKED_APPS, apps + DEFAULT_BLOCKED_APPS).apply()
        Log.d(TAG, "Blocked apps updated: ${apps.size} apps")
    }
    
    fun getPriorityApps(): Set<String> {
        return prefs.getStringSet(KEY_PRIORITY_APPS, emptySet()) ?: emptySet()
    }
    
    fun setPriorityApps(apps: Set<String>) {
        prefs.edit().putStringSet(KEY_PRIORITY_APPS, apps).apply()
        Log.d(TAG, "Priority apps updated: ${apps.size} apps")
    }
    
    fun toggleAppEnabled(packageName: String) {
        val blockedApps = getBlockedApps().toMutableSet()
        if (blockedApps.contains(packageName)) {
            blockedApps.remove(packageName)
            setBlockedApps(blockedApps)
        } else {
            blockedApps.add(packageName)
            setBlockedApps(blockedApps)
        }
    }
    
    fun toggleAppPriority(packageName: String) {
        val priorityApps = getPriorityApps().toMutableSet()
        if (priorityApps.contains(packageName)) {
            priorityApps.remove(packageName)
        } else {
            priorityApps.add(packageName)
        }
        setPriorityApps(priorityApps)
    }
    
    fun isQuietHoursEnabled(): Boolean {
        return prefs.getBoolean(KEY_QUIET_HOURS_ENABLED, false)
    }
    
    fun setQuietHoursEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_QUIET_HOURS_ENABLED, enabled).apply()
    }
    
    fun getQuietHoursStart(): Int {
        return prefs.getInt(KEY_QUIET_HOURS_START, 22) // 10 PM default
    }
    
    fun setQuietHoursStart(hour: Int) {
        prefs.edit().putInt(KEY_QUIET_HOURS_START, hour).apply()
    }
    
    fun getQuietHoursEnd(): Int {
        return prefs.getInt(KEY_QUIET_HOURS_END, 7) // 7 AM default
    }
    
    fun setQuietHoursEnd(hour: Int) {
        prefs.edit().putInt(KEY_QUIET_HOURS_END, hour).apply()
    }
    
    fun getMaxNotifications(): Int {
        return prefs.getInt(KEY_MAX_NOTIFICATIONS, 100)
    }
    
    fun setMaxNotifications(max: Int) {
        prefs.edit().putInt(KEY_MAX_NOTIFICATIONS, max).apply()
    }
    
    fun isInQuietHours(): Boolean {
        if (!isQuietHoursEnabled()) return false
        
        val currentHour = java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY)
        val startHour = getQuietHoursStart()
        val endHour = getQuietHoursEnd()
        
        return if (startHour <= endHour) {
            currentHour in startHour..endHour
        } else {
            currentHour >= startHour || currentHour <= endHour
        }
    }
}
