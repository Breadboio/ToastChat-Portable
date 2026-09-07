import java.util.Properties

plugins { id("com.android.application") }

val keystoreProps = Properties().apply {
    val f = rootProject.file("keystore.properties")
    if (f.exists()) f.inputStream().use { load(it) }
}

android {
    namespace = "com.breadtoasting.toastchat"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.breadtoasting.toastchat.portable"
        // NativeActivity's onInputEvent/looper contract used here is stable
        // well below this, but 24 is the floor for a comfortable NDK toolchain.
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "0.7.0"

        ndk {
            // 32-bit ARM is still worth carrying; x86_64 is what an emulator runs.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=none",          // pure C, no libc++ payload
                    "-DTC_HOST=${project.findProperty("tcHost") ?: "breadtoasting.com"}",
                    "-DTC_PORT=${project.findProperty("tcPort") ?: "443"}",
                    "-DTC_TLS=${project.findProperty("tcTls") ?: "ON"}",
                    "-DTC_AUTONAME=${project.findProperty("tcAutoname") ?: "OFF"}"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    signingConfigs {
        if (keystoreProps.containsKey("storeFile")) {
            create("release") {
                storeFile = rootProject.file(keystoreProps["storeFile"] as String)
                storePassword = keystoreProps["storePassword"] as String
                keyAlias = keystoreProps["keyAlias"] as String
                keyPassword = keystoreProps["keyPassword"] as String
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (keystoreProps.containsKey("storeFile")) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
