import com.google.protobuf.gradle.*
import groovy.json.JsonSlurper
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.protobuf)
}

val versions = JsonSlurper().parse(rootProject.file("../versions.json")) as Map<*, *>
val stageCanonicalProto by tasks.registering(Sync::class) {
    from(rootProject.file("../protocol/mii.proto"))
    into(layout.buildDirectory.dir("canonical-proto"))
}
val stageAuthConformance by tasks.registering(Sync::class) {
    from(rootProject.file("../protocol/host-tests/kotlin/src/main/kotlin")) {
        include("**/AuthVectorCheck.kt")
    }
    into(layout.buildDirectory.dir("generated/auth-conformance"))
}
val stageProtocolFixtures by tasks.registering(Sync::class) {
    from(rootProject.file("../protocol/test-vectors")) { include("*.json") }
    into(layout.buildDirectory.dir("protocol-fixtures"))
}

android {
    namespace = "com.mii.app"
    compileSdk = 36
    buildToolsVersion = "36.0.0"
    defaultConfig {
        applicationId = "com.mii.app"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = versions["androidAppVersion"] as String
    }
    buildTypes {
        debug { applicationIdSuffix = ".debug" }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures { compose = true; buildConfig = true }
    sourceSets {
        getByName("main").proto {
            srcDir(stageCanonicalProto.map { it.destinationDir })
        }
        getByName("test") {
            java.srcDir(stageAuthConformance.map { it.destinationDir })
            resources.srcDir(stageProtocolFixtures.map { it.destinationDir })
        }
    }
    lint { abortOnError = true; warningsAsErrors = true }
}
kotlin { compilerOptions { jvmTarget.set(JvmTarget.JVM_17) } }
protobuf {
    protoc { artifact = "com.google.protobuf:protoc:${libs.versions.protobuf.get()}" }
    generateProtoTasks {
        all().configureEach {
            dependsOn(stageCanonicalProto)
            builtins { maybeCreate("java").option("lite") }
        }
    }
}
tasks.matching { it.name.endsWith("UnitTestKotlin") }.configureEach { dependsOn(stageAuthConformance) }
tasks.matching { it.name.endsWith("UnitTestJavaRes") }.configureEach { dependsOn(stageProtocolFixtures) }
tasks.matching { it.name.endsWith("UnitTestLintModel") }.configureEach {
    dependsOn(stageAuthConformance, stageProtocolFixtures)
}
dependencies {
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.preview)
    implementation(libs.compose.material3)
    implementation(libs.activity.compose)
    implementation(libs.protobuf.lite)
    testImplementation(libs.junit)
}
