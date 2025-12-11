package com.example.polaris

import android.app.Application
import androidx.room.Room

class PolarisApp : Application() {
    lateinit var database: AppDatabase
        private set

    override fun onCreate() {
        super.onCreate()
        database = Room.databaseBuilder(
            applicationContext,
            AppDatabase::class.java,
            "signal-db"
        ).fallbackToDestructiveMigration().build()
    }
}