import java.security.MessageDigest
import groovy.json.JsonSlurper

plugins {
    id("com.android.application")
}

// The packaged engine must come from this checkout, never a stale checked-in DLL.
// Keep the vendored BCL unchanged; publish only our framework-dependent component.
val managedOutput = layout.buildDirectory.dir("managed/rendering")
val generatedAssets = layout.buildDirectory.dir("generated/aetherAssets")
val publishManagedCore by tasks.registering(Exec::class) {
    inputs.files(fileTree("../../managed/Aether.Core") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Scene") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Rendering") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Analyzers") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files("../../Directory.Build.props", "../../NuGet.Config")
    outputs.dir(managedOutput)
    workingDir = rootProject.projectDir.parentFile
    commandLine("dotnet", "publish", "managed/Aether.Rendering/Aether.Rendering.csproj", "-c", "Release",
        "-r", "linux-bionic-arm64", "--self-contained", "false",
        "-p:GenerateRuntimeConfigurationFiles=true", "-o", managedOutput.get().asFile.absolutePath)
}
val prepareEngineAssets by tasks.registering(Sync::class) {
    dependsOn(publishManagedCore)
    inputs.file("../../samples/material-preview/manifest.json")
    inputs.file("../../samples/dirt-road/manifest.json")
    inputs.file("../../samples/ocean/manifest.json")
    from("src/main/assets") {
        exclude("dotnet/Aether.*", "dotnet_manifest.txt", "dotnet_build_id.txt")
    }
    from(managedOutput) { include("Aether.Core.dll", "Aether.Scene.dll", "Aether.Rendering.dll", "Aether.Rendering.deps.json", "Aether.Rendering.runtimeconfig.json"); into("dotnet") }
    from("../../samples/material-preview/Imported") { include("*.aetex"); into("material_preview") }
    from("../../samples/dirt-road/Imported") { include("*.aetex", "*.aemap", "*.aeenv"); into("dirt_road") }
    from("../../samples/dirt-road") { include("manifest.json", "LICENSE.txt"); into("dirt_road") }
    from("../../samples/ocean/Imported") { include("*.aetex", "*.aemap", "*.aeenv"); into("ocean") }
    from("../../samples/ocean") { include("manifest.json", "LICENSE.txt"); into("ocean") }
    into(generatedAssets)
    doLast {
        val root = generatedAssets.get().asFile
        val materialManifest = JsonSlurper().parse(file("../../samples/material-preview/manifest.json")) as Map<*, *>
        check(materialManifest["version"] == 1) { "Unsupported material preview manifest version" }
        val materialOutputs = materialManifest["outputs"] as Map<*, *>
        val requiredMaps = setOf("albedo.aetex", "normal.aetex", "arm.aetex", "albedo-fallback.aetex",
            "normal-fallback.aetex", "arm-fallback.aetex", "studio.aetex", "brdf.aetex")
        check(materialOutputs.keys == requiredMaps) { "Incomplete material preview manifest" }
        materialOutputs.forEach { (name, expectedHash) ->
            val asset = root.resolve("material_preview/$name")
            check(asset.isFile) { "Missing cooked material asset: $name; see samples/material-preview/README.md" }
            val assetDigest = MessageDigest.getInstance("SHA-256")
            asset.inputStream().use { stream ->
                val buffer = ByteArray(65536)
                var count = stream.read(buffer)
                while (count >= 0) { assetDigest.update(buffer, 0, count); count = stream.read(buffer) }
            }
            val actualHash = assetDigest.digest().joinToString("") { "%02x".format(it) }
            check(actualHash == expectedHash) { "Cooked material asset checksum mismatch: $name" }
        }
        val mapManifest = JsonSlurper().parse(file("../../samples/dirt-road/manifest.json")) as Map<*, *>
        val mapVersion = (mapManifest["version"] as? Number)?.toInt()
        val mapFormat = mapManifest["format"] as? String
        check((mapVersion == 1 && mapFormat == "AEMAP-1") ||
              (mapVersion == 2 && mapFormat == "AEMAP-2") ||
              (mapVersion == 3 && mapFormat == "AEMAP-3")) {
            "Unsupported dirt road package manifest"
        }
        val mapOutputs = mapManifest["outputs"] as Map<*, *>
        check(mapOutputs.isNotEmpty()) { "Empty dirt road package" }
        mapOutputs.forEach { (name, expectedHash) ->
            val asset = root.resolve("dirt_road/$name")
            check(asset.isFile) { "Missing cooked map asset: $name; see samples/dirt-road/README.md" }
            val digest = MessageDigest.getInstance("SHA-256")
            asset.inputStream().use { stream ->
                val buffer = ByteArray(65536)
                var count = stream.read(buffer)
                while (count >= 0) { digest.update(buffer, 0, count); count = stream.read(buffer) }
            }
            val actualHash = digest.digest().joinToString("") { "%02x".format(it) }
            check(actualHash == expectedHash) { "Cooked map asset checksum mismatch: $name" }
        }
        val oceanManifest = JsonSlurper().parse(file("../../samples/ocean/manifest.json")) as Map<*, *>
        check((oceanManifest["version"] as? Number)?.toInt() == 1 &&
              oceanManifest["format"] == "AEMAP-3") { "Unsupported ocean package manifest" }
        val oceanOutputs = oceanManifest["outputs"] as Map<*, *>
        check(oceanOutputs.isNotEmpty()) { "Empty ocean package" }
        oceanOutputs.forEach { (name, expectedHash) ->
            val asset = root.resolve("ocean/$name")
            check(asset.isFile) { "Missing cooked ocean asset: $name; run tools/build-ocean-demo.py" }
            val digest = MessageDigest.getInstance("SHA-256")
            asset.inputStream().use { stream ->
                val buffer = ByteArray(65536)
                var count = stream.read(buffer)
                while (count >= 0) { digest.update(buffer, 0, count); count = stream.read(buffer) }
            }
            val actualHash = digest.digest().joinToString("") { "%02x".format(it) }
            check(actualHash == expectedHash) { "Cooked ocean asset checksum mismatch: $name" }
        }
        val dotnet = root.resolve("dotnet")
        val paths = dotnet.walkTopDown().filter { it.isFile }.map { it.relativeTo(dotnet).invariantSeparatorsPath }.sorted().toList()
        root.resolve("dotnet_manifest.txt").writeText(paths.joinToString("\n", postfix = "\n"))
        val digest = MessageDigest.getInstance("SHA-256")
        paths.forEach { path ->
            digest.update(path.toByteArray(Charsets.UTF_8))
            digest.update(0.toByte())
            dotnet.resolve(path).inputStream().use { stream ->
                val buffer = ByteArray(65536)
                var count = stream.read(buffer)
                while (count >= 0) { digest.update(buffer, 0, count); count = stream.read(buffer) }
            }
        }
        root.resolve("dotnet_build_id.txt").writeText(digest.digest().joinToString("") { "%02x".format(it) })
    }
}
tasks.named("preBuild") { dependsOn(prepareEngineAssets) }

