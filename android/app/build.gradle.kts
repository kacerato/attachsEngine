import java.security.MessageDigest
import groovy.json.JsonSlurper

plugins {
    id("com.android.application")
}

// The packaged engine must come from this checkout, never a stale checked-in DLL.
// Keep the vendored BCL unchanged; publish only our framework-dependent component.
val managedOutput = layout.buildDirectory.dir("managed/rendering")
val generatedAssets = layout.buildDirectory.dir("generated/aetherAssets")
val verifyPackagedOpenSsl by tasks.registering {
    val libraryRoot = file("../../native/third_party/openssl/arm64-v8a")
    inputs.dir(libraryRoot)
    doLast {
        val hashes = libraryRoot.resolve("SHA256SUMS")
        check(hashes.isFile) { "OpenSSL ausente; execute tools/build-android-openssl.sh" }
        val expected = hashes.readLines().filter { it.isNotBlank() }.associate { line ->
            val parts = line.trim().split(Regex("\\s+"), limit = 2)
            check(parts.size == 2) { "SHA256SUMS OpenSSL invalido" }
            parts[1].removePrefix("*") to parts[0]
        }
        check(expected.keys == setOf("libcrypto.so.astra.so", "libssl.so.astra.so")) {
            "Bibliotecas OpenSSL incompletas"
        }
        expected.forEach { (name, hash) ->
            val library = libraryRoot.resolve(name)
            check(library.isFile) { "OpenSSL ausente: $name" }
            val actual = MessageDigest.getInstance("SHA-256").digest(library.readBytes())
                .joinToString("") { "%02x".format(it) }
            check(actual == hash) { "Hash OpenSSL divergente: $name" }
        }
    }
}
val publishManagedCore by tasks.registering(Exec::class) {
    inputs.files(fileTree("../../managed/Aether.Core") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Scene") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Rendering") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Aether.Analyzers") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files(fileTree("../../managed/Astra.Scripting") { include("**/*.cs", "**/*.csproj"); exclude("bin/**", "obj/**") })
    inputs.files("../../Directory.Build.props", "../../NuGet.Config")
    outputs.dir(managedOutput)
    workingDir = rootProject.projectDir.parentFile
    commandLine("dotnet", "publish", "managed/Aether.Rendering/Aether.Rendering.csproj", "-c", "Release",
        "-r", "linux-bionic-arm64", "--self-contained", "false",
        "-p:GenerateRuntimeConfigurationFiles=true", "-o", managedOutput.get().asFile.absolutePath)
}
// Legacy packages are only for explicit regression/migration builds.
val includeLegacyDemos = providers.gradleProperty("astra.includeLegacyDemos").map {
    require(it == "true" || it == "false") { "astra.includeLegacyDemos must be true or false" }
    it.toBoolean()
}.getOrElse(false)
val prepareEngineAssets by tasks.registering(Sync::class) {
    inputs.property("includeLegacyDemos", includeLegacyDemos)
    dependsOn(publishManagedCore)
    dependsOn(verifyPackagedOpenSsl)
    from("../../native/third_party/openssl/LICENSE.txt") { into("licenses/openssl") }
    if (includeLegacyDemos) {
    inputs.file("../../samples/material-preview/manifest.json")
    inputs.file("../../samples/dirt-road/manifest.json")
    inputs.file("../../samples/ocean/manifest.json")
    }
    from("src/main/assets") {
        exclude("dotnet/Aether.*", "dotnet_manifest.txt", "dotnet_build_id.txt")
    }
    from(managedOutput) { include("*.dll", "*.deps.json", "*.runtimeconfig.json"); into("dotnet") }
    if (includeLegacyDemos) {
    from("../../samples/material-preview/Imported") { include("*.aetex"); into("material_preview") }
    from("../../samples/dirt-road/Imported") { include("*.aetex", "*.aemap", "*.aeenv"); into("dirt_road") }
    from("../../samples/dirt-road") { include("manifest.json", "LICENSE.txt"); into("dirt_road") }
    from("../../samples/ocean/Imported") { include("*.aetex", "*.aemap", "*.aeenv"); into("ocean") }
    from("../../samples/ocean") { include("manifest.json", "LICENSE.txt"); into("ocean") }
    }
    // Atlas da interface do editor. Os dois sao lidos uma vez na inicializacao
    // e enviados a GPU; noCompress abaixo permite le-los sem descompactar.
    from("../../assets/astra-visual/ui") { include("*.aeuf", "*.aeui"); into("ui") }
    into(generatedAssets)
    doLast {
        val root = generatedAssets.get().asFile
        if (includeLegacyDemos) {
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
        }
        if (!includeLegacyDemos) {
            listOf("ocean", "dirt_road", "material_preview").forEach { name ->
                check(!root.resolve(name).exists()) { "Legacy demo leaked into standard assets: $name" }
            }
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
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    ndkVersion = "27.1.12297006"
    androidResources { noCompress += setOf("aetex", "aemap", "aeenv", "aeuf", "aeui") }
    sourceSets.getByName("main").assets.setSrcDirs(listOf(generatedAssets))
    sourceSets.getByName("main").jniLibs.srcDir("../../native/third_party/openssl")
    // The regression laboratory is not part of the production editor.
    sourceSets.getByName("main").java.srcDir(if (includeLegacyDemos) "src/legacy/java" else "src/editor/java")
    if (includeLegacyDemos) {
        sourceSets.getByName("debug").manifest.srcFile("src/legacy/AndroidManifest.xml")
        sourceSets.getByName("release").manifest.srcFile("src/legacy/AndroidManifest.xml")
    }
    buildFeatures {
        // AGDK Frame Pacing is consumed as a native Prefab package by CMake.
        prefab = true
        buildConfig = true
    }

    defaultConfig {
        applicationId = "dev.aether.editor"
        buildConfigField("boolean", "INCLUDE_LEGACY_DEMOS", includeLegacyDemos.toString())
        minSdk = 26
        targetSdk = 35
        // Versão 2 foi o experimento Godot. Atualização preserva dados sem downgrade.
        versionCode = 4
        versionName = "0.2.0-editor-ui"

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
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}
