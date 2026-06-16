plugins {
    id("com.android.application")
}

android {
    namespace = "com.batchloader.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.batchloader.app"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    
    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.webkit:webkit:1.8.0")
    implementation("com.google.android.material:material:1.11.0")
    implementation("org.nanohttpd:nanohttpd:2.3.1")
}
