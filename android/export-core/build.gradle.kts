plugins { `java-library` }
java { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
dependencies {
    implementation("io.github.reandroid:ARSCLib:1.4.0")
    api("com.android.tools.build:apksig:8.7.3")
    implementation("org.bouncycastle:bcpkix-jdk18on:1.86")
    testImplementation("junit:junit:4.13.2")
}
tasks.register<JavaExec>("exportGame") {
    classpath = sourceSets.main.get().runtimeClasspath
    mainClass.set("dev.aether.export.GameExportCli")
    if (providers.gradleProperty("astraExportConfig").isPresent)
        args("--config", providers.gradleProperty("astraExportConfig").get())
}