android {
    namespace = "dev.aether.editor"
    compileSdk = 35
    ndkVersion = "27.1.12297006"
    androidResources { noCompress += setOf("aetex", "aemap", "aeenv") }
    sourceSets.getByName("main").assets.setSrcDirs(listOf(generatedAssets))
    buildFeatures {
        // AGDK Frame Pacing is consumed as a native Prefab package by CMake.
        prefab = true
    }

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
                arguments += listOf("-DANDROID_STL=c++_shared")
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

    // Medir em debug e comparar com um orçamento medido em release compara duas
    // coisas diferentes. Para que a rodada release exista, ela precisa de uma
    // assinatura; sem esta configuração `assembleRelease` produz um APK
    // unsigned que o aparelho recusa instalar.
    //
    // O padrão aponta para o keystore de depuração porque ele já existe em
    // qualquer máquina com o SDK, o que torna a medição reproduzível sem
    // segredo compartilhado. Isto NÃO é uma configuração de publicação: para
    // distribuir, defina ASTRA_KEYSTORE/ASTRA_KEYSTORE_PASSWORD/
    // ASTRA_KEY_ALIAS/ASTRA_KEY_PASSWORD e o bloco passa a usá-los.
    signingConfigs {
        create("measurement") {
            val explicitStore = System.getenv("ASTRA_KEYSTORE")
            if (explicitStore != null) {
                storeFile = file(explicitStore)
                storePassword = System.getenv("ASTRA_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("ASTRA_KEY_ALIAS")
                keyPassword = System.getenv("ASTRA_KEY_PASSWORD")
            } else {
                storeFile = File(System.getProperty("user.home"), ".android/debug.keystore")
                storePassword = "android"
                keyAlias = "androiddebugkey"
                keyPassword = "android"
            }
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("measurement")
            ndk {
                debugSymbolLevel = "SYMBOL_TABLE"
            }
        }
    }

    // Item 2.1.6 do plano: libVkLayer_khronos_validation.so só entra no APK
    // debug — o código nativo (native/rhi/device.cpp) também só tenta
    // habilitá-la fora de NDEBUG, mas mantê-la fora do sourceSet release é a
    // defesa primária (não faz sentido inflar ~27 MB num APK de distribuição
    // por uma ferramenta de diagnóstico que nunca roda ali).
    sourceSets.getByName("debug").jniLibs.srcDir("../../native/third_party/vulkan-validation-layers")

    lint {
        // O editor é intencionalmente landscape e o primeiro alvo nativo é
        // ARM64. targetSdk 35 acompanha o máximo oficialmente suportado pelo
        // AGP 8.7 usado neste build reproduzível.
        disable += setOf("ChromeOsAbiSupport", "OldTargetApi")
    }
}

dependencies {
    implementation("androidx.games:games-frame-pacing:2.1.3")
}
