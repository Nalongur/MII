plugins {
    kotlin("jvm") version "2.1.10"
    application
    id("com.google.protobuf") version "0.9.4"
}

dependencies {
    implementation("com.google.protobuf:protobuf-java:3.24.4")
}

val stageCanonicalProto by tasks.registering(Sync::class) {
    from(rootProject.file("../../mii.proto"))
    into(layout.buildDirectory.dir("canonical-proto"))
}

sourceSets {
    main {
        proto {
            srcDir(layout.buildDirectory.dir("canonical-proto"))
        }
    }
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:3.24.4"
    }
}

tasks.matching { it.name == "generateProto" }.configureEach {
    dependsOn(stageCanonicalProto)
}

tasks.named("processResources") {
    dependsOn(stageCanonicalProto)
}

kotlin {
    jvmToolchain(21)
}

application {
    mainClass.set("com.mii.protocol.ProtocolVectorCheckKt")
}

tasks.named<JavaExec>("run") {
    args(rootProject.file("../../test-vectors/device_status_request_v1.json"))
    args(rootProject.file("../../test-vectors/authentication_v1.json"))
}

tasks.named("check") {
    dependsOn("run")
}
