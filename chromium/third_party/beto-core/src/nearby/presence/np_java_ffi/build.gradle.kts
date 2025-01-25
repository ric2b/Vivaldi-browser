/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import net.ltgt.gradle.errorprone.errorprone;

plugins {
  `java-library`
  id("net.ltgt.errorprone") version "4.0.0"
}

java {
  // Gradle JUnit test finder doesn't support Java 21 class files.
  sourceCompatibility = JavaVersion.VERSION_1_9
  targetCompatibility = JavaVersion.VERSION_1_9
}

repositories {
  mavenCentral()
  google()
}

dependencies {
  implementation("androidx.annotation:annotation:1.6.0")
  implementation("com.google.errorprone:error_prone_core:2.28.0")
  implementation("org.checkerframework:checker-qual:3.45.0")

  // JUnit Test Support
  testImplementation("junit:junit:4.13")
  testImplementation("com.google.truth:truth:1.1.4")
  testImplementation("com.google.code.gson:gson:2.10.1")
  testImplementation("org.mockito:mockito-core:5.+")
}

// Flattened directory layout
sourceSets {
  main {
    java {
      setSrcDirs(listOf("java"))
    }
  }
  test {
    java {
      setSrcDirs(listOf("test"))
    }
  }
}

tasks.test {
  useJUnit()
  jvmArgs = mutableListOf(
      // libnp_java_ffi.so
      "-Djava.library.path=$projectDir/../../target/debug",
      // ByteBuddy agent for mocks
      "-XX:+EnableDynamicAgentLoading"
  )
}

tasks.withType<JavaCompile>().configureEach {
  options.errorprone {
    error("CheckReturnValue")
    error("UnnecessaryStaticImport")
    error("WildcardImport")
    error("RemoveUnusedImports")
    error("ReturnMissingNullable")
    error("FieldMissingNullable")
    error("AnnotationPosition")
    error("CheckedExceptionNotThrown")
    error("NonFinalStaticField")
    error("InvalidLink")
  }
}
