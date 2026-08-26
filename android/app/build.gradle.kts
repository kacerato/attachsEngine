plugins {
    id("com.android.application")
}

android {
    namespace = "dev.aether.editor"
    compileSdk = 35
    ndkVersion = "27.1.12297006"

    defaultConfig {
        applicationId = "dev.aether.editor"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20")
                targets += listOf("aether_android", "aether_transform")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../native/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Requerido pelo AGP em conjunto com extractNativeLibs="true" no manifesto (ver comentário
    // lá) — sem isso o empacotamento "comprimido" (padrão do AGP moderno) ainda impede extração
    // real em disco mesmo com a flag do manifesto ligada.
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = false
            ndk {
                debugSymbolLevel = "SYMBOL_TABLE"
            }
        }
    }

    lint {
        // O editor é intencionalmente landscape e o primeiro alvo nativo é
        // ARM64. targetSdk 35 acompanha o máximo oficialmente suportado pelo
        // AGP 8.7 usado neste build reproduzível.
        disable += setOf("ChromeOsAbiSupport", "OldTargetApi")
    }
}
